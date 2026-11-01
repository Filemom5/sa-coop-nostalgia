// The radio of a car shared by the players in it: the same station and the same song at the same moment.
//
// The station: one that any of them picks (the mouse wheel, the radio keys) plays for everyone in that car, as the game
// tunes for a script's SET_RADIO_CHANNEL (CAERadioTrackManager::RetuneRadio): the station's name shows and the dial
// takes its usual second and a half.
//
// The song: each game picks its own (the station's tracks, DJ talk, adverts and idents queue up at random), so the car's
// leader (the driver; with no player at the wheel, the lowest player id in it) tells what its radio plays: the five
// tracks of its queue and how far into the first one it is. A game that plays something else, or the same more than a
// moment apart, starts the leader's queue at the leader's place (as StartRadio would with its own pick); one in step only
// takes the leader's next track, so that both go on to the same one (the stream's next track, as the game's own
// CheckForTrackConcatenation gives it). User tracks are each player's own files: only their station is shared.
//
// G_RADIO, reliable: u8 op, u16 car (net id), i8 station.
//  * OP_TUNE: the sender turned the dial to that station just now; everyone in the car tunes to it.
//  * OP_SYNC: the car's station as its leader has it, then u8 1 and its song while it plays one (i32 x 5 queue, i8 x 5
//    track types, i8 x 5 track indices, u8 track flags, i32 ms into the first track, i32 its length), else u8 0. Sent on
//    someone getting in, on each new track and every 3 s while players share the car.
#include "features/radio/Radio.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <set>

#include "common/api/protocol/Protocol.h"
#include "common/lib/buffer/Buffer.h"
#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "entities/player/Players.h"
#include "entities/vehicle/Vehicles.h"
#include "shared/api/net-session/Session.h"
#include "shared/game/Game.h"
#include "shared/lib/core/Core.h"

