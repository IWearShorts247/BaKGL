#include "bak/combat/spellFormula.hpp"

#include "bak/spells.hpp"

#include <algorithm>

namespace BAK::Combat {

int SpellDamage(const Spell& spell, unsigned power)
{
    // STRUCTURE from SpellCalculationType (the enum names the relationship).
    switch (spell.mCalculationType)
    {
    case SpellCalculationType::CostTimesDamage:
        return std::max(0, static_cast<int>(power) * spell.mDamage);
    case SpellCalculationType::FixedAmount:
    case SpellCalculationType::NonCostRelated:
    default:
        return std::max(0, spell.mDamage);
    }
}

bool IsOffensiveSpell(const Spell& spell)
{
    // A damage-dealing spell. (Buffs/utility have mDamage == 0; the 6 StaticSpells go
    // through the existing CastStaticSpell path instead.)
    return spell.mDamage > 0;
}

}
