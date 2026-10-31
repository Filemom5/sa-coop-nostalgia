// The radio of a car shared by the players in it. A station any of them picks (the mouse wheel, the radio keys) plays
// for everyone in that car, and who gets in hears what is already on. The other games tune to it as the game does for a
// script's SET_RADIO_CHANNEL (CAERadioTrackManager::RetuneRadio): the station's name shows and the dial takes its usual
// second and a half. Each game still picks its own songs: the station is shared, not the song.
//
// G_RADIO, reliable: u8 op, u16 car (net id), i8 station.
//  * OP_TUNE: the sender turned the dial to that station just now; everyone in the car tunes to it.
//  * OP_SYNC: the car's station as its leader has it (the driver; with no player at the wheel, the lowest player id in
//    it), on someone getting in and every 3 s while players share the car. One that differs is tuned to, unless the
//    dial was turned in the last moments (the leader may not have heard that yet).
#include "features/radio/Radio.h"

#include <cstdint>
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
constexpr int kStationsListed = 0x6C;      // the dial's steps not tuned to yet
constexpr int kStationsListDown = 0x70;
constexpr int kScriptRequest = 0x7C;       // RetuneRadio's station, taken by the next CheckForStationRetune (-1 none)
constexpr int kRequestedStation = 0xAD;    // m_RequestedSettings.StationID: what plays or is about to
constexpr int8_t kRadioEmergency = 0, kRadioOff = 13, kRadioCount = 14;

constexpr uint64_t kSyncEveryMs = 3000;
constexpr uint64_t kTuneSettleMs = 3500;   // a dial turned this recently wins over the leader's word
constexpr uint64_t kOccupantFreshMs = 3000;

void* Manager() { return (void*)kRadio; }

// the station the dial points at, folded as CAERadioTrackManager::CheckForStationRetune folds it (8-bit sum)
int8_t Fold(int listed) {
    int8_t s = (int8_t)(uint8_t)((uint8_t)Field<int8_t>(Manager(), kRequestedStation) + (uint8_t)listed);
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
    station = Field<int8_t>(Manager(), kRequestedStation);
    if (station < 0 || station > kRadioOff) station = kRadioOff;  // a radio switched off keeps no station
    return true;
}

// our player's vehicle has a radio of its own: not a police scanner (AE_RT_EMERGENCY), not a bicycle (AE_RT_DISABLED)
bool CivilianRadio() {
    auto* settings = Call<0x4F4ED0, uint8_t*>();  // CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio
    return settings && (int8_t)settings[0x1B] == 0;  // tVehicleAudioSettings::RadioType: AE_RT_CIVILIAN
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

uint16_t g_car = 0;              // the car we were in at the last frame
std::set<uint8_t> g_others;      // the other players in it then
bool g_leader = false;           // our game speaks for the car's station (at the last frame)
bool g_syncDue = false;          // someone got in: the leader tells the station as soon as it is settled
uint64_t g_syncMs = 0;
uint64_t g_tuneMs = 0;           // the dial was last turned (here or by someone in the car) then
int8_t g_pendingTune = -1;       // our player's turn of the dial, to send after the radio's update

void Send(uint8_t op, uint16_t car, int8_t station) {
    Writer w(proto::G_RADIO);
    w.U8(op);
    w.U16(car);
    w.I8(station);
    Net().Send(proto::CH_RELIABLE, w);
}

void Reset() {
    g_car = 0;
    g_others.clear();
    g_leader = g_syncDue = false;
    g_syncMs = g_tuneMs = 0;
    g_pendingTune = -1;
}

void Handle(uint8_t from, Reader& r) {
    uint8_t op = r.U8();
    uint16_t car = r.U16();
    int8_t station = r.I8();
    if (!r.Ok() || from == Net().MyId() || station <= kRadioEmergency || station > kRadioOff) return;
    if (!core::IsPlayerReady() || !car || car != CurrentCar() || !CivilianRadio()) return;
    uint64_t now = NowMs();
    if (op == OP_TUNE) {
        g_tuneMs = now;
        if (Target() == station) return;
        LOGI("Radio: %s tuned the car to station %d", Net().PeerName(from).c_str(), station);
    } else if (op == OP_SYNC) {
        // two leaders for a moment (a driver changing): ours keeps its word and the next sync settles it
        int8_t mine = -1;
        if (g_leader || now - g_tuneMs < kTuneSettleMs || !Settled(mine) || mine == station) return;
        LOGI("Radio: the car's station is %d (%s), here it was %d", station, Net().PeerName(from).c_str(), mine);
    } else {
        return;
    }
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
        }
        g_leader = false;
        if (!car || !CivilianRadio()) {
            g_pendingTune = -1;
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
            return;
        }
        uint64_t now = NowMs();
        if (g_pendingTune >= 0) {
            Send(OP_TUNE, car, g_pendingTune);
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
        if (!g_leader || now - g_tuneMs < kTuneSettleMs) return;
        int8_t station = -1;
        if (!Settled(station) || station == kRadioEmergency) return;
        if (!g_syncDue && now - g_syncMs < kSyncEveryMs) return;
        Send(OP_SYNC, car, station);
        g_syncDue = false;
        g_syncMs = now;
    }

    static Radio& Instance() {
        static Radio m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Radio::Instance()); }
} g_reg;
}  // namespace

void StationKey(int delta) {
    if (!Net().Online() || !g_car) return;
    // the game adds the step to the dial right after the key: where it is going
    g_pendingTune = Fold(Field<int32_t>(Manager(), kStationsListed) + delta);
    g_tuneMs = NowMs();
}

}  // namespace coop::radio
