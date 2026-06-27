#include "bak/combat/combatFormula.hpp"

#include "bak/random.hpp"

#include <algorithm>

namespace BAK::Combat {

namespace {
// <<< RE-tunable constants — placeholders pending KRONDOR.EXE capture >>>
// Chosen to give playable (non-degenerate) combat; NOT the original numbers.
// NOTE: only the to-hit CURVE below is still a placeholder. The damage BASE term
// was recovered exactly from live capture — see MeleeDamage.
constexpr int sHitFloorPercent = 5;     // even a poor attacker sometimes connects
constexpr int sHitCeilPercent  = 95;    // even a great attacker sometimes misses
}

int MeleeHitChancePercent(const AttackerProfile& a, const DefenderProfile& d)
{
    // STRUCTURE (faithful): accuracy = weaponAccuracy + Melee, contested vs Defense.
    const int accuracy = std::max(0, a.accuracyMelee + a.weaponAccuracy);
    const int defense  = std::max(0, d.defense);
    // CURVE (placeholder): contested ratio of accuracy to total, clamped.
    const int denom = std::max(1, accuracy + defense);
    const int chance = (accuracy * 100) / denom;
    return std::clamp(chance, sHitFloorPercent, sHitCeilPercent);
}

int MeleeDamage(const AttackerProfile& a, const DefenderProfile& d)
{
    // BASE damage — RECOVERED EXACTLY from live KRONDOR.EXE capture (2026-06-22):
    //   base = effectiveStrength + weaponStrengthScalar   (NO divisor)
    // where weaponStrengthScalar is the chosen attack's mStrength{Thrust,Swing}
    // (already selected into a.weaponDamage by GatherCombatProfiles). This is the
    // pre-armour "Damage" the original game shows in its combat targeting box, and
    // it matched all 9 captured data points across two battles / three characters /
    // four weapons / both attack types exactly. (Matches the manual + inventory UI:
    // "Base Dmg X + Strength".) Full derivation: _remaster/combat_capture_results.md.
    const int base = a.strength + a.weaponDamage;
    // ARMOUR reduction — STILL a placeholder (the post-armour value [+0x44] / HP-drop
    // was not captured this session). armourModifier is 0 today, so this is a no-op;
    // the curve here will be replaced once armour reduction is captured.
    const int reduction = (base * std::clamp(d.armourModifier, 0, 90)) / 100;
    return std::max(1, base - reduction);
}

bool RollSucceeds(int chancePercent)
{
    // CONFIRMED from KRONDOR.EXE — identical to the engine's GetRandomMod100().
    return static_cast<int>((GetRandom() & 0xfff) % 100) < chancePercent;
}

}
