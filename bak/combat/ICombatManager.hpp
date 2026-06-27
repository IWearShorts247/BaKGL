#pragma once

#include "bak/types.hpp"
#include "bak/combat/combatFormula.hpp"

namespace BAK {

class ICombatManager
{
public:
    virtual void SetCastingSpell(SpellIndex) = 0;
    virtual void SetUsingCrossbow() = 0;

    // Melee attack type for the player's attacks (swing = more damage/less accuracy,
    // thrust = more accuracy/less damage; the difference comes from the weapon's swing
    // vs thrust scalars).
    virtual void SetMeleeAttackType(Combat::MeleeAttackType) = 0;
    virtual Combat::MeleeAttackType GetMeleeAttackType() const = 0;

    // Begin casting for the current combatant: if it's a spellcaster with an offensive
    // spell, enter targeting mode (the next enemy click resolves the spell). No-op
    // otherwise. Toggles off if already casting.
    virtual void BeginSpellCast() = 0;

    // End the current combatant's turn (a move alone no longer ends it, so the player
    // needs an explicit pass after moving without attacking).
    virtual void EndTurn() = 0;

    virtual ~ICombatManager() = default;
};

}
