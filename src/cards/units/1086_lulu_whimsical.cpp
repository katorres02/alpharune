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

// "When I move, you may have another unit's base Might become 4 this turn."
class LuluWhimsical : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const GameObjectId self = ctx.source;
        auto others = [&]() {
            return chooseableUnits(ctx.state, ctx.controller,
                                   [self](const GameObject& o) { return o.id != self; });
        };
        int go = confirmOptional(ctx, "Lulu, Whimsical: set another unit's base Might to 4?",
                                 [&]() { return !others().empty(); });
        if (go <= 0) return;
        GameObjectId t = pickTarget(ctx, "Lulu, Whimsical: base Might becomes 4", others());
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        setBaseMightThisTurn(ctx.state, t, 4);
        ctx.events.logTrace("LULU, WHIMSICAL: " + ctx.state.getObject(t).name +
                            " base Might is 4 this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1086;
        d.def_id = R"RB(rad-110-167)RB";
        d.name = R"RB(Lulu, Whimsical)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-110/167)RB";
        d.collector_number = 110;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Lulu)RB", R"RB(Bandle City)RB", R"RB(Yordle)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.ability_text = R"RB(When I move, you may have another unit's base Might become 4 this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/defd09480671a1e394725f21b6c3448ebec77913-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1086(CardRegistry& r) {
    r.registerCard(1086, std::make_unique<LuluWhimsical>());
}

}  // namespace riftbound
