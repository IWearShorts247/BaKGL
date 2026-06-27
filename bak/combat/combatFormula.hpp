#pragma once

namespace BAK::Combat {

enum class MeleeAttackType
{
    Swing,   // higher damage, lower accuracy
    Thrust,  // lower damage, higher accuracy
};

// Inputs gathered from the attacker: effective skills (TrueSkill + Modifier) plus the
// chosen attack's weapon scalars (mStrengthSwing/mAccuracySwing or mStrengthThrust/...).
struct AttackerProfile
{
    int accuracyMelee;    // effective Melee skill
    int strength;         // effective Strength skill
    int weaponDamage;     // weapon strength-damage for the chosen attack type
    int weaponAccuracy;   // weapon accuracy modifier for the chosen attack type
};

struct DefenderProfile
{
    int defense;          // effective Defense skill
    int armourModifier;   // armour mod % reducing incoming damage (0 if unarmoured)
};

// ===========================================================================
//  BEST-EFFORT combat formulas.   (full notes: _remaster/COMBAT_FORMULA.md)
//
//  STRUCTURE — confirmed faithful:
//    * to-hit accuracy = weaponAccuracy + Melee, contested against Defense
//        (BaK's own inventory UI renders "Accuracy X + Skill")
//    * damage          = weaponBase + Strength, reduced by armour
//        (BaK's own inventory UI renders "Base Dmg X + Strength")
//    * the roll        = (GetRandom() & 0xfff) % 100 < chance
//        (read out of KRONDOR.EXE; the engine already uses this exact idiom in
//         bak/lock.cpp GetRandomMod100())
//
//  CURVE CONSTANTS — partially recovered.
//    * DAMAGE base: RECOVERED EXACTLY from live capture — base = Strength +
//      weaponStrengthScalar (no divisor). Folded into MeleeDamage below.
//    * to-hit % (accuracy vs Defense) and the armour-reduction curve: NOT yet
//      recovered — still placeholders in the function bodies / s* constants.
//  When dynamic RE yields the rest, replace ONLY those bodies/constants — every
//  caller (combatManager) is unaffected.  This is the planned fold-in point.
// ===========================================================================

// Probability in [0,100] that a melee attack lands.            <<< RE-tunable >>>
int MeleeHitChancePercent(const AttackerProfile&, const DefenderProfile&);

// Melee damage dealt on a hit, after armour (>= 1).            <<< RE-tunable >>>
int MeleeDamage(const AttackerProfile&, const DefenderProfile&);

// CONFIRMED roll mechanic (same as GetRandomMod100): true iff a fresh roll < chance.
bool RollSucceeds(int chancePercent);

}
