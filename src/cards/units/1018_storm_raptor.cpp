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

// "[Deflect]. When a stunned enemy unit here dies, draw 1."
class StormRaptor : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenAnEnemyUnitDiesHere; }
    // Stunned is not cleared by dying, so the dead unit still shows it.
    bool triggerCondition(const GameState& state, GameObjectId, PlayerId, TriggerType,
                          GameObjectId subject) const override {
        return subject != kInvalidId && state.objectExists(subject) &&
               state.getObject(subject).is_stunned;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("STORM RAPTOR: a stunned enemy unit died here -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1018;
        d.def_id = R"RB(rad-036-167)RB";
        d.name = R"RB(Storm Raptor)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-036/167)RB";
        d.collector_number = 36;
        d.artist = R"RB(Bubble Cat Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Freljord)RB", R"RB(Bird)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 6;
        d.power_cost = 0;
        d.might = 6;
        d.keywords.set(Keyword::Deflect);
        d.deflect_value = 1;
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
When a stunned enemy unit here dies, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fc59661dfc70a1fdb393d795c90779a2b5661609-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1018(CardRegistry& r) {
    r.registerCard(1018, std::make_unique<StormRaptor>());
}

}  // namespace riftbound
