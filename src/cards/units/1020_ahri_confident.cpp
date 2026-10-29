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

// "[Disarm]. When you reduce the Might of an enemy unit here by 1 or more,
//  give me +1 [M] this turn."
// The trigger manager fires WhenYouReduceEnemyMightHere when a spell or ability
// its controller controls gives an enemy unit at my location -[M] (her own
// Disarm included).
class AhriConfident : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    int disarmValue() const override { return 1; }
    TriggerType triggerType() const override { return TriggerType::WhenYouReduceEnemyMightHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source) || !ctx.state.getObject(ctx.source).location)
            return;
        ctx.executor.giveTemporaryMight(ctx.source, 1);
        ctx.events.logTrace("AHRI, CONFIDENT: reduced an enemy's Might here -> +1 [M]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1020;
        d.def_id = R"RB(rad-038-167)RB";
        d.name = R"RB(Ahri, Confident)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-038/167)RB";
        d.collector_number = 38;
        d.artist = R"RB(Naifan Zhang)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ahri)RB", R"RB(Ionia)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.ability_text = R"RB([Disarm] (When I attack, give an enemy unit here -1 [M] this turn.)
When you reduce the Might of an enemy unit here by 1 or more, give me +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cf3c6a5963ad27976960977b0a51ea185f551696-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1020(CardRegistry& r) {
    r.registerCard(1020, std::make_unique<AhriConfident>());
}

}  // namespace riftbound