namespace coop::radio {

namespace {
using namespace game;

enum : uint8_t { OP_TUNE = 1, OP_SYNC = 2 };

// CAERadioTrackManager AERadioTrackManager (0x8CB6F8) and its fields
constexpr uintptr_t kRadio = 0x8CB6F8;
constexpr int kInitialised = 0x00;         // a start is pending: the next stop plays m_RequestedSettings
constexpr int kMode = 0x68;                // eRadioTrackMode (i32)
constexpr int kStationsListed = 0x6C;      // the dial's steps not tuned to yet
constexpr int kStationsListDown = 0x70;
constexpr int kScriptRequest = 0x7C;       // RetuneRadio's station, taken by the next CheckForStationRetune (-1 none)
constexpr int kRequested = 0x88;           // tRadioSettings m_RequestedSettings: what is about to play
constexpr int kActive = 0xC4;              // tRadioSettings m_ActiveSettings: what plays
// tRadioSettings
constexpr int kQueue = 0x00;               // i32 x 5: track ids, the first one plays
constexpr int kCurrTrack = 0x14;           // the stream's playing track (CAEAudioHardware::GetPlayingTrackID)
constexpr int kPlayTime = 0x1C;            // ms into it (negative: a state of the stream); requested: where to start
constexpr int kLength = 0x20;              // its length, ms
constexpr int kFlags = 0x24;
constexpr int kStation = 0x25;
constexpr int kTypes = 0x2C;               // i8 x 5: eTrackType of the queue
constexpr int kIndices = 0x33;             // i8 x 5: the music history's indices of the queue
constexpr int kQueueLen = 5;

enum : int32_t { MODE_STARTING = 0, MODE_WAITING_TO_PLAY = 1, MODE_PLAYING = 2, MODE_STOPPING = 3 };
constexpr int8_t kRadioEmergency = 0, kRadioUserTracks = 12, kRadioOff = 13, kRadioCount = 14;
constexpr int8_t kTrackUser = 7;           // eTrackType::USER_TRACK
constexpr uintptr_t kAudioHardware = 0xB5F8B8;

constexpr uint64_t kSyncEveryMs = 3000;
constexpr uint64_t kTuneSettleMs = 3500;   // a dial turned this recently wins over the leader's word
constexpr uint64_t kOccupantFreshMs = 3000;
constexpr uint64_t kSongKeepMs = 4000;     // a song heard from the leader is still worth acting on this long
constexpr uint64_t kForceGapMs = 6000;     // after starting the leader's song, give it time before judging again
constexpr int32_t kDriftMs = 800;          // further apart than this, the leader's place is taken again
constexpr int32_t kEdgeMs = 2000;          // around a change of track, one game may be a moment ahead
constexpr int32_t kStartLagMs = 150;       // a stream takes about this long from the start to the first sound

uint8_t* Manager() { return (uint8_t*)kRadio; }
uint8_t* Active() { return Manager() + kActive; }
uint8_t* Requested() { return Manager() + kRequested; }
int32_t& QueueAt(uint8_t* settings, int i) { return Field<int32_t>(settings, kQueue + 4 * i); }

// the station the dial points at, folded as CAERadioTrackManager::CheckForStationRetune folds it (8-bit sum)
int8_t Fold(int listed) {
    int8_t s = (int8_t)(uint8_t)((uint8_t)Field<int8_t>(Requested(), kStation) + (uint8_t)listed);
    if (s > kRadioEmergency) {
        if (s >= kRadioCount) s = (int8_t)(s - kRadioCount + 1);
    } else {
        s = (int8_t)(s + kRadioCount - 1);
    }
    return s;
}

// what the dial shows now (a turn still going on included)
int8_t Target() {
    int32_t request = Field<int32_t>(Manager(), kScriptRequest);
    if (request >= 0) return (int8_t)request;
    return Fold(Field<int32_t>(Manager(), kStationsListed));
}

// the station, once nothing is being tuned
bool Settled(int8_t& station) {
    if (Field<int32_t>(Manager(), kStationsListed) || Field<int32_t>(Manager(), kStationsListDown) ||
        Field<int32_t>(Manager(), kScriptRequest) >= 0)
        return false;
    station = Field<int8_t>(Requested(), kStation);
    if (station < 0 || station > kRadioOff) station = kRadioOff;  // a radio switched off keeps no station
    return true;
}

// tVehicleAudioSettings of our player's vehicle, if it has a radio of its own: not a police scanner
// (AE_RT_EMERGENCY), not a bicycle (AE_RT_DISABLED)
uint8_t* CivilianRadio() {
    auto* settings = Call<0x4F4ED0, uint8_t*>();  // CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio
    return settings && (int8_t)settings[0x1B] == 0 ? settings : nullptr;  // RadioType: AE_RT_CIVILIAN
}

// our player's car as both games know it (0: on foot, or a vehicle of this game only)
uint16_t CurrentCar(CVehicle** out = nullptr) {
    CPlayerPed* me = FindPlayerPed(0);
    CVehicle* v = me ? me->VehicleIfIn() : nullptr;
    NetVehicle* nv = v ? Vehicles::Instance().Find(v) : nullptr;
    if (out) *out = v;
    return nv ? nv->netId : 0;
}

// the other players in that car, by what their games last said
std::set<uint8_t> Others(uint16_t car, uint8_t* driver = nullptr) {
    std::set<uint8_t> ids;
    uint64_t now = NowMs();
    for (const auto& [id, rp] : Players::Instance().All()) {
        if (!rp.hasState || now - rp.stateRecvMs > kOccupantFreshMs || rp.mode == RemotePlayer::Mode::OnFoot ||
            rp.vehNetId != car)
            continue;
        ids.insert(id);
        if (driver && rp.mode == RemotePlayer::Mode::Driver) *driver = id;
    }
    return ids;
}

struct Song {
    int32_t queue[kQueueLen];
    int8_t types[kQueueLen];
    int8_t indices[kQueueLen];
    uint8_t flags;
    int32_t pos;     // ms into queue[0]
    int32_t length;  // of queue[0], ms
};

// what our radio plays now, if a station's song is going on (not the user tracks: each player's own files)
bool PlayingSong(int8_t station, Song& s) {
    uint8_t* a = Active();
    if (station <= kRadioEmergency || station >= kRadioUserTracks || Field<int32_t>(Manager(), kMode) != MODE_PLAYING ||
        Field<int8_t>(a, kStation) != station || Field<int32_t>(a, kPlayTime) < 0 || QueueAt(a, 0) < 0 ||
        Field<int32_t>(a, kCurrTrack) != QueueAt(a, 0))
        return false;
    for (int i = 0; i < kQueueLen; ++i) {
        s.queue[i] = QueueAt(a, i);
        s.types[i] = Field<int8_t>(a, kTypes + i);
        s.indices[i] = Field<int8_t>(a, kIndices + i);
    }
    s.flags = Field<uint8_t>(a, kFlags);
    s.pos = Field<int32_t>(a, kPlayTime);
    s.length = Field<int32_t>(a, kLength);
    return true;
}

uint16_t g_car = 0;              // the car we were in at the last frame
std::set<uint8_t> g_others;      // the other players in it then
bool g_leader = false;           // our game speaks for the car's radio (at the last frame)
bool g_syncDue = false;          // someone got in: the leader tells the station as soon as it is settled
uint64_t g_syncMs = 0;
int32_t g_sentTrack = -1;        // leader: the first track of the queue it last told
uint64_t g_tuneMs = 0;           // the dial was last turned (here or by someone in the car) then
int8_t g_pendingTune = -1;       // our player's turn of the dial, to send after the radio's update
// the leader's song as last heard, until it is acted on
bool g_songPending = false;
Song g_song;
int8_t g_songStation = -1;
uint64_t g_songRecvMs = 0;
uint64_t g_forceMs = 0;          // we last started the leader's song then

void SendTune(uint16_t car, int8_t station) {
    Writer w(proto::G_RADIO);
    w.U8(OP_TUNE);
    w.U16(car);
    w.I8(station);
    Net().Send(proto::CH_RELIABLE, w);
}

void SendSync(uint16_t car, int8_t station, const Song* song) {
    Writer w(proto::G_RADIO);
    w.U8(OP_SYNC);
    w.U16(car);
    w.I8(station);
    w.U8(song ? 1 : 0);
    if (song) {
        for (int32_t t : song->queue) w.I32(t);
        for (int8_t t : song->types) w.I8(t);
        for (int8_t i : song->indices) w.I8(i);
        w.U8(song->flags);
        w.I32(song->pos);
        w.I32(song->length);
    }
    Net().Send(proto::CH_RELIABLE, w);
}

void Reset() {
    g_car = 0;
    g_others.clear();
    g_leader = g_syncDue = false;
    g_syncMs = g_tuneMs = g_forceMs = 0;
    g_sentTrack = -1;
    g_pendingTune = -1;
    g_songPending = false;
}

// the leader's place, now: what it said plus the time since and the way here
int32_t LeaderPlace(uint64_t now) {
    int32_t way = (int32_t)std::min<uint32_t>(Net().Rtt(), 500);
    return g_song.pos + (int32_t)(now - g_songRecvMs) + way;
}

// the leader's next track becomes ours too (ours plays the same track as the leader's, in step)
void TakeNext() {
    uint8_t* a = Active();
    if (g_song.queue[1] < 0 || QueueAt(a, 1) == g_song.queue[1]) return;
    for (int i = 1; i < kQueueLen; ++i) {
        QueueAt(a, i) = g_song.queue[i];
        Field<int8_t>(a, kTypes + i) = g_song.types[i];
        Field<int8_t>(a, kIndices + i) = g_song.indices[i];
    }
    // the stream keeps playing its track and only takes the next one, as after the game's own change of track
    // (CheckForTrackConcatenation; CAEStreamThread::Service: the playing track is the one asked for)
    CallMethod<0x4D8F10>((void*)kAudioHardware, (uint32_t)QueueAt(a, 0), (int32_t)QueueAt(a, 1), (uint32_t)0,
                         (uint8_t)Field<uint8_t>(a, kFlags), Field<int8_t>(a, kTypes) == kTrackUser,
                         Field<int8_t>(a, kTypes + 1) == kTrackUser);  // CAEAudioHardware::PlayTrack
    LOGI("Radio: next track %d as the leader's", g_song.queue[1]);
}

// our radio starts the leader's queue at its place, as StartRadio starts a station
void StartLeadersSong(int32_t at, const uint8_t* vehicleSettings) {
    uint8_t* m = Manager();
    uint8_t* rs = Requested();
    Field<int32_t>(m, kStationsListed) = 0;
    Field<int32_t>(m, kStationsListDown) = 0;
    Field<int32_t>(m, kScriptRequest) = -1;
    for (int i = 0; i < kQueueLen; ++i) {
        QueueAt(rs, i) = g_song.queue[i];
        Field<int8_t>(rs, kTypes + i) = g_song.types[i];
        Field<int8_t>(rs, kIndices + i) = g_song.indices[i];
    }
    Field<uint8_t>(rs, kFlags) = g_song.flags;
    Field<int8_t>(rs, kStation) = g_songStation;
    Field<int8_t>(rs, 0x26) = (int8_t)vehicleSettings[6];             // BassSetting (tVehicleAudioSettings::BassSetting)
    Field<float>(rs, 0x28) = Field<float>(vehicleSettings, 8);       // BassGain (::BassFactor)
    Field<int32_t>(rs, kPlayTime) = std::max(at, 0);
    if (Field<int32_t>(m, kMode) <= MODE_PLAYING) Field<int32_t>(m, kMode) = MODE_STOPPING;
    Field<uint8_t>(m, kInitialised) = 1;
}

// acts on the leader's song: true when done with it (in step, started, or no longer of use), false to look again
bool FollowSong(uint64_t now) {
    if (!g_songPending || now - g_songRecvMs > kSongKeepMs) return true;
    uint8_t* vs = CivilianRadio();
    if (g_leader || !g_car || !vs) return true;
    int8_t mine = -1;
    if (!Settled(mine)) return false;  // our dial still turns
    if (mine != g_songStation && now - g_tuneMs < kTuneSettleMs) return true;  // the leader may not have our pick yet
    int32_t place = LeaderPlace(now);
    if (g_song.length > 0 && place >= g_song.length) return true;  // that song is over there: its next one comes soon
    uint8_t* a = Active();
    if (mine == g_songStation && Field<int32_t>(Manager(), kMode) == MODE_PLAYING) {
        int32_t first = QueueAt(a, 0), pos = Field<int32_t>(a, kPlayTime), length = Field<int32_t>(a, kLength);
        bool playing = pos >= 0 && Field<int32_t>(a, kCurrTrack) == first;
        if (playing && first == g_song.queue[0] && std::abs(pos - place) <= kDriftMs) {
            TakeNext();
            return true;
        }
        // a moment ahead (already in the leader's next track) or behind (the leader is in our next one)
        if (playing && first == g_song.queue[1] && pos < kEdgeMs && g_song.length - place < kEdgeMs) return true;
        if (playing && QueueAt(a, 1) == g_song.queue[0] && length - pos < kEdgeMs && place < kEdgeMs) return true;
    }
    if (now - g_forceMs < kForceGapMs) return false;  // the last start may still be on its way
    StartLeadersSong(place + kStartLagMs, vs);
    g_forceMs = now;
    LOGI("Radio: playing the leader's song (station %d, track %d at %d ms)", g_songStation, g_song.queue[0],
         place + kStartLagMs);
    return true;
}

void Handle(uint8_t from, Reader& r) {
    uint8_t op = r.U8();
    uint16_t car = r.U16();
    int8_t station = r.I8();
    Song song{};
    bool hasSong = false;
    if (op == OP_SYNC) {
        hasSong = r.U8() != 0;
        if (hasSong) {
            for (int32_t& t : song.queue) t = r.I32();
            for (int8_t& t : song.types) t = r.I8();
            for (int8_t& i : song.indices) i = r.I8();
            song.flags = r.U8();
            song.pos = r.I32();
            song.length = r.I32();
        }
    }
    if (!r.Ok() || from == Net().MyId() || station <= kRadioEmergency || station > kRadioOff) return;
    if (!core::IsPlayerReady() || !car || car != CurrentCar() || !CivilianRadio()) return;
    uint64_t now = NowMs();
    if (op == OP_TUNE) {
        g_tuneMs = now;
        g_songPending = false;  // a song of the station before
        if (Target() == station) return;
        LOGI("Radio: %s tuned the car to station %d", Net().PeerName(from).c_str(), station);
        CallMethod<0x4E8290>(Manager(), station);  // CAERadioTrackManager::RetuneRadio
        return;
    }
    if (op != OP_SYNC || g_leader) return;  // two leaders for a moment (a driver changing): ours keeps its word
    if (hasSong && station < kRadioUserTracks && song.queue[0] >= 0 && song.pos >= 0) {
        g_song = song;
        g_songStation = station;
        g_songRecvMs = now;
        g_songPending = true;
        if (FollowSong(now)) g_songPending = false;
        return;
    }
    // no song to follow (user tracks, the radio off): the station alone
    int8_t mine = -1;
    if (now - g_tuneMs < kTuneSettleMs || !Settled(mine) || mine == station) return;
    LOGI("Radio: the car's station is %d (%s), here it was %d", station, Net().PeerName(from).c_str(), mine);
    CallMethod<0x4E8290>(Manager(), station);  // CAERadioTrackManager::RetuneRadio
}

class Radio : public Module {
public:
    const char* Name() const override { return "Radio"; }
    void OnInstallHooks() override { Net().SetHandler(proto::G_RADIO, &Handle); }
    void OnSessionEnd() override { Reset(); }
    void OnGameShutdown() override { Reset(); }

