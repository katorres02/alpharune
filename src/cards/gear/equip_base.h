#pragma once
/// @file equip_base.h
/// Shared equip infrastructure for gear cards — the canonical standardEquip
/// helper plus the SimpleEquipGear / UniversalEquipGear base classes. Lives in
/// the gear/ type dir (relocated from the old _shared holding pen). This is the
/// ONE canonical standardEquip; the old per-audit-file copies redirect here.

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {

// ── Equip: pay one power (of `domain`, or any rune if nullopt), then attach ──
// Complements the energy+Domain `standardEquip` overload in gear/equip_base.h.
// One Power of `domain` (any domain if nullopt), the way CR 164.2 lets it be
// paid: floating Power of that domain, then universal Power, then recycling a
// matching rune — ready or exhausted (an exhausted one first, since a ready
// rune can still be exhausted for Energy). `purpose` names what the Power pays
// for (an [Equip] is SpendPurpose::GearAbility), so restricted [A] allowed for
// it (RunePool::restricted — Fire Below the Mountain) may be spent too.
inline bool runeMatchesDomain(const GameObject& rune, std::optional<Domain> domain) {
    if (!domain.has_value()) return true;
    for (auto d : rune.domains) if (d == *domain) return true;
    return false;
}
inline bool inShowdown(const GameState& state) {
    return state.turn.ns_state == NeutralShowdownState::Showdown;
}
inline bool payableOnePower(const GameState& state, PlayerId player,
                            std::optional<Domain> domain,
                            SpendPurpose purpose = SpendPurpose::None) {
    const auto& pool = state.player(player).rune_pool;
    if (pool.universal_power > 0) return true;
    if (pool.restrictedAvailable(/*power=*/true, purpose, inShowdown(state)) > 0) return true;
    if (domain.has_value()) { if (pool.power[static_cast<int>(*domain)] > 0) return true; }
    else if (pool.totalPower() > 0) return true;
    const LocationId base{BaseLocation{player}};
    for (const auto& [id, obj] : state.objects)
        if (obj.isRune() && obj.controller == player && obj.location.has_value() &&
            *obj.location == base && runeMatchesDomain(obj, domain)) return true;
    return false;
}
inline bool payOnePower(CardContext& ctx, PlayerId player,
                        std::optional<Domain> domain,
                        SpendPurpose purpose = SpendPurpose::None) {
    auto& state = ctx.state;
    auto& pool = state.player(player).rune_pool;
    if (domain.has_value()) {
        int& own = pool.power[static_cast<int>(*domain)];
        if (own > 0) { --own; return true; }
    } else {
        for (int di = 0; di < static_cast<int>(Domain::Count); ++di)
            if (pool.power[di] > 0) { --pool.power[di]; return true; }
    }
    if (pool.spendRestricted(/*power=*/true, 1, purpose, inShowdown(state)) == 1) return true;
    if (pool.universal_power > 0) { --pool.universal_power; return true; }
    const LocationId base_loc{BaseLocation{player}};
    GameObjectId rune = kInvalidId;
    for (auto& [id, obj] : state.objects) {
        if (!obj.isRune() || obj.controller != player) continue;
        if (!obj.location.has_value() || *obj.location != base_loc) continue;
        if (!runeMatchesDomain(obj, domain)) continue;
        if (rune == kInvalidId || (obj.is_exhausted && !state.getObject(rune).is_exhausted))
            rune = id;
    }
    if (rune == kInvalidId) return false;
    auto& dr = state.getObject(rune);
    dr.location = std::nullopt;
    dr.zone = ZoneType::RuneDeck;
    state.player(player).rune_deck.insert(state.player(player).rune_deck.begin(), rune);
    ctx.events.logTrace("  ADDL_COST: recycled " + dr.name + " for power");
    return true;
}

// ── Paying Energy inside an effect ("you may pay [N] to ...") ───────────────
/// Energy `player` could pay right now: floating pool + ready runes in base.
inline int payableEnergy(const GameState& state, PlayerId player) {
    int n = state.player(player).rune_pool.energy;
    for (const auto& [id, obj] : state.objects)
        if (obj.isRune() && obj.controller == player && !obj.is_exhausted &&
            obj.location.has_value()) ++n;
    return n;
}

/// Pay `amount` Energy: floating pool first, then exhaust ready runes. Returns
/// false (paying nothing) if it can't be covered.
inline bool payEnergy(CardContext& ctx, PlayerId player, int amount) {
    if (amount <= 0) return true;
    if (payableEnergy(ctx.state, player) < amount) return false;
    auto& pool = ctx.state.player(player).rune_pool;
    int from_pool = std::min(pool.energy, amount);
    pool.energy -= from_pool;
    amount -= from_pool;
    for (auto& [id, obj] : ctx.state.objects) {
        if (amount <= 0) break;
        if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
        if (!obj.location.has_value()) continue;
        obj.is_exhausted = true;
        ctx.events.emit(ObjectStateChangedEvent{id, "exhausted"});
        --amount;
    }
    return true;
}

// ─── Canonical standard equip: pay [energy] + one [domain] power, then attach ──
inline bool standardEquip(CardContext& ctx, GameObjectId gear_id, GameObjectId unit_id,
                          int energy_cost, Domain domain) {
    auto& state = ctx.state;
    auto player = ctx.controller;
    auto& ps = state.player(player);
    auto base_loc = BaseLocation{player};

    // PRE-CHECK: bail (no state change) unless BOTH the energy and the
    // domain-power can be paid — otherwise the loops below would partially
    // pay and then "equip for free".
    int ready_count = 0;
    GameObjectId domain_rune = kInvalidId;
    for (auto& [id, obj] : state.objects) {
        if (!obj.isRune() || obj.controller != player) continue;
        if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
        if (!obj.is_exhausted) ready_count++;
        if (domain_rune == kInvalidId) {
            for (auto d : obj.domains) {
                if (d == domain) { domain_rune = id; break; }
            }
        }
    }
    if (ready_count < energy_cost) return false;
    // [Equip] is a gear ability: gear-restricted [A] can pay its Power.
    const bool restricted_power =
        ps.rune_pool.restrictedAvailable(/*power=*/true, SpendPurpose::GearAbility,
                                         inShowdown(state)) > 0;
    if (domain_rune == kInvalidId && !restricted_power) return false;

    // Pay energy: exhaust ready runes.
    if (energy_cost > 0) {
        int e_remaining = energy_cost;
        for (auto& [id, obj] : state.objects) {
            if (e_remaining <= 0) break;
            if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            obj.is_exhausted = true;
            e_remaining--;
        }
    }

    // Recycle the pre-located matching-domain rune for power (restricted [A]
    // first when there is some).
    if (restricted_power) {
        ps.rune_pool.spendRestricted(/*power=*/true, 1, SpendPurpose::GearAbility,
                                     inShowdown(state));
        ctx.events.logTrace("  EQUIP_COST: paid power with gear-only [A]");
    } else {
        auto& dr = state.getObject(domain_rune);
        ctx.events.logTrace("  EQUIP_COST: recycled " + dr.name + " for power");
        dr.location = std::nullopt;
        dr.zone = ZoneType::RuneDeck;
        ps.rune_deck.insert(ps.rune_deck.begin(), domain_rune);
    }

    // Attach.
    auto& gear = state.getObject(gear_id);
    auto& unit = state.getObject(unit_id);
    ctx.events.logTrace("EQUIP: " + gear.name + " -> " + unit.name +
                         " (might_bonus=" + std::to_string(gear.might_bonus) + ")");
    gear.attached_to = unit_id;
    unit.attachments.push_back(gear_id);
    gear.location = unit.location;
    gear.zone = unit.zone;
    unit.attachment_might_bonus += gear.might_bonus;
    unit.recomputeMight();
    ctx.events.emit(ObjectStateChangedEvent{gear_id, "attached"});
    ctx.events.emit(ObjectStateChangedEvent{unit_id, "equipped"});
    return true;
}

// ─── Simple equip gear: pay [DOMAIN] power, then attach ─────────────────────
class SimpleEquipGear : public GearCard {
public:
    // Behavior config only (equip domain + energy). The concrete card supplies
    // its data via def().
    explicit SimpleEquipGear(Domain equip_domain, int energy_cost = 0)
        : equip_domain_(equip_domain), energy_cost_(energy_cost) {}

    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        return standardEquip(ctx, ctx.source, unit, energy_cost_, equip_domain_);
    }

protected:
    Domain equip_domain_;
    int energy_cost_;
};

