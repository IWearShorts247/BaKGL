#include "game/combat/combatManager.hpp"

#include "bak/combat/spellFormula.hpp"
#include "bak/spells.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <sstream>

namespace Game::Combat {

CombatManager::CombatManager(ICombatStage& stage, BAK::ICombatUI& ui)
:
    mStage{stage},
    mCombatUI{ui},
    mLogger{Logging::LogState::GetLogger("Game::CombatManager")}
{}

void CombatManager::SetCastingSpell(BAK::SpellIndex) {}

void CombatManager::SetUsingCrossbow() {}

void CombatManager::SetMeleeAttackType(BAK::Combat::MeleeAttackType type)
{
    mPlayerAttackType = type;
    mLogger.Info() << "Player melee attack type: "
        << (type == BAK::Combat::MeleeAttackType::Thrust ? "Thrust" : "Swing") << "\n";
}

BAK::Combat::MeleeAttackType CombatManager::GetMeleeAttackType() const
{
    return mPlayerAttackType;
}

void CombatManager::BeginSpellCast()
{
    // Toggle off if already targeting a spell.
    if (mCastingSpell)
    {
        mCastingSpell.reset();
        ComputeGrid();
        mLogger.Info() << "Spell cast cancelled\n";
        return;
    }

    auto& caster = GetCurrentCombatant();
    auto* casterChar = caster.mCharacter;
    if (!casterChar || casterChar->IsEnemy() || !casterChar->IsSpellcaster())
    {
        mLogger.Info() << "Current combatant cannot cast spells\n";
        return;
    }

    // Pick the caster's most-damaging known offensive spell.
    const auto& db = BAK::SpellDatabase::Get();
    std::optional<BAK::SpellIndex> best{};
    int bestDamage = 0;
    for (const auto spellIndex : casterChar->GetSpells().GetSpells())
    {
        const auto& spell = db.GetSpell(spellIndex);
        if (BAK::Combat::IsOffensiveSpell(spell) && spell.mDamage > bestDamage)
        {
            bestDamage = spell.mDamage;
            best = spellIndex;
        }
    }
    if (!best)
    {
        mLogger.Info() << casterChar->GetName() << " has no offensive combat spell\n";
        return;
    }

    mCastingSpell = best;
    mCastPower = db.GetSpell(*best).mMinCost;
    ComputeGrid(); // highlight enemies as spell targets (any range)
    mLogger.Info() << casterChar->GetName() << " casting " << db.GetSpell(*best).mName
        << " - select a target\n";
}

bool CombatManager::ResolveSpellAttack(Combatant& caster, Combatant& target)
{
    auto* casterChar = caster.mCharacter;
    auto* targetChar = target.mCharacter;
    if (!casterChar || !targetChar || !mCastingSpell) return false;

    const auto& spell = BAK::SpellDatabase::Get().GetSpell(*mCastingSpell);
    const int damage = BAK::Combat::SpellDamage(spell, mCastPower);

    // Pay the casting cost (health), matching the cast-cost pattern.
    casterChar->ImproveSkill(BAK::SkillType::TotalHealth,
        BAK::SkillChange::HealMultiplier_100, (-static_cast<int>(mCastPower)) << 8);

    // Apply damage: Stamina absorbs first, then Health.
    auto& skills = targetChar->GetSkills();
    auto& stam = skills.GetSkill(BAK::SkillType::Stamina);
    auto& hp   = skills.GetSkill(BAK::SkillType::Health);
    int remaining = damage;
    const int sLoss = std::min(remaining, static_cast<int>(stam.mTrueSkill));
    stam.mTrueSkill -= static_cast<std::uint8_t>(sLoss);
    if (stam.mCurrent > stam.mTrueSkill) stam.mCurrent = stam.mTrueSkill;
    remaining -= sLoss;
    const int hLoss = std::min(remaining, static_cast<int>(hp.mTrueSkill));
    hp.mTrueSkill -= static_cast<std::uint8_t>(hLoss);
    if (hp.mCurrent > hp.mTrueSkill) hp.mCurrent = hp.mTrueSkill;

    const bool died = hp.mTrueSkill == 0;
    mLogger.Info() << casterChar->GetName() << " casts " << spell.mName << " at "
        << targetChar->GetName() << " for " << damage << (died ? " - KILLED" : "") << "\n";
    if (died)
    {
        target.mIsDead = true;
        target.mTurnPending = false;
        target.mState = BAK::Combat::CombatantState::Dead;
    }
    return died;
}

void CombatManager::AddCombatant(Combatant combatant)
{
    mCombatants.emplace_back(combatant);
}

Combatant* CombatManager::GetCombatant(BAK::CharIndex character)
{
    auto it = std::find_if(mCombatants.begin(), mCombatants.end(),
        [&](auto& combatant){
            return (combatant.mCharacter != nullptr)
                && combatant.mCharacter->GetIndex() == character;
    });

    if (it != mCombatants.end())
    {
        return &(*it);
    }

    return nullptr;
}

Combatant* CombatManager::GetCombatant(BAK::EntityIndex entityIndex)
{
    auto it = std::find_if(mCombatants.begin(), mCombatants.end(),
        [&](auto& combatant){
            return combatant.mEntityIndex == entityIndex;
    });

    if (it != mCombatants.end())
    {
        return &(*it);
    }

    return nullptr;
}

Combatant* CombatManager::GetCombatant(glm::uvec2 gridPos)
{
    auto it = std::find_if(mCombatants.begin(), mCombatants.end(),
        [&](auto& combatant){
            return combatant.mGridPos == gridPos;
    });

    if (it != mCombatants.end())
    {
        return &(*it);
    }

    return nullptr;
}

std::vector<Combatant> CombatManager::GetCombatants()
{
    return mCombatants;
}

void CombatManager::BeginCombat()
{
    SetCurrentCombatant(true);
}

Combatant& CombatManager::GetCurrentCombatant()
{
    assert(mCurrentCombatant < mCombatants.size());
    return mCombatants[mCurrentCombatant];
}

void CombatManager::GridCellClicked(glm::uvec2 targetGrid)
{
    auto& combatant = GetCurrentCombatant();
    mLogger.Debug() << "Moving combatant: " << mCurrentCombatant
        << " " << GetCurrentCombatant().mCharacter->GetName()
        << " eid: " << combatant.mEntityIndex << "\n";

    // Spell targeting mode: a click on an enemy resolves the pending spell; anything else
    // cancels the cast.
    if (mCastingSpell)
    {
        auto* target = GetCombatant(targetGrid);
        if (target && target->mCharacter && !target->mIsDead
            && target->mCharacter->IsEnemy() != combatant.mCharacter->IsEnemy())
        {
            const bool died = ResolveSpellAttack(combatant, *target);
            mCastingSpell.reset();
            mStage.SetCombatantAction(target->mEntityIndex,
                died ? BAK::AnimationType::Dead : BAK::AnimationType::ParryHigh);
            mStage.AnimateCombatant(target->mEntityIndex, []{});
            mStage.SetCombatantAction(combatant.mEntityIndex, BAK::AnimationType::StaticCast);
            mStage.AnimateCombatant(combatant.mEntityIndex, [this]{ FinishTurn(); });
            return;
        }
        mCastingSpell.reset();
        ComputeGrid();
        mLogger.Info() << "Spell cast cancelled\n";
        return;
    }

    if (CanAttack(combatant, targetGrid))
    {
        auto* target = GetCombatant(targetGrid);
        assert(target);
        PlayMeleeAttack(combatant, *target, mPlayerAttackType);
        return; // attacking ends the turn
    }

    if (!CanMoveTo(combatant, targetGrid))
    {
        return;
    }

    auto entityIndex = combatant.mEntityIndex;
    auto sourceGrid = combatant.mGridPos;
    mStage.MoveCombatant(entityIndex, sourceGrid, targetGrid,
        [this, entityIndex, targetGrid]()
        {
            CompleteMove(entityIndex, targetGrid);
        });
}

void CombatManager::EndCombat()
{
    mCombatants.clear();
    ClearGrid();
}

void CombatManager::SetCurrentCombatant(bool onlyParty)
{
    std::optional<unsigned> bestSpeed{};
    auto combatantIndex = 0;
    for (unsigned i = 0; i < mCombatants.size(); i++)
    {
        auto& combatant = mCombatants[i];
        auto* character = combatant.mCharacter;
        if (!character)
        {
            continue;
        }

        if (combatant.mIsDead)
        {
            continue;
        }

        if (onlyParty && character->IsEnemy())
        {
            continue;
        }

        if (!combatant.mTurnPending)
        {
            continue;
        }

        auto speed = character->GetSkill(BAK::SkillType::Speed);

        if (!bestSpeed || speed > *bestSpeed)
        {
            bestSpeed = speed;
            combatantIndex = i;
        }
    }

    mCurrentCombatant = combatantIndex;
    auto& character = *GetCurrentCombatant().mCharacter;
    if (!character.IsEnemy())
    {
        mCombatUI.SetSelectedCharacter(character.GetIndex());
    }

    // Fresh turn: full movement budget, no pending spell.
    mMovementRemaining = character.GetSkill(BAK::SkillType::Speed);
    mCastingSpell.reset();

    ComputeGrid();

    mLogger.Debug() << "Current combatant set to: " << combatantIndex << " " << GetCurrentCombatant().mCharacter->GetName()
        << " charIndex: " << character.GetIndex().mValue << "\n";
}

void CombatManager::ComputeGrid()
{
    ClearGrid();

    auto& me = GetCurrentCombatant();
    const auto speed = mMovementRemaining; // movement left this turn (not full Speed)

    for (unsigned x = 0; x < mGrid.GetCols(); x++)
    {
        for (unsigned y = 0; y < mGrid.GetRows(); y++)
        {
            auto& cell = mGrid.Get(x, y);
            auto distance = ChebyshevDistance(me.mGridPos, glm::uvec2{x, y});
            if (distance <= speed)
            {
                cell.mState = SetBit(cell.mState, StateFlags::Reachable, true);
            }
        }
    }

    for (auto& combatant : mCombatants)
    {
        auto& cell = mGrid.Get(combatant.mGridPos.x, combatant.mGridPos.y);
        cell.mElement = &combatant;
        if (!combatant.mIsDead)
        {
            cell.mState = SetBit(cell.mState, StateFlags::Reachable, false);
        }

        if (me.mCharacter->IsEnemy() != combatant.mCharacter->IsEnemy())
        {
            // Spells can target any enemy; melee only adjacent ones.
            const bool inRange = mCastingSpell.has_value()
                || ChebyshevDistance(me.mGridPos, combatant.mGridPos) <= 1;
            if (!combatant.mIsDead && inRange)
            {
                cell.mState = SetBit(cell.mState, StateFlags::Attackable, true);
            }
        }
    }

    // Surface the per-cell state to the stage so it can highlight the grid this turn.
    std::vector<Game::Combat::GridHighlight> highlights(
        mGrid.GetRows() * mGrid.GetCols(), Game::Combat::GridHighlight::None);
    for (unsigned row = 0; row < mGrid.GetRows(); row++)
    {
        for (unsigned col = 0; col < mGrid.GetCols(); col++)
        {
            const auto& cell = mGrid.Get(col, row);
            const auto idx = row * mGrid.GetCols() + col;
            if (CheckBitSet(cell.mState, StateFlags::Attackable))
                highlights[idx] = Game::Combat::GridHighlight::Attackable;
            else if (CheckBitSet(cell.mState, StateFlags::Reachable))
                highlights[idx] = Game::Combat::GridHighlight::Reachable;
        }
    }
    mStage.SetGridHighlights(highlights);

    PrintGridState();
}

void CombatManager::PrintGridState()
{
    std::stringstream ss{};
    ss << std::hex;
    for (unsigned _y = mGrid.GetRows(); _y > 0; _y--)
    {
        auto y = _y - 1;
        for (unsigned x = 0; x < mGrid.GetCols(); x++)
        {
            auto& cell = mGrid.Get(x, y);
            ss << cell.mState << " ";
        }
        ss << "\n";
    }
    mLogger.Debug() << "GridState: \n" << ss.str() << "\n";
}

bool CombatManager::CanMoveTo(const Combatant& combatant, glm::uvec2 target) const
{
    if (!mGrid.WithinBounds(target.x, target.y)) return false;
    auto& cell = mGrid.Get(target.x, target.y);
    return CheckBitSet(cell.mState, StateFlags::Reachable);
}

bool CombatManager::CanAttack(const Combatant& combatant, glm::uvec2 target) const
{
    if (!mGrid.WithinBounds(target.x, target.y)) return false;
    auto& cell = mGrid.Get(target.x, target.y);
    return CheckBitSet(cell.mState, StateFlags::Attackable);
}

std::pair<BAK::Combat::AttackerProfile, BAK::Combat::DefenderProfile>
CombatManager::GatherCombatProfiles(Combatant& attacker, Combatant& defender,
    BAK::Combat::MeleeAttackType attackType) const
{
    // Effective skill = the FULL faithful calculation (Character::GetSkill ->
    // CalculateEffectiveSkillValue, SkillRead::Current): TrueSkill + Modifier, then
    // condition effects, skill affectors (spell/potion buffs), and the health-condition
    // scaling (effective = base * curHealth/maxHealth for Str/Def/Melee/Speed). Recovered
    // from KRONDOR.EXE via the Assess-skill capture (2026-06-23): a wounded combatant's
    // Defense/Strength/Melee fall with current Health. At full health this reduces to
    // TrueSkill + Modifier, matching the earlier base-damage capture. Details:
    // _remaster/combat_capture_results.md.
    const auto effective = [](BAK::Character& c, BAK::SkillType t) {
        return std::max(0, static_cast<int>(c.GetSkill(t)));
    };
    const bool thrust = attackType == BAK::Combat::MeleeAttackType::Thrust;

    auto* atkChar = attacker.mCharacter;
    BAK::Combat::AttackerProfile atk{};
    atk.accuracyMelee  = effective(*atkChar, BAK::SkillType::Melee);
    atk.strength       = effective(*atkChar, BAK::SkillType::Strength);
    atk.weaponDamage   = 0;
    atk.weaponAccuracy = 0;
    {
        auto& inv = atkChar->GetInventory();
        const auto it = inv.FindEquipped(atkChar->GetWeaponType());
        if (it != inv.GetItems().end())
        {
            // Swing vs thrust selects the weapon's matching scalars (faithful structure:
            // swing = more damage/less accuracy, thrust = the reverse).
            const auto& obj = it->GetObject();
            atk.weaponDamage   = thrust ? obj.mStrengthThrust : obj.mStrengthSwing;
            atk.weaponAccuracy = thrust ? obj.mAccuracyThrust : obj.mAccuracySwing;
        }
    }

    BAK::Combat::DefenderProfile def{};
    def.defense        = effective(*defender.mCharacter, BAK::SkillType::Defense);
    def.armourModifier = 0; // MVP placeholder: armour reduction curve pending RE
    return {atk, def};
}

BAK::Combat::MeleeAttackType CombatManager::ChooseBestAttackType(
    Combatant& attacker, Combatant& defender) const
{
    // Pick whichever attack has the higher expected damage (hit% * damage).
    const auto expected = [&](BAK::Combat::MeleeAttackType t) {
        const auto [atk, def] = GatherCombatProfiles(attacker, defender, t);
        return BAK::Combat::MeleeHitChancePercent(atk, def) * BAK::Combat::MeleeDamage(atk, def);
    };
    return expected(BAK::Combat::MeleeAttackType::Swing)
         >= expected(BAK::Combat::MeleeAttackType::Thrust)
        ? BAK::Combat::MeleeAttackType::Swing
        : BAK::Combat::MeleeAttackType::Thrust;
}

bool CombatManager::ResolveMeleeAttack(Combatant& attacker, Combatant& defender,
    BAK::Combat::MeleeAttackType attackType)
{
    auto* atkChar = attacker.mCharacter;
    auto* defChar = defender.mCharacter;
    if (!atkChar || !defChar) return false;

    const auto [atk, def] = GatherCombatProfiles(attacker, defender, attackType);
    const char* typeName = attackType == BAK::Combat::MeleeAttackType::Thrust ? "thrust" : "swing";

    const int hitChance = BAK::Combat::MeleeHitChancePercent(atk, def);
    if (!BAK::Combat::RollSucceeds(hitChance))
    {
        mLogger.Info() << atkChar->GetName() << " " << typeName << " misses "
            << defChar->GetName() << " (" << hitChance << "% to hit)\n";
        return false;
    }

    const int damage = BAK::Combat::MeleeDamage(atk, def);

    // apply damage: Stamina absorbs first, then Health (faithful structure)
    auto& skills = defChar->GetSkills();
    auto& stam = skills.GetSkill(BAK::SkillType::Stamina);
    auto& hp   = skills.GetSkill(BAK::SkillType::Health);
    int remaining = damage;
    const int sLoss = std::min(remaining, static_cast<int>(stam.mTrueSkill));
    stam.mTrueSkill -= static_cast<std::uint8_t>(sLoss);
    if (stam.mCurrent > stam.mTrueSkill) stam.mCurrent = stam.mTrueSkill;
    remaining -= sLoss;
    const int hLoss = std::min(remaining, static_cast<int>(hp.mTrueSkill));
    hp.mTrueSkill -= static_cast<std::uint8_t>(hLoss);
    if (hp.mCurrent > hp.mTrueSkill) hp.mCurrent = hp.mTrueSkill;

    const bool died = hp.mTrueSkill == 0;
    mLogger.Info() << atkChar->GetName() << " " << typeName << " hits " << defChar->GetName()
        << " for " << damage << " (" << hitChance << "% to hit)"
        << (died ? " - KILLED" : "") << "\n";
    if (died)
    {
        defender.mIsDead = true;
        defender.mTurnPending = false;
        defender.mState = BAK::Combat::CombatantState::Dead;
    }
    return died;
}

bool CombatManager::AllEnemiesDead() const
{
    for (const auto& c : mCombatants)
        if (c.mCharacter && c.mCharacter->IsEnemy() && !c.mIsDead) return false;
    return true;
}

bool CombatManager::AllPartyDead() const
{
    for (const auto& c : mCombatants)
        if (c.mCharacter && !c.mCharacter->IsEnemy() && !c.mIsDead) return false;
    return true;
}

void CombatManager::PlayMeleeAttack(Combatant& attacker, Combatant& defender,
    BAK::Combat::MeleeAttackType attackType)
{
    const bool died = ResolveMeleeAttack(attacker, defender, attackType);
    mStage.SetCombatantAction(defender.mEntityIndex,
        died ? BAK::AnimationType::Dead : BAK::AnimationType::ParryHigh);
    mStage.AnimateCombatant(defender.mEntityIndex, []{});
    mStage.SetCombatantAction(attacker.mEntityIndex,
        attackType == BAK::Combat::MeleeAttackType::Thrust
            ? BAK::AnimationType::Thrust : BAK::AnimationType::Slash);
    mStage.AnimateCombatant(attacker.mEntityIndex, [this]{ FinishTurn(); });
}

Combatant* CombatManager::NearestEnemyOf(const Combatant& me)
{
    if (!me.mCharacter) return nullptr;
    Combatant* best = nullptr;
    auto bestDist = std::numeric_limits<unsigned>::max();
    for (auto& c : mCombatants)
    {
        if (c.mCharacter && !c.mIsDead
            && c.mCharacter->IsEnemy() != me.mCharacter->IsEnemy())
        {
            const auto d = ChebyshevDistance(me.mGridPos, c.mGridPos);
            if (d < bestDist) { bestDist = d; best = &c; }
        }
    }
    return best;
}

void CombatManager::EndTurn()
{
    if (mCastingSpell) mCastingSpell.reset();
    FinishTurn();
}

void CombatManager::DoEnemyTurn()
{
    auto& enemy = GetCurrentCombatant();
    if (enemy.mIsDead || !enemy.mCharacter)
    {
        FinishTurn();
        return;
    }

    auto* target = NearestEnemyOf(enemy);
    if (!target)
    {
        FinishTurn();
        return;
    }

    // Adjacent -> attack (ends the turn).
    if (ChebyshevDistance(enemy.mGridPos, target->mGridPos) <= 1)
    {
        PlayMeleeAttack(enemy, *target, ChooseBestAttackType(enemy, *target));
        return;
    }

    // Otherwise step toward the target: the reachable cell closest to it.
    glm::uvec2 bestCell = enemy.mGridPos;
    auto bestToTarget = ChebyshevDistance(enemy.mGridPos, target->mGridPos);
    for (unsigned x = 0; x < mGrid.GetCols(); x++)
    {
        for (unsigned y = 0; y < mGrid.GetRows(); y++)
        {
            const glm::uvec2 cell{x, y};
            if (!CanMoveTo(enemy, cell)) continue;
            const auto d = ChebyshevDistance(cell, target->mGridPos);
            if (d < bestToTarget) { bestToTarget = d; bestCell = cell; }
        }
    }

    if (bestCell == enemy.mGridPos)
    {
        FinishTurn(); // blocked / cannot close the distance this turn
        return;
    }

    // Move, then (move-then-attack) strike if the move brought us adjacent.
    const auto eid = enemy.mEntityIndex;
    const auto src = enemy.mGridPos;
    mStage.MoveCombatant(eid, src, bestCell,
        [this, eid, bestCell]{
            CompleteMove(eid, bestCell);
            auto* e = GetCombatant(eid);
            auto* t = e ? NearestEnemyOf(*e) : nullptr;
            if (e && t && ChebyshevDistance(e->mGridPos, t->mGridPos) <= 1)
                PlayMeleeAttack(*e, *t, ChooseBestAttackType(*e, *t));
            else
                FinishTurn();
        });
}

void CombatManager::CompleteMove(BAK::EntityIndex entityIndex, glm::uvec2 target)
{
    auto* combatant = GetCombatant(entityIndex);
    if (!combatant) return;

    const auto dist = ChebyshevDistance(combatant->mGridPos, target);

    auto& oldCell = mGrid.Get(combatant->mGridPos.x, combatant->mGridPos.y);
    oldCell.mElement = nullptr;

    combatant->mGridPos = target;

    auto& newCell = mGrid.Get(target.x, target.y);
    newCell.mElement = combatant;

    // Consume movement and refresh the grid from the new position. The turn does NOT end
    // here — the combatant may now attack, move again, or pass (FinishTurn) is the caller's
    // decision (player: explicit; enemy AI: in DoEnemyTurn's move callback).
    mMovementRemaining = (mMovementRemaining > dist) ? (mMovementRemaining - dist) : 0;
    ComputeGrid();
}

void CombatManager::FinishTurn()
{
    GetCurrentCombatant().mTurnPending = false;

    // Combat decided? Stop the turn loop and request the (deferred) exit to the world.
    if (AllEnemiesDead() || AllPartyDead())
    {
        const bool won = AllEnemiesDead();
        mLogger.Info() << (won
            ? "All enemies defeated - combat won\n"
            : "Party defeated - combat lost\n");
        mStage.RequestCombatExit(won ? BAK::CombatResult::Won : BAK::CombatResult::Dead);
        return;
    }

    // New round once every living combatant has acted: re-arm the living.
    auto it = std::find_if(mCombatants.begin(), mCombatants.end(),
        [](auto& c){ return !c.mIsDead && c.mTurnPending; });
    if (it == mCombatants.end())
    {
        for (auto& c : mCombatants)
            if (!c.mIsDead) c.mTurnPending = true;
    }

    SetCurrentCombatant(false);

    // If it is now an enemy's turn, run its AI automatically (chains via callbacks).
    auto& next = GetCurrentCombatant();
    if (next.mCharacter && next.mCharacter->IsEnemy() && !next.mIsDead)
    {
        DoEnemyTurn();
    }
}

void CombatManager::ClearGrid()
{
    for (unsigned y = 0; y < mGrid.GetRows(); y++)
    {
        for (unsigned x = 0; x < mGrid.GetCols(); x++)
        {
            auto& gridCell = mGrid.Get(x, y);
            gridCell.mElement = nullptr;
            gridCell.mState = 0;
        }
    }
}

}