    void OnPostProcess() override {
        if (!Net().Online() || !core::IsPlayerReady()) {
            Reset();
            return;
        }
        CVehicle* v = nullptr;
        uint16_t car = CurrentCar(&v);
        if (car != g_car) {
            g_car = car;
            g_others.clear();
            g_syncDue = false;
            g_pendingTune = -1;
            g_sentTrack = -1;
            g_songPending = false;
        }
        g_leader = false;
        if (!car || !CivilianRadio()) {
            g_pendingTune = -1;
            g_songPending = false;
            return;
        }
        uint8_t driver = 0xFF;
        std::set<uint8_t> others = Others(car, &driver);
        for (uint8_t id : others)
            if (!g_others.count(id)) g_syncDue = true;  // someone got in
        g_others = others;
        if (others.empty()) {
            g_pendingTune = -1;
            g_syncDue = false;
            g_songPending = false;
            return;
        }
        uint64_t now = NowMs();
        if (g_pendingTune >= 0) {
            SendTune(car, g_pendingTune);
            LOGI("Radio: we tuned the car to station %d", g_pendingTune);
            g_pendingTune = -1;
        }
        // the leader: our player at the wheel, a player who drives, or the lowest id of the players in it
        CPlayerPed* me = FindPlayerPed(0);
        uint8_t leader = Net().MyId();
        if (v->Driver() != me) {
            if (driver != 0xFF) leader = driver;
            else if (*others.begin() < leader) leader = *others.begin();
        }
        g_leader = leader == Net().MyId();
        if (!g_leader) {
            if (g_songPending && FollowSong(now)) g_songPending = false;
            return;
        }
        g_songPending = false;
        int8_t station = -1;
        if (!Settled(station) || station == kRadioEmergency) return;
        Song song{};
        bool playing = PlayingSong(station, song);
        bool newTrack = playing && song.queue[0] != g_sentTrack;
        if (!playing && now - g_tuneMs < kTuneSettleMs) return;  // the station alone: let a turn of the dial settle
        if (!g_syncDue && !newTrack && now - g_syncMs < kSyncEveryMs) return;
        SendSync(car, station, playing ? &song : nullptr);
        g_syncDue = false;
        g_syncMs = now;
        g_sentTrack = playing ? song.queue[0] : -1;
    }

    static Radio& Instance() {
        static Radio m;
        return m;
    }
};

struct Reg {
    Reg() {
        core::RegisterModule(&Radio::Instance());
        Players::s_onStationKey = &StationKey;
    }
} g_reg;
}  // namespace

void StationKey(int delta) {
    if (!Net().Online() || !g_car) return;
    // the game adds the step to the dial right after the key: where it is going
    g_pendingTune = Fold(Field<int32_t>(Manager(), kStationsListed) + delta);
    g_tuneMs = NowMs();
    g_songPending = false;
}

}  // namespace coop::radio
