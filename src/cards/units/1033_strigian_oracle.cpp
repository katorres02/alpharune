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

class StrigianOracle : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When I become exhausted, [Predict]."
    TriggerType triggerType() const override { return TriggerType::WhenIBecomeExhausted; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.predict(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1033;
        d.def_id = R"RB(rad-053-167)RB";
        d.name = R"RB(Strigian Oracle)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-053/167)RB";
        d.collector_number = 53;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Freljord)RB", R"RB(Bird)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 2;
        d.keywords.set(Keyword::Predict);
        d.ability_text = R"RB(When I become exhausted, [Predict]. (Look at the top card of your Main Deck. You may recycle it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5973b5036060f29e24f8778949bf6f01f360c0a4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1033(CardRegistry& r) {
    r.registerCard(1033, std::make_unique<StrigianOracle>());
}

}  // namespace riftbound
