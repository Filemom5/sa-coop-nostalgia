#include "features/basketball/BallPath.h"

#include <algorithm>
#include <cmath>

namespace coop::basket {

namespace {

constexpr float kPi = 3.14159265f;

// xorshift32: the same numbers from the same seed in every game
float Rand(uint32_t& s) {
    if (!s) s = 0x9E3779B9u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return (s >> 8) * (1.0f / 16777216.0f);
}

// the court's forward (fx, fy) turned by deg degrees
void Turn(const Court& c, float deg, float& x, float& y) {
    float a = deg * kPi / 180.0f, cs = std::cos(a), sn = std::sin(a);
    x = c.fx * cs - c.fy * sn;
    y = c.fx * sn + c.fy * cs;
}

float Base(float d) {
    if (d <= 2.5f) return 0.75f;
    if (d <= kThreeArc) return 0.75f - (d - 2.5f) / (kThreeArc - 2.5f) * 0.20f;
    return std::max(0.05f, 0.40f - (d - kThreeArc) * 0.04f);
}

}  // namespace

float Flat(const V3& a, const V3& b) { return std::hypot(a.x - b.x, a.y - b.y); }

int Points(float dist) { return dist > kThreeArc ? 3 : 2; }

int Timing(float error) { return error <= 0.03f ? 2 : error <= 0.07f ? 1 : 0; }

float Chance(float dist, float error) {
    int t = Timing(error);
    float c = Base(dist) + (t == 2 ? 0.20f : t == 1 ? 0.0f : error >= 1.0f ? -0.45f : -0.35f);
    return std::clamp(c, 0.02f, 0.97f);
}

BallPath::BallPath(const Court& c, const Shot& s) {
    m_floor = c.floorZ;
    uint32_t rng = s.seed;
    if (s.dunk) {
        // pushed down through the net from the rim, slowed by it
        V3 p = c.rim;
        V3 v{c.fx * 0.3f, c.fy * 0.3f, -2.2f};
        m_rim = 0.0f;
        m_segs.push_back({0.0f, 0.3f, p, v, 3.0f});
        V3 e{p.x + v.x * 0.3f, p.y + v.y * 0.3f, p.z + v.z * 0.3f - 1.5f * 0.09f};
        Fall(0.3f, e, {c.fx * 0.5f, c.fy * 0.5f, -2.0f}, rng);
        return;
    }
    // to the rim: its middle (in), else a point of its edge (out: mostly the front, then the back, then a side)
    V3 to = c.rim;
    float side = 0.0f;
    if (!s.made) {
        float r = Rand(rng), deg;
        if (r < 0.5f) deg = (Rand(rng) - 0.5f) * 140.0f;                  // the front: toward the court
        else if (r < 0.8f) deg = 180.0f + (Rand(rng) - 0.5f) * 100.0f;    // the back
        else deg = (Rand(rng) < 0.5f ? 90.0f : -90.0f) + (Rand(rng) - 0.5f) * 30.0f;
        float dx, dy;
        Turn(c, deg, dx, dy);
        float reach = kRimRadius + kBallRadius * 0.5f;
        to = {c.rim.x + dx * reach, c.rim.y + dy * reach, c.rim.z + 0.04f};
        side = deg;
    }
    float dist = Flat(s.from, to);
    float top = std::max(s.from.z, to.z) + std::min(0.7f + 0.12f * dist, 3.0f);
    float vz = std::sqrt(2.0f * kGravity * std::max(top - s.from.z, 0.05f));
    float flight = vz / kGravity + std::sqrt(2.0f * std::max(top - to.z, 0.05f) / kGravity);
    V3 v{(to.x - s.from.x) / flight, (to.y - s.from.y) / flight, vz};
    m_segs.push_back({0.0f, flight, s.from, v, kGravity});
    m_rim = flight;
    if (s.made) {
        // down through the net, slowed by it, then out under the rim
        V3 nv{c.fx * 0.25f + v.x * 0.1f, c.fy * 0.25f + v.y * 0.1f, -1.8f};
        m_segs.push_back({flight, flight + 0.32f, to, nv, 2.0f});
        V3 e{to.x + nv.x * 0.32f, to.y + nv.y * 0.32f, to.z + nv.z * 0.32f - 1.0f * 0.32f * 0.32f};
        float sway = (Rand(rng) - 0.5f) * 0.6f;
        Fall(flight + 0.32f, e, {c.fx * 0.6f - c.fy * sway, c.fy * 0.6f + c.fx * sway, -2.5f}, rng);
        return;
    }
    // off the rim: back into the court, the front's toward the shooter, the back's farther
    bool back = std::fabs(std::remainder(side - 180.0f, 360.0f)) < 60.0f;
    float deg = (Rand(rng) - 0.5f) * 160.0f, dx, dy;
    Turn(c, deg, dx, dy);
    float speed = (back ? 2.5f : 1.5f) + 2.0f * Rand(rng);
    Fall(flight, to, {dx * speed, dy * speed, 1.8f + 2.2f * Rand(rng)}, rng);
}

void BallPath::Throw(float t0, const V3& from, const V3& to, float secs) {
    secs = std::max(secs, 0.05f);
    V3 v{(to.x - from.x) / secs, (to.y - from.y) / secs, (to.z - from.z) / secs + kGravity * secs * 0.5f};
    m_segs.push_back({t0, t0 + secs, from, v, kGravity});
}

void BallPath::Stop(float t, const V3& at) {
    m_restAt = at;
    m_rest = t;
}

BallPath BallPath::Toss(const V3& from, const V3& to, float secs, float floorZ, bool fall, uint32_t seed) {
    BallPath p;
    p.m_floor = floorZ;
    p.Throw(0.0f, from, to, secs);
    p.m_rim = secs;
    if (!fall) {
        p.Stop(secs, to);
        return p;
    }
    // on as it came, a little slower: the vertical speed it arrives with
    float dx = (to.x - from.x) / secs * 0.6f, dy = (to.y - from.y) / secs * 0.6f;
    float vz = (to.z - from.z) / secs - kGravity * secs * 0.5f;
    p.Fall(secs, to, {dx, dy, vz}, seed);
    return p;
}

BallPath BallPath::Bank(const V3& from, const V3& via, const V3& to, float secs1, float secs2, float floorZ, bool fall,
                        uint32_t seed) {
    BallPath p;
    p.m_floor = floorZ;
    p.Throw(0.0f, from, via, secs1);
    p.Throw(secs1, via, to, secs2);
    float t = secs1 + secs2;
    p.m_rim = t;
    if (!fall) {
        p.Stop(t, to);
        return p;
    }
    float dx = (to.x - via.x) / secs2 * 0.6f, dy = (to.y - via.y) / secs2 * 0.6f;
    float vz = (to.z - via.z) / secs2 - kGravity * secs2 * 0.5f;
    p.Fall(t, to, {dx, dy, vz}, seed);
    return p;
}

BallPath BallPath::Knock(const Court& c, const V3& from, uint32_t seed) {
    BallPath p;
    p.m_floor = c.floorZ;
    uint32_t rng = seed;
    float deg = (Rand(rng) - 0.5f) * 140.0f, dx, dy;
    Turn(c, deg, dx, dy);
    float speed = 3.5f + 2.5f * Rand(rng);
    p.m_rim = 0.0f;
    p.Fall(0.0f, from, {dx * speed, dy * speed, 1.0f + 1.5f * Rand(rng)}, rng);
    return p;
}

void BallPath::Fall(float t0, V3 p, V3 v, uint32_t& rng) {
    float rest = m_floor + kBallRadius;
    for (int bounce = 0; bounce < 8; ++bounce) {
        float drop = std::max(p.z - rest, 0.0f);
        float t = (v.z + std::sqrt(v.z * v.z + 2.0f * kGravity * drop)) / kGravity;
        m_segs.push_back({t0, t0 + t, p, v, kGravity});
        p = {p.x + v.x * t, p.y + v.y * t, rest};
        float down = v.z - kGravity * t;
        t0 += t;
        v = {v.x * 0.7f, v.y * 0.7f, -down * 0.6f};
        if (v.z < 0.6f) break;
    }
    // it rolls to a stop
    float speed = std::hypot(v.x, v.y), slow = 2.5f;
    float roll = speed / slow;
    V3 flat{v.x, v.y, 0.0f};
    m_segs.push_back({t0, t0 + roll, p, flat, -slow});
    float d = speed * roll - slow * roll * roll * 0.5f;
    m_restAt = speed > 0.0f ? V3{p.x + v.x / speed * d, p.y + v.y / speed * d, rest} : p;
    m_rest = t0 + roll;
    (void)rng;
}

V3 BallPath::At(float t) const {
    if (m_segs.empty()) return m_restAt;
    if (t >= m_rest) return m_restAt;
    t = std::max(t, 0.0f);
    for (const Seg& s : m_segs) {
        if (t > s.t1) continue;
        float u = t - s.t0;
        if (s.g < 0.0f) {  // a roll: slowing on the flat
            float speed = std::hypot(s.v0.x, s.v0.y);
            if (speed <= 0.0f) return s.p0;
            float d = speed * u + s.g * u * u * 0.5f;
            return {s.p0.x + s.v0.x / speed * d, s.p0.y + s.v0.y / speed * d, s.p0.z};
        }
        return {s.p0.x + s.v0.x * u, s.p0.y + s.v0.y * u, s.p0.z + s.v0.z * u - s.g * u * u * 0.5f};
    }
    return m_restAt;
}

float BallPath::Height(float t) const { return At(t).z - m_floor - kBallRadius; }

}  // namespace coop::basket
