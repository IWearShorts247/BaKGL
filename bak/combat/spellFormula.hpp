#pragma once

namespace BAK { class Spell; }

namespace BAK::Combat {

// ===========================================================================
//  BEST-EFFORT combat spell damage.   (full notes: _remaster/COMBAT_FORMULA.md)
//
//  STRUCTURE — taken from the spell's own SpellCalculationType, whose enum names
//  the relationship (FixedAmount, CostTimesDamage, ...). The exact constants and
//  any resist/saving-throw roll are NOT yet recovered; replace ONLY this body when
//  dynamic RE yields them. Callers (combatManager) are unaffected.
// ===========================================================================

// Damage an offensive spell deals at the given casting power (>= 0).  <<< RE-tunable >>>
int SpellDamage(const Spell& spell, unsigned power);

// True if the spell is an offensive (damage-dealing) combat spell.
bool IsOffensiveSpell(const Spell& spell);

}
