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

// "When you play this or when this leaves the board, banish the top card of
// your Main Deck. Until your next Ending Phase, you may play that card. (You
// still pay its costs.)
//  [2][P], [E]: Kill this."
// The permission is PlayerState::banished_playable (cleared as the player's
// next Ending Phase begins); the play generators offer those cards like cards
// in hand.
class RunebladeAnvil : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayThis, TriggerType::WhenILeaveTheBoard};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.main_deck.empty()) return;
        const GameObjectId top = ps.main_deck.back();
        ctx.executor.banishObject(top);
        if (!ctx.state.objectExists(top) || ctx.state.getObject(top).zone != ZoneType::Banishment) return;
        ps.banished_playable.push_back(top);
        ctx.events.logTrace("RUNEBLADE ANVIL: banished " + ctx.state.getObject(top).name +
                            " — playable until your next Ending Phase");
    }

    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .energy = 2, .power = 1,
                              .power_domain = Domain::Chaos};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;
        ctx.executor.killObject(ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 997;
        d.def_id = R"RB(rad-012-167)RB";
        d.name = R"RB(Runeblade Anvil)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-012/167)RB";
        d.collector_number = 12;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 1;
        d.power_cost = 0;
        d.ability_text = R"RB(When you play this or when this leaves the board, banish the top card of your Main Deck. Until your next Ending Phase, you may play that card. (You still pay its costs.)
[2][P], [E]: Kill this.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/11e2ddea70ec4dd8e562681a1ab1dfbc2b2dca0f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_997(CardRegistry& r) {
    r.registerCard(997, std::make_unique<RunebladeAnvil>());
}

}  // namespace riftbound
