#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class UnlicensedArmory : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "Discard 1, [E]: Choose a friendly unit. The next time it would die this
    //  turn, you may pay [C] to heal it, exhaust it, and recall it instead."
    // [C] = one Fury power (my Domain). The activation marks the chosen unit
    // (keyed by this Armory and the turn); this Armory's death replacement
    // consumes the mark the next time the unit would die this turn and asks
    // whether to pay [C] (only offered when payable).
    // GAP: the mark is read through this gear's replacement hook, so it lapses
    // if the Armory leaves the board before the unit would die.
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true, .discard = true, .discard_count = 1},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                           .must_be_friendly = true},
            .is_action = false, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int /*ability_index*/) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            out.push_back(id);
        }
        return out;
    }
    static std::string markKey(GameObjectId armory) {
        return "__unlicensed_armory_" + std::to_string(armory);
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) picked = targets[0];
        else picked = pickTarget(ctx, "Unlicensed Armory: choose a friendly unit",
                                 enumerateLegalTargets(ctx.state, ctx.controller, 0));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        // turn_number + 1 so that turn 0 still reads as set.
        ctx.state.getObject(picked).card_counters[markKey(ctx.source)] =
            ctx.state.turn.turn_number + 1;
        ctx.events.logTrace("UNLICENSED ARMORY: " + ctx.state.getObject(picked).name +
                            " - next death this turn may pay [C] to heal/exhaust/recall");
    }

    bool hasReplacementEffect() const override { return true; }
    bool applyReplacement(CardContext& ctx, GameObjectId dying_unit) override {
        if (!ctx.state.objectExists(dying_unit)) return false;
        auto& victim = ctx.state.getObject(dying_unit);
        auto it = victim.card_counters.find(markKey(ctx.source));
        if (it == victim.card_counters.end()) return false;
        const bool this_turn = it->second == ctx.state.turn.turn_number + 1;
        victim.card_counters.erase(it);   // "the next time": used up either way
        if (!this_turn || !victim.isUnit()) return false;
        if (!payableOnePower(ctx.state, ctx.controller, Domain::Fury)) return false;
        if (!ctx.executor.confirmNow(ctx.controller,
                "Unlicensed Armory: pay [C] to save " + victim.name)) return false;
        if (!payOnePower(ctx, ctx.controller, Domain::Fury)) return false;
        ctx.events.logTrace("UNLICENSED ARMORY: paid [C] -> heal/exhaust/recall " + victim.name);
        ctx.executor.healObject(dying_unit);
        ctx.executor.exhaustObject(dying_unit);
        ctx.executor.moveToBase(dying_unit);
        return true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 23;
        d.def_id = R"RB(ogn-023-298)RB";
        d.name = R"RB(Unlicensed Armory)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-023/298)RB";
        d.collector_number = 23;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Discard 1, [E]: Choose a friendly unit. The next time it would die this turn, you may pay [C] to heal it, exhaust it, and recall it instead. (Send it to base. This isn't a move.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1a80bedd893d9024fc90f108da36b4fd1d496ad6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_23(CardRegistry& r) {
    r.registerCard(23, std::make_unique<UnlicensedArmory>());
}

} // namespace riftbound
