#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// [Deploy]. "When you play this or when you conquer here, you may pay [1] to
// play a 1 [M] Recruit unit token to your base." The conquer trigger is
// dispatched to gear at the scored battlefield by TriggerManager::onScore.
class ConcertPoster : public DeployGear {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayThis, TriggerType::WhenYouConquerHere};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const bool paid = costAlreadyPaid(ctx);
        int go = confirmOptional(ctx, "Concert Poster: pay [1] for a Recruit?", [&]() {
            return paid || payableEnergy(ctx.state, ctx.controller) >= 1;
        });
        if (go <= 0) return;
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;
        auto tok = ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1,
                                            {"Recruit"}, KeywordSet{},
                                            LocationId{BaseLocation{ctx.controller}}, false);
        bindTokenCard(ctx.state, tok, 954);
        ctx.events.logTrace("CONCERT POSTER: paid [1] -> 1 [M] Recruit to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1092;
        d.def_id = R"RB(rad-116-167)RB";
        d.name = R"RB(Concert Poster)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-116/167)RB";
        d.collector_number = 116;
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};   // gallery said "Device"; the printed card has no tag
        d.rarity = Rarity::Common;
        d.energy_cost = 1;
        d.power_cost = 0;
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When you play this or when you conquer here, you may pay [1] to play a 1 [M] Recruit unit token to your base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0e80322dac6065e3ee726f5f9a2650a88195a44c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1092(CardRegistry& r) {
    r.registerCard(1092, std::make_unique<ConcertPoster>());
}

}  // namespace riftbound
