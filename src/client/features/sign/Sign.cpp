// NOSTALGIA in place of VINEWOOD: the sign on the hill (models VineSign1_cunte01, the letters' fronts; VineSign1_cunte,
// their backs and frames; LODVineSign1_cunte, far away) is made anew from the game's own as they load, by the recipe
// in SignData.h (SignRecipe.cpp; made by tools/sign from the game's files): N, O and I are the game's own letters, moved;
// S, T, A, L and G are ours in the same typeface, with the same textures, frames and colours of day and night. The
// collision of the two is ours too. Everyone with the mod sees it; another mod's sign is left as it is.
//
// The models and the collision are loaded in place of the game's by shared/game/ModelSwap.
#include <cstdint>
#include <vector>

#include "common/lib/log/Log.h"
#include "features/sign/SignRecipe.h"
#include "shared/game/ModelSwap.h"
#include "shared/lib/core/Core.h"

namespace coop::sign {

namespace {

std::vector<uint8_t> Model(uint32_t modelId, const uint8_t* dff, size_t size) {
    if (!IsSignModel(modelId)) return {};
    std::vector<uint8_t> ours = Compose(modelId, dff, size);
    if (!ours.empty()) LOGI("Sign: NOSTALGIA (model %u)", modelId);
    return ours;
}

class Sign : public Module {
public:
    const char* Name() const override { return "Sign"; }
    void OnInstallHooks() override {
        game::AddModelSwap(&Model);
        game::AddCollisionSwap(&Collision);
    }
    static Sign& Instance() {
        static Sign m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Sign::Instance()); }
} g_reg;
}  // namespace

}  // namespace coop::sign
