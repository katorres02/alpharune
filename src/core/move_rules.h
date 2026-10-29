#pragma once

// Movement rules shared by the standard move (GameEngine) and the effect
// moves (EffectExecutor::moveToBase / moveToBattlefield / moveUnitsTogether).
//
// CR 448-455: a Move is a permanent changing position on the board; standard
// moves and moves caused by spells or abilities are both Moves, so "can't
// move" restrictions block both. CR 457-458: a Recall is NOT a Move and is
// never prevented by movement restrictions (EffectExecutor::recallToBase).
// CR 204.4: applied costs on moving ("Opponents must pay ... to move") are
// paid by the player performing the move, once per move action.

#include "core/game_state.h"
#include "core/types.h"

#include <string>

namespace riftbound {

/// The player performing an effect move: the controller of the resolving
/// spell or ability, or else the unit's controller (the same attribution
/// "can't be moved by enemy spells and abilities" uses).
inline PlayerId effectMover(const GameState& state, const GameObject& unit) {
    if (state.chain.resuming.has_value() && state.chain.resuming->controller != PlayerId::None)
        return state.chain.resuming->controller;
    return unit.controller;
}

/// Why `unit` can't move to `dest` when `mover` moves it, or nullptr when the
/// move is allowed. `by_effect` = a spell/ability move (not a standard move).
/// Covers the per-unit and per-battlefield movement restrictions; it does not
/// check standard-move reachability (base<->battlefield, Ganking) nor applied
/// costs (moveAppliedCost).
inline const char* moveForbidden(const GameState& state, const GameObject& unit,
                                 const LocationId& dest, PlayerId mover, bool by_effect) {
    // "I / it can't move this turn" (Entrenched Boar, Hunker Down, Vex).
    if (unit.cant_move_turn == state.turn.turn_number &&
        (unit.cant_move_only_by == PlayerId::None || unit.cant_move_only_by == mover))
        return "can't move this turn";
    // Jagged Cutlass: "can't be moved by enemy spells and abilities".
    if (by_effect && unit.cant_be_moved_by_enemy && mover != unit.controller)
        return "can't be moved by enemy spells and abilities";
    if (std::holds_alternative<BaseLocation>(dest)) {
        // Determined Sentry ("I can't move to base"), Minotaur Reckoner
        // ("Units can't move to base").
        if (unit.cant_move_to_base || unit.aura_cant_move_to_base)
            return "can't move to base";
        // Vilemaw's Lair: "Units can't move from here to base."
        if (auto here = unit.battlefieldId()) {
            for (const auto& bf : state.battlefields)
                if (bf.id == *here && bf.blocks_move_to_base)
                    return "can't move from this battlefield to base";
        }
    }
    return nullptr;
}

/// Applied cost `mover` pays to move `unit_count` units to `dest` as one move
/// (CR 204.4): Gravity Field's [1][A] once per move, plus Mageseeker
/// Investigator's [A] for each unit beyond the first.
struct MoveCost {
    int energy = 0;
    int power = 0;   // any-domain power
    bool any() const { return energy > 0 || power > 0; }
};
inline MoveCost moveAppliedCost(const GameState& state, PlayerId mover,
                                const LocationId& dest, int unit_count) {
    MoveCost c;
    const auto* b = std::get_if<BattlefieldLocation>(&dest);
    if (!b || unit_count <= 0 || mover == PlayerId::None) return c;
    const int pi = playerIndex(mover);
    for (const auto& bf : state.battlefields) {
        if (bf.id != b->id) continue;
        c.energy += std::max(0, bf.move_tax_energy[pi]);
        c.power += std::max(0, bf.move_tax_power[pi]);
        if (unit_count > 1) c.power += (unit_count - 1) * std::max(0, bf.multi_move_surcharge_power[pi]);
    }
    return c;
}

}  // namespace riftbound
