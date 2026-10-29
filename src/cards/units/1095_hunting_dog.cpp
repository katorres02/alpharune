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

// "[Deathknell][>] Play a 1 [M] Bird unit token with [Deflect] to your base."
class HuntingDog : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        KeywordSet kw;
        kw.set(Keyword::Deflect);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Bird", 1, {"Bird"}, kw,
                                 LocationId{BaseLocation{ctx.controller}}, false);
        ctx.events.logTrace("HUNTING DOG: Deathknell -> 1 [M] Bird with [Deflect] to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1095;
        d.def_id = R"RB(rad-119-167)RB";
        d.name = R"RB(Hunting Dog)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-119/167)RB";
        d.collector_number = 119;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Demacia)RB", R"RB(Dog)RB"};   // as printed
        d.rarity = Rarity::Common;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell][>] Play a 1 [M] Bird unit token with [Deflect] to your base. (When I die, get the effect. Opponents must pay [A] to choose a [Deflect] unit with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cefd575150527670879748a7212f80a1f4c1c4fc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1095(CardRegistry& r) {
    r.registerCard(1095, std::make_unique<HuntingDog>());
}

}  // namespace riftbound
