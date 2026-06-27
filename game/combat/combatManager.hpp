#pragma once

#include "game/combat/grid.hpp"
#include "game/combat/gridAlgorithms.hpp"
#include "game/combat/ICombatStage.hpp"

#include "bak/combat/ICombatManager.hpp"
#include "bak/combat/ICombatUI.hpp"

#include "bak/character.hpp"
#include "bak/combat/combatFormula.hpp"
#include "bak/combat/mechanics.hpp"
#include "bak/coordinates.hpp"

#include "com/bits.hpp"
#include "com/logger.hpp"

#include <cassert>
#include <optional>
#include <vector>

namespace Game::Combat {

struct Combatant
{
    BAK::Character* mCharacter;
    BAK::MonsterIndex mMonster;
    glm::uvec2 mGridPos;
    BAK::Combat::CombatantState mState;
    BAK::EntityIndex mEntityIndex;

    bool mTurnPending{true};
    bool mIsDead{false};
    bool mIsPoisoned{false};
};

enum class StateFlags
{
    Reachable     = 0,
    Attackable    = 1,
    LOSAttackable = 2,
    HasZap        = 3,
    HasMine       = 4,
    HasCrystal    = 5
};

struct GridElem
{
    BAK::GamePositionAndHeading mPos;
    std::uint16_t mState;
    Combatant* mElement{nullptr};
};

/* 
 * Combat procedure
 * Each turn
 *   Order the combatants by speed
 *   In order, combatants take a turn
 *
 * Combatant turn
 *   | Defend
 *   | Rest
 *   | Assess
 *   | Cast
 *   | Shoot
 *   | Move
 *     + Melee
 *   | Melee
 *   Next combatant
 */
class CombatManager : public BAK::ICombatManager
{
public:
    CombatManager(ICombatStage& stage, BAK::ICombatUI& ui);

    void SetCastingSpell(BAK::SpellIndex) override;
    void SetUsingCrossbow() override;
    void SetMeleeAttackType(BAK::Combat::MeleeAttackType) override;
    BAK::Combat::MeleeAttackType GetMeleeAttackType() const override;
    void BeginSpellCast() override;
    void EndTurn() override;

    void AddCombatant(Combatant combatant);

    void BeginCombat();

    void GridCellClicked(glm::uvec2 targetGrid);

    void EndCombat();

private:
    Combatant* GetCombatant(BAK::CharIndex character);
    Combatant* GetCombatant(BAK::EntityIndex entityIndex);
    Combatant* GetCombatant(glm::uvec2 gridPos);

    std::vector<Combatant> GetCombatants();

    void SetCurrentCombatant(bool onlyParty);
    Combatant& GetCurrentCombatant();

    void ComputeGrid();

    void PrintGridState();

    bool CanMoveTo(const Combatant& combatant, glm::uvec2 target) const;
    bool CanAttack(const Combatant& combatant, glm::uvec2 target) const;

    // Gather the formula inputs (effective skills + the chosen attack's weapon scalars).
    std::pair<BAK::Combat::AttackerProfile, BAK::Combat::DefenderProfile>
        GatherCombatProfiles(Combatant& attacker, Combatant& defender,
            BAK::Combat::MeleeAttackType attackType) const;
    // Pick the attack type with the higher expected damage (hit% * damage).
    BAK::Combat::MeleeAttackType ChooseBestAttackType(Combatant& attacker, Combatant& defender) const;
    // Melee resolution: roll to hit, apply damage, handle death. Returns true if the
    // defender died. Uses the (RE-tunable) combat formulas in bak/combat/combatFormula.
    bool ResolveMeleeAttack(Combatant& attacker, Combatant& defender,
        BAK::Combat::MeleeAttackType attackType);
    // Resolve a cast offensive spell on a target: spell damage, cost, death. Returns true
    // if the target died. Uses the (RE-tunable) spell formula in bak/combat/spellFormula.
    bool ResolveSpellAttack(Combatant& caster, Combatant& target);
    // Play a melee attack (resolve + animations) that ends the attacker's turn.
    void PlayMeleeAttack(Combatant& attacker, Combatant& defender,
        BAK::Combat::MeleeAttackType attackType);
    // Nearest living opponent of the given combatant, or nullptr.
    Combatant* NearestEnemyOf(const Combatant& combatant);
    // Run the current (enemy) combatant's AI turn: attack an adjacent party member, else
    // step toward the nearest one. Chains to FinishTurn via the animation callbacks.
    void DoEnemyTurn();
    // True if every enemy (or every party member) combatant is dead.
    bool AllEnemiesDead() const;
    bool AllPartyDead() const;

    void CompleteMove(BAK::EntityIndex entityIndex, glm::uvec2 target);

    void FinishTurn();

    void ClearGrid();

    std::vector<Combatant> mCombatants{};

    unsigned mCurrentCombatant{0};
    BAK::Combat::MeleeAttackType mPlayerAttackType{BAK::Combat::MeleeAttackType::Swing};
    // When set, the current (player) combatant is in spell-targeting mode: the next click
    // on an enemy resolves this spell at mCastPower.
    std::optional<BAK::SpellIndex> mCastingSpell{};
    unsigned mCastPower{};
    // Movement left this turn (reset to Speed each turn). Moving consumes it without
    // ending the turn, so a combatant can move several steps and then attack.
    unsigned mMovementRemaining{};
    Grid<GridElem> mGrid;
    ICombatStage& mStage;
    BAK::ICombatUI& mCombatUI;
    const Logging::Logger& mLogger;
};

}

