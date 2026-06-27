#pragma once

#include "bak/types.hpp"
#include "bak/combat/combat.hpp"
#include "bak/combat/combatModel.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace Game::Combat {

// How a combat grid cell should be drawn this turn.
enum class GridHighlight : std::uint8_t
{
    None       = 0,  // plain grid cell
    Reachable  = 1,  // current combatant can move here
    Attackable = 2,  // current combatant can attack an opponent here
};

class ICombatStage
{
public:
    virtual void MoveCombatant(
        BAK::EntityIndex entityId,
        glm::uvec2 sourceGrid,
        glm::uvec2 targetGrid,
        std::function<void()>&& onComplete) = 0;

    virtual void SetCombatantAction(
        BAK::EntityIndex entityId,
        BAK::AnimationType animType) = 0;

    virtual void AnimateCombatant(
        BAK::EntityIndex entityId,
        std::function<void()>&& onComplete) = 0;

    // Recolour the combat grid for the current turn. Row-major, size rows*cols.
    virtual void SetGridHighlights(const std::vector<GridHighlight>& cells) = 0;

    // Request that combat end with the given result. Called from within an animation
    // callback (e.g. on the killing blow), so the implementation MUST defer the actual
    // teardown to a safe point rather than tearing down mid-callback.
    virtual void RequestCombatExit(BAK::CombatResult result) = 0;

    virtual ~ICombatStage() = default;
};

}
