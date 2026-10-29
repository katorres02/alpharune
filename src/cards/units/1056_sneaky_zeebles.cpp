#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace riftbound {
namespace {

class SneakyZeebles : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When I conquer, [Add] [A]."
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.addFloatingUniversalPower(ctx.controller, 1);
        ctx.events.logTrace("SNEAKY ZEEBLES: add [A]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1056;
        d.def_id = R"RB(rad-076-167)RB";
        d.name = R"RB(Sneaky Zeebles)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-076/167)RB";
        d.collector_number = 76;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Mount Targon)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 1;
        d.ability_text = R"RB(When I conquer, [Add] [A]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c04963c90ab2b338883c980ac947b8c9d80a7427-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1056(CardRegistry& r) {
    r.registerCard(1056, std::make_unique<SneakyZeebles>());
}

}  // namespace riftbound
