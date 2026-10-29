#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class NoxianGuillotine : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a unit. Kill it the next time it takes damage this turn.
    //  [Legion] — Kill it now instead."
    // Without Legion: a one-shot delayed ability scoped to the unit
    // (TriggerManager: WhenAUnitTakesDamage), expiring at end of turn.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        GameObjectId tgt = targets[0];
        if (!ctx.state.getObject(tgt).location.has_value()) return;
        // Legion = the controller has played another card this turn (this spell
        // already incremented the counter, so >= 2 means Legion is active).
        bool legion = ctx.state.player(ctx.controller).cards_played_this_turn >= 2;
        if (legion) {
            ctx.executor.killObject(tgt);          // "Kill it now instead."
            ctx.events.logTrace("NOXIAN GUILLOTINE: [Legion] kill now");
            return;
        }
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::WhenAUnitTakesDamage;
        da.expires_on_turn = ctx.state.turn.turn_number;
        da.target_filter = tgt;
        ctx.state.delayed_abilities.push_back(da);
        ctx.events.logTrace("NOXIAN GUILLOTINE: kill it the next time it takes damage this turn");
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        GameObjectId u = ctx.state.chain.resuming->triggering_subject;
        if (u == kInvalidId || !ctx.state.objectExists(u)) return;
        if (!ctx.state.getObject(u).location.has_value()) return;
        ctx.executor.killObject(u);
        ctx.events.logTrace("NOXIAN GUILLOTINE: it took damage -> killed");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // NOTE: the base spell works without Legion (Legion only UPGRADES it to
    // an immediate kill), so this is NOT a Legion-gated play.
    bool requiresLegion() const override { return false; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 251;
        d.def_id = R"RB(ogn-254-298)RB";
        d.name = R"RB(Noxian Guillotine)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-254/298)RB";
        d.collector_number = 254;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Order};
        d.tags = {R"RB(Darius)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose a unit. Kill it the next time it takes damage this turn.
[Legion] — Kill it now instead. (Get the effect if you've played another card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/57f646b0393b58b657cfabf66357d9fc4a34e046-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_251(CardRegistry& r) {
    r.registerCard(251, std::make_unique<NoxianGuillotine>());
}

} // namespace riftbound