// ─── Universal equip gear: pay [A] (recycle any rune), then attach ──────────
class UniversalEquipGear : public GearCard {
public:
    explicit UniversalEquipGear(int energy_cost = 0)
        : energy_cost_(energy_cost) {}

    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        auto& state = ctx.state;
        auto player = ctx.controller;
        if (!state.objectExists(unit)) return false;
        // PRE-CHECK (no state change): the Energy and the one [A] must both be
        // payable — otherwise the gear would be attached for free.
        if (payableEnergy(state, player) < energy_cost_) return false;
        if (!payableOnePower(state, player, std::nullopt, SpendPurpose::GearAbility))
            return false;
        if (!payEnergy(ctx, player, energy_cost_)) return false;
        if (!payOnePower(ctx, player, std::nullopt, SpendPurpose::GearAbility)) return false;
        auto& gear = state.getObject(ctx.source);
        auto& unit_obj = state.getObject(unit);
        ctx.events.logTrace("EQUIP: " + gear.name + " -> " + unit_obj.name);
        gear.attached_to = unit;
        unit_obj.attachments.push_back(ctx.source);
        gear.location = unit_obj.location;
        gear.zone = unit_obj.zone;
        unit_obj.attachment_might_bonus += gear.might_bonus;
        unit_obj.recomputeMight();
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{unit, "equipped"});
        return true;
    }

protected:
    int energy_cost_;
};

} // namespace riftbound
