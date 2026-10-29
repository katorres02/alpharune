#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// "[Reaction]. Give a unit [Shield 2] this turn. [Predict]."
class ConsultTheHeavens : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId t = pickTarget(ctx, "Consult the Heavens: give a unit [Shield 2]", legal);
        if (ctx.executor.hasPendingChoice()) return;
        if (t != kInvalidId && ctx.state.objectExists(t))
            ctx.executor.giveTemporaryKeyword(t, Keyword::Shield, 2);
        ctx.executor.predict(ctx.controller, 1);
        ctx.events.logTrace("CONSULT THE HEAVENS: [Shield 2] + Predict");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1010;
        d.def_id = R"RB(rad-025-167)RB";
        d.name = R"RB(Consult the Heavens)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-025/167)RB";
        d.collector_number = 25;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 1;
        d.power_cost = 0;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Give a unit [Shield 2] this turn. (+2 [M] while it's a defender.)
[Predict]. (Look at the top card of your Main Deck. You may recycle it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/120b20ee26d221cea31f510090cef037e74c023f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1010(CardRegistry& r) {
    r.registerCard(1010, std::make_unique<ConsultTheHeavens>());
}

}  // namespace riftbound
