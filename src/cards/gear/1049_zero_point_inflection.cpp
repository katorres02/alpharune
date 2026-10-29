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

class ZeroPointInflection : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you play a card, draw 2."
    // "When you draw 1 or more cards with 9 or more other cards in your hand,
    //  discard your hand, kill this, and choose an opponent. They score 1 point."
    // "Other cards" = the hand minus the cards just drawn
    // (PlayerState::last_draw_count), checked when the draw happens.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayACard, TriggerType::WhenYouDrawACard};
    }
    bool triggerCondition(const GameState& state, GameObjectId, PlayerId controller,
                          TriggerType t, GameObjectId) const override {
        if (t != TriggerType::WhenYouDrawACard) return true;
        const auto& ps = state.player(controller);
        return static_cast<int>(ps.hand.size()) - ps.last_draw_count >= 9;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenYouPlayACard) {
            ctx.executor.drawCards(ctx.controller, 2);
            return;
        }
        if (ctx.firing_trigger != TriggerType::WhenYouDrawACard) return;
        const auto hand = ctx.state.player(ctx.controller).hand;
        for (auto id : hand) ctx.executor.applyDiscard(ctx.controller, id);
        if (ctx.state.objectExists(ctx.source) && ctx.state.getObject(ctx.source).location.has_value())
            ctx.executor.killObject(ctx.source);
        ctx.executor.scorePoints(opponent(ctx.controller), 1);   // 1v1: the only opponent
        ctx.events.logTrace("ZERO POINT INFLECTION: discarded hand, killed, opponent scores 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1049;
        d.def_id = R"RB(rad-069-167)RB";
        d.name = R"RB(Zero Point Inflection)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-069/167)RB";
        d.collector_number = 69;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.rarity = Rarity::Epic;
        d.energy_cost = 9;
        d.power_cost = 2;
        d.ability_text = R"RB(When you play a card, draw 2.
When you draw 1 or more cards with 9 or more other cards in your hand, discard your hand, kill this, and choose an opponent. They score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5e0646270d27f9fa78f4c3a637695e5cdeb8ee67-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1049(CardRegistry& r) {
    r.registerCard(1049, std::make_unique<ZeroPointInflection>());
}

}  // namespace riftbound
