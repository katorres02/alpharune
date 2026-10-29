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

class ForgottenSignpost : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Action][>] Exhaust a unit you control, [E]: Move a different unit you
    //  control to the location of the unit you exhausted to pay for this
    //  ability."
    // [>]/[E] = exhaust the gear (engine-paid). Exhausting a friendly unit is
    // an ADDITIONAL COST: it is chosen and paid as the ability is played
    // (kFinalizeActivate), together with the unit to move, so the exhausted
    // unit is exhausted while the ability is on the chain.
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true},
            .targets = {},
            .is_action = true, .is_reaction = false,
        }};
    }
    // Playable only if a ready friendly unit can pay and a different friendly
    // unit could be moved to its location.
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!Card::canActivateAbilityAt(state, controller, source, ability_index)) return false;
        for (auto a : payers(state, controller, /*need_ready=*/true))
            if (!movers(state, controller, a).empty()) return true;
        return false;
    }
    static std::vector<GameObjectId> payers(const GameState& state, PlayerId controller,
                                            bool need_ready) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller || !obj.location.has_value()) continue;
            if (need_ready && obj.is_exhausted) continue;
            out.push_back(id);
        }
        return out;
    }
    static std::vector<GameObjectId> movers(const GameState& state, PlayerId controller,
                                            GameObjectId exhausted) {
        std::vector<GameObjectId> out;
        if (!state.objectExists(exhausted)) return out;
        auto dest = state.getObject(exhausted).location;
        if (!dest) return out;
        for (auto& [id, obj] : state.objects) {
            if (id == exhausted) continue;
            if (!obj.isUnit() || obj.controller != controller || !obj.location.has_value()) continue;
            if (*obj.location == *dest) continue;  // already there
            out.push_back(id);
        }
        return out;
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // Choices: the unit exhausted as the cost (ready, unless it was already
        // exhausted to pay) and a different unit to move.
        const bool paid = costAlreadyPaid(ctx);
        auto [to_exhaust, mover] = pickTargetPair(ctx,
            "Forgotten Signpost: exhaust a friendly unit / move a different one",
            payers(ctx.state, ctx.controller, /*need_ready=*/!paid),
            [&](GameObjectId a) { return movers(ctx.state, ctx.controller, a); });
        if (to_exhaust == kInvalidId) return;   // suspended, or nothing to pay with
        if (mover == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point < 13) return;   // suspended
        if (costDueNow(ctx)) {
            if (!ctx.state.objectExists(to_exhaust) ||
                ctx.state.getObject(to_exhaust).is_exhausted) return;
            ctx.executor.exhaustObject(to_exhaust);
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)

        if (mover == kInvalidId || !ctx.state.objectExists(mover)) return;
        if (!ctx.state.objectExists(to_exhaust)) return;
        auto dest = ctx.state.getObject(to_exhaust).location;
        if (!dest) return;
        moveToLocation(ctx.executor, mover, dest);
        ctx.events.logTrace("FORGOTTEN SIGNPOST: moved a friendly unit to the exhausted unit's location");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 607;
        d.def_id = R"RB(unl-045-219)RB";
        d.name = R"RB(Forgotten Signpost)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-045/219)RB";
        d.collector_number = 45;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action][>] Exhaust a unit you control, [E]: Move a different unit you control to the location of the unit you exhausted to pay for this ability.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/edf248974d1d97ff5ff1c6745f663fa059dbc82d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_607(CardRegistry& r) {
    r.registerCard(607, std::make_unique<ForgottenSignpost>());
}

} // namespace riftbound
