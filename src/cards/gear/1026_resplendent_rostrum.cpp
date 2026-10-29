#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class ResplendentRostrum : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "[E]: Draw 1 and channel 1 rune exhausted."
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return ActivationCost{.exhaust = true}; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
        ctx.events.logTrace("RESPLENDENT ROSTRUM: draw 1, channel 1 rune exhausted");
    }
    // "When I become exhausted, give a friendly unit +3 [M] this turn."
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIBecomeExhausted; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId mine = ctx.controller;
        auto friends = chooseableUnits(ctx.state, mine,
            [mine](const GameObject& o) { return o.controller == mine; });
        std::sort(friends.begin(), friends.end());
        GameObjectId t = pickTarget(ctx, "Resplendent Rostrum: +3 [M] to a friendly unit", friends);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.giveTemporaryMight(t, 3);
        ctx.events.logTrace("RESPLENDENT ROSTRUM: +3 [M] to " + ctx.state.getObject(t).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1026;
        d.def_id = R"RB(rad-046-167)RB";
        d.name = R"RB(Resplendent Rostrum)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-046/167)RB";
        d.collector_number = 46;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.rarity = Rarity::Epic;
        d.energy_cost = 8;
        d.power_cost = 1;
        d.ability_text = R"RB([E]: Draw 1 and channel 1 rune exhausted.
When I become exhausted, give a friendly unit +3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/df1c12e65cc3c63c96fb6c96b2071795f37f30a6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1026(CardRegistry& r) {
    r.registerCard(1026, std::make_unique<ResplendentRostrum>());
}

}  // namespace riftbound
