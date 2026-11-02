// Checks the basketball's paths (src/client/features/basketball/BallPath.cpp): a basket goes through the rim, every path
// is smooth and ends still on the floor, rebounds stay on the court, and the chances read right.
// g++ -std=c++17 -I../../src/client path_test.cpp ../../src/client/features/basketball/BallPath.cpp && ./a.out
#include <cmath>
#include <cstdio>

#include "features/basketball/BallPath.h"

using namespace coop::basket;

int main() {
    int bad = 0;
    Court c;
    c.rim = {100.0f, 50.0f, 13.05f};
    c.fx = 0.6f, c.fy = 0.8f;
    c.floorZ = 10.0f;
    float farthest = 0.0f, longest = 0.0f;
    for (uint32_t seed = 1; seed <= 3000; ++seed) {
        float d = 1.0f + (seed % 90) * 0.1f, side = ((int)((seed * 37) % 100) - 50) * 0.02f;
        Shot s;
        s.from = {c.rim.x + c.fx * d - c.fy * side, c.rim.y + c.fy * d + c.fx * side, c.floorZ + 2.75f};
        s.made = seed % 3 == 0;
        s.dunk = seed % 17 == 0;
        s.seed = seed * 2654435761u;
        if (s.dunk) s.from = c.rim, s.made = true;
        BallPath p(c, s);
        V3 at = p.At(p.RimTime());
        float off = std::sqrt((at.x - c.rim.x) * (at.x - c.rim.x) + (at.y - c.rim.y) * (at.y - c.rim.y) + (at.z - c.rim.z) * (at.z - c.rim.z));
        if (s.made && off > 0.05f) printf("seed %u: the basket passes %.2f m from the rim\n", seed, off), bad++;
        if (!s.made && (off < 0.2f || off > 0.4f)) printf("seed %u: the miss hits %.2f m from the rim's middle\n", seed, off), bad++;
        V3 last = p.At(0.0f);
        float worst = 0.0f;
        for (float t = 1.0f / 60; t < p.RestTime() + 0.5f; t += 1.0f / 60) {
            V3 n = p.At(t);
            float step = std::sqrt((n.x - last.x) * (n.x - last.x) + (n.y - last.y) * (n.y - last.y) + (n.z - last.z) * (n.z - last.z));
            worst = std::fmax(worst, step);
            if (n.z < c.floorZ + kBallRadius - 0.01f) { printf("seed %u: under the floor at %.2f s\n", seed, t), bad++; break; }
            last = n;
        }
        if (worst > 0.45f) printf("seed %u: a jump of %.2f m in a frame\n", seed, worst), bad++;
        V3 r = p.At(p.RestTime() + 1.0f);
        if (std::fabs(r.z - (c.floorZ + kBallRadius)) > 0.01f) printf("seed %u: rests at %.2f over the floor\n", seed, r.z - c.floorZ), bad++;
        float back = (r.x - c.rim.x) * c.fx + (r.y - c.rim.y) * c.fy;
        if (back < -0.5f) printf("seed %u: rests %.2f m behind the backboard\n", seed, -back), bad++;
        farthest = std::fmax(farthest, Flat(r, c.rim));
        longest = std::fmax(longest, p.RestTime());
    }
    printf("rebounds rest up to %.1f m from the rim, still after %.1f s at most\n", farthest, longest);
    printf("chance: 2 m perfect %.2f good %.2f bad %.2f; 5 m good %.2f; 7 m perfect %.2f good %.2f; never released %.2f\n",
           Chance(2, 0.01f), Chance(2, 0.05f), Chance(2, 0.2f), Chance(5, 0.05f), Chance(7, 0.0f), Chance(7, 0.05f), Chance(3, 1.0f));
    if (Points(5.9f) != 2 || Points(6.1f) != 3) printf("points wrong\n"), bad++;
    printf(bad ? "%d FAILED\n" : "all good\n", bad);
    return bad != 0;
}
