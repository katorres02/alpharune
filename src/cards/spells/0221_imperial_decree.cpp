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

class ImperialDecree : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "When any unit takes damage this turn, kill it." Creates a delayed
    // triggered ability that lasts until the end of this turn and triggers for
    // every instance of damage (TriggerManager: WhenAUnitTakesDamage, the
    // damaged unit is the subject).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::WhenAUnitTakesDamage;
        da.expires_on_turn = ctx.state.turn.turn_number;
        da.repeats = true;
        ctx.state.delayed_abilities.push_back(da);
        ctx.events.logTrace("IMPERIAL DECREE: units that take damage this turn are killed");
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        GameObjectId u = ctx.state.chain.resuming->triggering_subject;
        if (u == kInvalidId || !ctx.state.objectExists(u)) return;
        const auto& obj = ctx.state.getObject(u);
        if (!obj.isUnit() || !obj.location.has_value()) return;
        ctx.executor.killObject(u);
        ctx.events.logTrace("IMPERIAL DECREE: " + obj.name + " took damage -> killed");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 221;
        d.def_id = R"RB(ogn-221-298)RB";
        d.name = R"RB(Imperial Decree)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-221/298)RB";
        d.collector_number = 221;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
When any unit takes damage this turn, kill it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fc6ab617f1ce20c82ed76dc6446d42d416c08b34-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_221(CardRegistry& r) {
    r.registerCard(221, std::make_unique<ImperialDecree>());
}

} // namespace riftbound
