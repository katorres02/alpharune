#pragma once
/// @file weaponmaster_base.h
/// [Weaponmaster] (CR 821): "When you play me, you may [Equip] one of your
/// Equipment to me for [A] less, even if it's already attached."
///
/// CR 821.1.c: the "you may" and the choice of Equipment are made when the
/// trigger is finalized (CR 383.3.a); the Equip cost, reduced by one Power, is
/// paid as it resolves. If it can't be paid the Equipment stays where it is
/// (821.1.c.5); an Equipment without an Equip cost can't be paid (821.1.c.4).
///
/// Every Weaponmaster unit goes through `WeaponmasterUnit::weaponmaster(ctx)`.
/// Single-trigger cards inherit `onTrigger`; multi-trigger cards call
/// `weaponmaster(ctx)` from their WhenYouPlayMe branch (and must put the
/// `finalizingChoices` marker in their other branches: the base opts every
/// trigger of the card into kFinalizeTrigger).

#include "cards/card.h"
#include "cards/card_registry.h"
#include "cards/card_helpers.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace weaponmaster {

/// The Equip cost Weaponmaster charges for one Equipment.
struct EquipCost {
    bool payable_shape = false;           // an Equip cost made only of Energy/Power
    int energy = 0;
    std::vector<std::vector<Domain>> power;  // one entry per Power; empty list = [A]
};

/// The Equipment's printed "[Equip] [n][X]..." cost, as if activated choosing
/// `unit` (CR 821.1.c.2), reduced by one Power (CR 821.1.c: "[A] less").
/// ENGINE-GAP: an Equip cost lives inside each gear's onEquip — no Card hook
/// exposes it — so the printed cost is read from the gear's text. Equip costs
/// with non-rune parts ("— Spend 1 XP", "..., Kill a friendly unit", "[A] or
/// exhaust a friendly unit") are not offered.
inline EquipCost reducedEquipCost(const CardContext& ctx, const GameObject& gear,
                                  GameObjectId unit) {
    EquipCost out;
    // A definition-less gear object (engine/test stand-in) has no printed cost
    // to read: treated as an Equip cost of [A], i.e. free after the discount.
    if (gear.card_def_id == kInvalidId) { out.payable_shape = true; return out; }
    const auto& def = ctx.executor.cardDB().get(gear.card_def_id);
    const std::string& text = def.ability_text;
    static const std::string kKey = "[Equip]";
    size_t p = text.find(kKey);
    if (p == std::string::npos) return out;
    p += kKey.size();
    while (p < text.size() && text[p] == ' ') ++p;
    bool any = false;
    while (p < text.size() && text[p] == '[') {
        const size_t q = text.find(']', p);
        if (q == std::string::npos) return out;
        const std::string tok = text.substr(p + 1, q - p - 1);
        if (tok.empty()) return out;
        if (std::all_of(tok.begin(), tok.end(),
                        [](unsigned char c) { return c >= '0' && c <= '9'; })) {
            out.energy += std::stoi(tok);
        } else if (tok.size() == 1) {
            switch (tok[0]) {
                case 'R': out.power.push_back({Domain::Fury}); break;
                case 'G': out.power.push_back({Domain::Calm}); break;
                case 'B': out.power.push_back({Domain::Mind}); break;
                case 'O': out.power.push_back({Domain::Body}); break;
                case 'P': out.power.push_back({Domain::Chaos}); break;
                case 'Y': out.power.push_back({Domain::Order}); break;
                case 'A': out.power.push_back({}); break;
                case 'C': out.power.push_back(def.domains); break;   // CR 135.2.e.6
                default: return out;
            }
        } else {
            return out;
        }
        any = true;
        p = q + 1;
    }
    if (!any) return out;                                   // "[Equip] — ..." form
    while (p < text.size() && text[p] == ' ') ++p;
    if (p < text.size() && text[p] == ',') return out;      // further non-rune costs
    if (text.compare(p, 3, "or ") == 0) return out;
    // Hextech Gauntlets: "This ability's Energy cost is reduced by the Might of
    // the unit you choose." — the Weaponmaster unit (CR 821.1.c.2).
    if (text.find("Energy cost is reduced by the Might of the unit you choose") !=
            std::string::npos && ctx.state.objectExists(unit))
        out.energy = std::max(0, out.energy - ctx.state.getObject(unit).current_might);
    // "[A] less": one Power off — a domain-specific one first (the most
    // restrictive), else an [A]. No Power: paid unreduced (CR 821.1.c.3).
    if (!out.power.empty()) {
        auto it = std::find_if(out.power.begin(), out.power.end(),
                               [](const std::vector<Domain>& d) { return !d.empty(); });
        out.power.erase(it != out.power.end() ? it : out.power.begin());
    }
    out.payable_shape = true;
    return out;
}

