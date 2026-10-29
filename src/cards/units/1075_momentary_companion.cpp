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

class MomentaryCompanion : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // [Hidden] is engine-handled.
    // "When you play me, if it's not your turn, give me [Temporary]."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return state.turn.turn_player != controller;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.turn.turn_player == ctx.controller) return;   // intervening "if"
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        if (!me.location.has_value()) return;
        me.keywords.set(Keyword::Temporary);
        ctx.events.logTrace("MOMENTARY COMPANION: played on an opponent's turn -> [Temporary]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1075;
        d.def_id = R"RB(rad-095-167)RB";
        d.name = R"RB(Momentary Companion)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-095/167)RB";
        d.collector_number = 95;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Demon)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 4;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When you play me, if it's not your turn, give me [Temporary]. (Kill me at the start of my controller's Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/aea67209470e47ec876a11ae9b3eacda00df7be1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1075(CardRegistry& r) {
    r.registerCard(1075, std::make_unique<MomentaryCompanion>());
}

}  // namespace riftbound
