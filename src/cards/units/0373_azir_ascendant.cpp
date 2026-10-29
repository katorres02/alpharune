#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class AzirAscendant : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }

    bool hasActivatedAbility() const override { return true; }
    bool isActionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.power = 1, .power_domain = Domain::Calm};
    }

    // Choose another friendly unit you control (target). Azir itself excluded.
    static std::vector<GameObjectId> legalTargets(const GameState& state,
                                                  PlayerId controller,
                                                  GameObjectId self_def_holder_excluded) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            if (id == self_def_holder_excluded) continue;
            out.push_back(id);
        }
        return out;
    }

    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        // Find Azir's on-board object id to exclude it from the candidate list.
        GameObjectId azir = kInvalidId;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id == cardDefId() && obj.controller == controller &&
                obj.location.has_value()) {
                azir = id;
                break;
            }
        }
        return legalTargets(state, controller, azir);
    }

    // Once-per-turn legality gate. Find Azir's on-board instance, require it to
    // be on board, ready (engine also gates), with at least one move target,
    // and not already used this turn.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            auto it = obj.card_counters.find("azir_used_turn");
            if (it != obj.card_counters.end() &&
                it->second == state.turn.turn_number + 1) {
                return false;  // already used this turn
            }
            // Need at least one other friendly unit to swap with.
            return !legalTargets(state, controller, id).empty();
        }
        return false;
    }

    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        GameObjectId source = ctx.source;

        // "Choose a unit you control" — made as the ability is activated.
        std::vector<GameObjectId> legal_a = enumerateLegalTargets(ctx.state, ctx.controller);
        if (!targets.empty()) legal_a = {targets[0]};
        const GameObjectId picked = pickTarget(ctx, "Azir: choose a unit to swap with", legal_a);
        if (ctx.executor.hasPendingChoice()) return;   // waiting for the agent
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId) return;     // fizzled (no target / mistarget)
        if (!ctx.state.objectExists(picked) || picked == source) return;

        auto& ro = ctx.state.chain.resuming;
        if (ro.has_value() && ro->resume_data.size() < kSwapped + 1)
            ro->resume_data.resize(kSwapped + 1, 0);
        if (!ro.has_value() || ro->resume_data[kSwapped] == 0) {
            if (ro.has_value()) ro->resume_data[kSwapped] = 1;   // once, across re-entries
            auto& azir = ctx.state.getObject(source);
            auto& other = ctx.state.getObject(picked);

            // Mark once-per-turn used (store turn_number+1 so 0 = never).
            azir.card_counters["azir_used_turn"] = ctx.state.turn.turn_number + 1;

            // Swap locations. Capture before moving.
            auto azir_loc = azir.location;
            auto other_loc = other.location;
            moveToLoc(ctx.executor, source, other_loc);
            moveToLoc(ctx.executor, picked, azir_loc);
            ctx.events.logTrace("AZIR ASCENDANT: swapped locations with " + other.name);
        }

        // "If it's equipped, you may attach one of its Equipment to me." —
        // decided as it resolves: first whether to (no / yes), then which.
        if (!ctx.state.objectExists(picked) || !ctx.state.objectExists(source)) return;
        std::vector<GameObjectId> equipment;
        for (auto gid : ctx.state.getObject(picked).attachments)
            if (ctx.state.objectExists(gid) && isEquipment(ctx.state.getObject(gid)))
                equipment.push_back(gid);
        std::sort(equipment.begin(), equipment.end());
        int go = 0;
        const std::vector<std::pair<int, std::string>> yes_no =
            equipment.empty() ? std::vector<std::pair<int, std::string>>{}
                              : std::vector<std::pair<int, std::string>>{
                                    {0, "Don't attach"}, {1, "Attach one of its Equipment to Azir"}};
        if (!stepChooseValue(ctx, ctx.controller, yes_no,
                             "Azir: attach one of its Equipment to me?",
                             /*rp=*/20, /*slot=*/10, /*fallback=*/0, go))
            return;   // waiting for the agent
        if (go != 1) return;
        GameObjectId gear = kInvalidId;
        if (!stepChooseObject(ctx, ctx.controller, equipment, "Azir: which Equipment",
                              /*rp=*/22, /*slot=*/11, gear))
            return;   // waiting for the agent
        if (gear != kInvalidId && ctx.state.objectExists(gear))
            reattachGear(ctx, gear, picked, source);
    }

private:
    static constexpr size_t kSwapped = 12;   // resume_data: 1 = the swap was done
    static void moveToLoc(EffectExecutor& exec, GameObjectId obj,
                          const std::optional<LocationId>& loc) {
        if (!loc) return;
        if (std::holds_alternative<BattlefieldLocation>(*loc)) {
            exec.moveToBattlefield(obj, std::get<BattlefieldLocation>(*loc).id);
        } else {
            exec.moveToBase(obj);
        }
    }

    // Detach `gear` from `from_unit` and attach to `to_unit`, transferring the
    // gear's might bonus. Mirrors the engine's equip bookkeeping (might_bonus +
    // attachment lists).
    static void reattachGear(CardContext& ctx, GameObjectId gear,
                             GameObjectId /*from_unit*/, GameObjectId to_unit) {
        if (!ctx.state.objectExists(gear) || !ctx.state.objectExists(to_unit)) return;
        // Detach from old unit (refunds might + clears attach state).
        ctx.executor.unattachGear(gear);
        if (!ctx.state.objectExists(gear)) return;
        auto& g = ctx.state.getObject(gear);
        auto& dest = ctx.state.getObject(to_unit);
        g.attached_to = to_unit;
        dest.attachments.push_back(gear);
        dest.attachment_might_bonus += g.might_bonus;
        dest.recomputeMight();
        ctx.events.logTrace("AZIR ASCENDANT: attached " + g.name + " to me");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 373;
        d.def_id = R"RB(sfd-050-221)RB";
        d.name = R"RB(Azir, Ascendant)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-050/221)RB";
        d.collector_number = 50;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Bird)RB", R"RB(Azir)RB", R"RB(Shurima)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([G]: [Action] — Choose a unit you control. Move me to its location and it to my original location. If it's equipped, you may attach one of its Equipment to me. Use only once per turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c42c02f6ebd2e4d4a640c2c6ac081231439888e-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_373(CardRegistry& r) {
    r.registerCard(373, std::make_unique<AzirAscendant>());
}

} // namespace riftbound
