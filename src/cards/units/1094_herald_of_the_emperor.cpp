#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When you play me, choose a friendly unit. Its base Might becomes 5 this turn."
class HeraldOfTheEmperor : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId me = ctx.controller;
        auto friendly = chooseableUnits(ctx.state, me,
                                        [me](const GameObject& o) { return o.controller == me; });
        GameObjectId t = pickTarget(ctx, "Herald of the Emperor: base Might becomes 5", friendly);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        setBaseMightThisTurn(ctx.state, t, 5);
        ctx.events.logTrace("HERALD OF THE EMPEROR: " + ctx.state.getObject(t).name +
                            " base Might is 5 this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1094;
        d.def_id = R"RB(rad-118-167)RB";
        d.name = R"RB(Herald of the Emperor)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-118/167)RB";
        d.collector_number = 118;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Shurima)RB"};   // as printed
        d.rarity = Rarity::Common;
        d.energy_cost = 5;
        d.power_cost = 0;
        d.might = 3;
        d.ability_text = R"RB(When you play me, choose a friendly unit. Its base Might becomes 5 this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a168856002994f7ac67150d991e65152e6434ab6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1094(CardRegistry& r) {
    r.registerCard(1094, std::make_unique<HeraldOfTheEmperor>());
}

}  // namespace riftbound
