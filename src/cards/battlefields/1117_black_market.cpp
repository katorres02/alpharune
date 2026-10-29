#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "[A][A]: Draw 1. Each player may use this ability once on their turn (even
// if they don't control this battlefield)." Offered to any player
// (anyPlayerMayActivate) on their own turn; the engine stamps
// "__ability_used_turn_0" on activation, and since a turn belongs to one
// player that stamp is also "this player used it this turn".
class BlackMarket : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    bool anyPlayerMayActivate() const override { return true; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.power = 2;
        c.power_any_domain = true;
        return c;
    }
    bool canActivateAbility(const GameState& state, PlayerId controller) const override {
        return state.turn.turn_player == controller;
    }
    bool canActivateAbilityAt(const GameState& state, PlayerId controller, GameObjectId source,
                              int ability_index) const override {
        if (state.turn.turn_player != controller || !state.objectExists(source)) return false;
        const auto& c = state.getObject(source).card_counters;
        auto it = c.find("__ability_used_turn_" + std::to_string(ability_index));
        return it == c.end() || it->second != state.turn.turn_number + 1;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("BLACK MARKET: draw 1");
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1117;
        d.def_id = R"RB(rad-158-167)RB";
        d.name = R"RB(Black Market)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-158/167)RB";
        d.collector_number = 158;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB([A][A]: Draw 1. Each player may use this ability once on their turn (even if they don't control this battlefield).)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9a02ae108f32a8b88be47a94b5f0d39b5a06a6a1-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1117(CardRegistry& r) {
    r.registerCard(1117, std::make_unique<BlackMarket>());
}

}  // namespace riftbound