/// Whether `cost` can be paid now (no state change). Energy is paid first by
/// exhausting runes, which can then still be recycled for Power.
inline bool canPay(const GameState& state, PlayerId player, const EquipCost& cost) {
    if (!cost.payable_shape) return false;
    if (payableEnergy(state, player) < cost.energy) return false;
    auto pool = state.player(player).rune_pool;
    std::vector<GameObjectId> used;
    const LocationId base{BaseLocation{player}};
    for (const auto& doms : cost.power) {
        bool paid = false;
        for (auto d : doms) {
            int& own = pool.power[static_cast<int>(d)];
            if (own > 0) { --own; paid = true; break; }
        }
        if (!paid && doms.empty()) {
            for (int di = 0; di < static_cast<int>(Domain::Count); ++di)
                if (pool.power[di] > 0) { --pool.power[di]; paid = true; break; }
        }
        if (!paid && pool.universal_power > 0) { --pool.universal_power; paid = true; }
        if (!paid) {
            for (const auto& [id, obj] : state.objects) {
                if (!obj.isRune() || obj.controller != player || !obj.location.has_value() ||
                    *obj.location != base) continue;
                if (std::find(used.begin(), used.end(), id) != used.end()) continue;
                bool match = doms.empty();
                for (auto d : doms) match = match || runeMatchesDomain(obj, d);
                if (!match) continue;
                used.push_back(id);
                paid = true;
                break;
            }
        }
        if (!paid) return false;
    }
    return true;
}

/// Pay `cost` (call canPay first).
inline bool pay(CardContext& ctx, const EquipCost& cost) {
    if (!canPay(ctx.state, ctx.controller, cost)) return false;
    if (!payEnergy(ctx, ctx.controller, cost.energy)) return false;
    for (const auto& doms : cost.power) {
        bool paid = false;
        if (doms.empty()) {
            paid = payOnePower(ctx, ctx.controller, std::nullopt);
        } else {
            for (auto d : doms)
                if (payableOnePower(ctx.state, ctx.controller, d) &&
                    payOnePower(ctx, ctx.controller, d)) { paid = true; break; }
        }
        if (!paid) return false;
    }
    return true;
}

/// Equipment `self`'s controller may choose (CR 821.1.c: "a card you control
/// with the Equipment tag"). With `payable_only`, only those whose reduced
/// Equip cost can be paid right now.
inline std::vector<GameObjectId> candidates(const CardContext& ctx, bool payable_only) {
    std::vector<GameObjectId> out;
    for (const auto& [id, obj] : ctx.state.objects) {
        if (!obj.isGear() || obj.controller != ctx.controller) continue;
        if (!obj.location.has_value() || !isEquipment(obj)) continue;
        // Already on me: nothing to attach.
        if (obj.attached_to.has_value() && *obj.attached_to == ctx.source) continue;
        if (payable_only &&
            !canPay(ctx.state, ctx.controller, reducedEquipCost(ctx, obj, ctx.source)))
            continue;
        out.push_back(id);
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace weaponmaster

/// Base class for Weaponmaster units.
class WeaponmasterUnit : public UnitCard {
public:
    // Abstract: concrete weaponmaster cards supply their data via def().
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        weaponmaster(ctx);
    }

protected:
    /// The whole [Weaponmaster] ability, including its end-of-choices marker.
    void weaponmaster(CardContext& ctx) {
        const GameObjectId gear_id = weaponmasterChoice(ctx);
        if (ctx.executor.hasPendingChoice()) return;   // waiting for the agent
        if (finalizingChoices(ctx)) return;            // choices end here (CR 383.3)
        weaponmasterEquip(ctx, gear_id);
    }

    /// The "you may" and the Equipment (made at finalization). kInvalidId when
    /// declined, nothing to choose, or waiting (check hasPendingChoice()).
    /// Uses confirmOptional (0/1/2) + pickTarget (6/7/8) slots.
    GameObjectId weaponmasterChoice(CardContext& ctx) {
        const std::string name = def().name;
        // Finalized earlier: the choice stands even if its cost became
        // unpayable (then nothing happens, CR 821.1.c.5).
        const auto& r = ctx.state.chain.resuming;
        const bool decided = r.has_value() && r->choices_finalized && !r->finalizing;
        int go = confirmOptional(ctx, name + ": [Weaponmaster] equip one of your Equipment?",
            [&ctx, decided]() {
                return ctx.state.objectExists(ctx.source) &&
                       !weaponmaster::candidates(ctx, !decided).empty();
            });
        if (go != 1) return kInvalidId;   // waiting, declined, or nothing to choose
        return pickTarget(ctx, name + ": [Weaponmaster] which Equipment",
                          weaponmaster::candidates(ctx, !decided));
    }

    /// Pay `gear_id`'s Equip cost reduced by [A] and attach it to me.
    void weaponmasterEquip(CardContext& ctx, GameObjectId gear_id) {
        const std::string name = def().name;
        if (gear_id == kInvalidId || !ctx.state.objectExists(gear_id)) return;
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return;
        auto& gear = ctx.state.getObject(gear_id);
        if (gear.controller != ctx.controller || !gear.location.has_value()) return;
        if (gear.attached_to.has_value() && *gear.attached_to == ctx.source) return;
        // Pay the Equip cost reduced by [A]; unpaid -> it stays (CR 821.1.c.5).
        const auto cost = weaponmaster::reducedEquipCost(ctx, gear, ctx.source);
        if (!weaponmaster::pay(ctx, cost)) {
            ctx.events.logTrace("WEAPONMASTER: " + name + " can't pay the Equip cost of " +
                                gear.name);
            return;
        }
        // "even if it's already attached" — through the executor so the
        // gear's onUnattached hook runs.
        if (ctx.state.getObject(gear_id).attached_to.has_value())
            ctx.executor.unattachGear(gear_id);
        attachFree(ctx, gear_id, ctx.source);
        ctx.events.logTrace("WEAPONMASTER: " + name + " equips " +
                            ctx.state.getObject(gear_id).name + " (Equip cost [A] less)");
    }
};

} // namespace riftbound
