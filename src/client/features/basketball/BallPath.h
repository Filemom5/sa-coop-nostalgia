#pragma once
// The basketball's flight and bounces (Basketball.cpp): pure functions, no game in them. Every game makes the same path
// from a shot's record (where the ball left the hands, made or not, a seed), so all of them show the same shot and the
// same rebound without sending the ball's place. Tested on the PC that builds the mod (tools/basketball).
#include <cstdint>
#include <vector>

namespace coop::basket {

struct V3 {
    float x = 0, y = 0, z = 0;
};

struct Court {
    V3 rim;                 // the rim's centre
    float fx = 0, fy = 1;   // flat and unit: from the backboard into the court
    float floorZ = 0;       // the court's ground
};

constexpr float kRimRadius = 0.23f;  // a rim's 45 cm
constexpr float kBallRadius = 0.12f;
constexpr float kThreeArc = 6.0f;    // farther than this from the rim (flat), a basket is worth 3
constexpr float kPeak = 0.4f;        // the jump shot's top: a part of BBALL_JUMP_SHOT (the game's own minigame's)
constexpr float kGravity = 9.81f;

float Flat(const V3& a, const V3& b);  // distance in the plane
int Points(float dist);                // 2 or 3, by the distance from the rim
// how good a release was: 2 perfect, 1 good, 0 bad. error: off the jump's top, in parts of the jump (1: never released)
int Timing(float error);
// the chance a jump shot goes in from dist (metres from the rim, flat) released `error` off the top
float Chance(float dist, float error);

struct Shot {
    V3 from;            // where the ball left the hands
    bool made = false;
    bool dunk = false;  // from the rim itself, down through the net
    uint32_t seed = 0;
};

class BallPath {
public:
    BallPath() = default;
    BallPath(const Court& court, const Shot& shot);
    V3 At(float t) const;                      // the ball's centre t seconds after the shot
    float RimTime() const { return m_rim; }    // it reaches the rim: a basket counts then
    float RestTime() const { return m_rest; }  // it lies still from then on
    float Height(float t) const;               // its bottom over the floor

private:
    // p0 + v0 * s + (0, 0, -g s^2 / 2) for s = t - t0; a roll (g < 0) slows on the flat by -g instead
    struct Seg {
        float t0, t1;
        V3 p0, v0;
        float g;
    };
    std::vector<Seg> m_segs;
    V3 m_restAt;
    float m_rim = 0, m_rest = 0, m_floor = 0;
    void Fall(float t0, V3 p, V3 v, uint32_t& rng);  // bounces till it lies still
};

}  // namespace coop::basket
