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

class RiptideMage : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Accelerate]" engine-handled. "When you play me, give an enemy unit -3 [M] this turn."
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId mine = ctx.controller;
        auto enemies = chooseableUnits(ctx.state, mine,
            [mine](const GameObject& o) { return o.controller != mine; });
        std::sort(enemies.begin(), enemies.end());
        GameObjectId t = pickTarget(ctx, "Riptide Mage: give an enemy unit -3 [M]", enemies);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.giveTemporaryMight(t, -3);
        ctx.events.logTrace("RIPTIDE MAGE: -3 [M] to " + ctx.state.getObject(t).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1038;
        d.def_id = R"RB(rad-058-167)RB";
        d.name = R"RB(Riptide Mage)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-058/167)RB";
        d.collector_number = 58;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Bilgewater)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][B] as an additional cost to have me enter ready.)
When you play me, give an enemy unit -3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4e3c24c1c315656772d0bfd8d100bc7893e94994-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1038(CardRegistry& r) {
    r.registerCard(1038, std::make_unique<RiptideMage>());
}

}  // namespace riftbound
