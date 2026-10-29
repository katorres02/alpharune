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

class Fanfare : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "The next three times you play a unit this turn, draw 1." A delayed
    // ability with three uses (DelayedAbility::uses), expiring this turn.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::WhenYouPlayAUnit;
        da.expires_on_turn = ctx.state.turn.turn_number;   // "this turn"
        da.uses = 3;
        ctx.state.delayed_abilities.push_back(da);
        ctx.events.logTrace("FANFARE: the next 3 units played this turn draw 1");
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("FANFARE: played a unit -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1027;
        d.def_id = R"RB(rad-047-167)RB";
        d.name = R"RB(Fanfare)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-047/167)RB";
        d.collector_number = 47;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.ability_text = R"RB(The next three times you play a unit this turn, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/75a3d8fbfd8caeabfd5be38513a6624c61f5230f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1027(CardRegistry& r) {
    r.registerCard(1027, std::make_unique<Fanfare>());
}

}  // namespace riftbound
