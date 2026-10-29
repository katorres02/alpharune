#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// "[Deploy]. When you win a combat here, draw 1."
// The trigger manager fires WhenIWinCombat on the winner's unattached gear at
// the combat's battlefield.
class BlossomingDreamroot : public DeployGear {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIWinCombat; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("BLOSSOMING DREAMROOT: won a combat here -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1009;
        d.def_id = R"RB(rad-024-167)RB";
        d.name = R"RB(Blossoming Dreamroot)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-024/167)RB";
        d.collector_number = 24;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When you win a combat here, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5b819cc232389f3ef1438350d68c7801e181961c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1009(CardRegistry& r) {
    r.registerCard(1009, std::make_unique<BlossomingDreamroot>());
}

}  // namespace riftbound
