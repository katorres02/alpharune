#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class TheWatersWage : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // [Hidden] is engine-handled. "Play a Gold gear token exhausted. If it's
    // your turn and you played this from face down, play three ready Gold gear
    // tokens instead." GameEngine::executePlayFromHidden stamps
    // card_counters["__played_from_facedown"].
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        bool from_facedown = false;
        if (ctx.state.objectExists(ctx.source)) {
            auto& self = ctx.state.getObject(ctx.source);
            auto it = self.card_counters.find("__played_from_facedown");
            from_facedown = it != self.card_counters.end() && it->second == 1;
        }
        if (from_facedown && ctx.state.turn.turn_player == ctx.controller) {
            for (int i = 0; i < 3; ++i) createGoldToken(ctx);
            ctx.events.logTrace("THE WATERS' WAGE: from face down on your turn -> 3 ready Gold");
        } else {
            createGoldExhausted(ctx);
            ctx.events.logTrace("THE WATERS' WAGE: 1 Gold exhausted");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1079;
        d.def_id = R"RB(rad-100-167)RB";
        d.name = R"RB(The Waters' Wage)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-100/167)RB";
        d.collector_number = 100;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 1;
        d.power_cost = 0;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
Play a Gold gear token exhausted. If it's your turn and you played this from face down, play three ready Gold gear tokens instead. (Gold tokens have "[Reaction][>] Kill this, [E]: [Add] [A]."))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0552932562db1dd48f14c74779999a645d62cc0d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1079(CardRegistry& r) {
    r.registerCard(1079, std::make_unique<TheWatersWage>());
}

}  // namespace riftbound
