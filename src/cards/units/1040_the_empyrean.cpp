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

class TheEmpyrean : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Show Off] a card." Any card in hand, or any other friendly card on the
    // board (tokens are not cards).
    bool hasShowOff() const override { return true; }
    bool showOffMatches(const GameState&, PlayerId, const GameObject& c) const override {
        return c.card_def_id != kInvalidId && !c.isToken();
    }
    // "When you play me, if you showed off a card, [Predict] equal to its
    // Power cost, then draw 2."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        GameObjectId shown = shownOff(ctx.state, ctx.source);
        if (shown == kInvalidId || !ctx.state.objectExists(shown)) return;
        const auto& s = ctx.state.getObject(shown);
        int power = s.card_def_id != kInvalidId ? ctx.executor.cardDB().get(s.card_def_id).power_cost : 0;
        if (power > 0) ctx.executor.predict(ctx.controller, power);
        ctx.executor.drawCards(ctx.controller, 2);
        ctx.events.logTrace("THE EMPYREAN: showed off " + s.name + " -> predict " +
                            std::to_string(power) + ", draw 2");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1040;
        d.def_id = R"RB(rad-060-167)RB";
        d.name = R"RB(The Empyrean)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-060/167)RB";
        d.collector_number = 60;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Ionia)RB", R"RB(Dragon)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 8;
        d.power_cost = 0;
        d.might = 8;
        d.keywords.set(Keyword::Predict);
        d.ability_text = R"RB([Show Off] a card. (As you play me, you may reveal a card from hand or pick another friendly one.)
When you play me, if you showed off a card, [Predict] equal to its Power cost, then draw 2. (Look at that many cards from your Main Deck. Recycle any of them and put the rest back in any order.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b7cd2e475834863bf4e66994118a835ba4377d16-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1040(CardRegistry& r) {
    r.registerCard(1040, std::make_unique<TheEmpyrean>());
}

}  // namespace riftbound
