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

class ElnukHerd : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // Errata: "When you play me, give a friendly unit +3 [M] this turn, then
    // recycle me if I was played from face down." The face-down marker stamped
    // by GameEngine::executePlayFromHidden is latched (and consumed) the first
    // time the ability runs, and cleared once it resolves.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.state.objectExists(ctx.source)) {
            auto& c = ctx.state.getObject(ctx.source).card_counters;
            if (c["__played_from_facedown"] == 1) { c["__elnuk_fd"] = 1; c["__played_from_facedown"] = 0; }
        }
        const PlayerId mine = ctx.controller;
        auto friendly = chooseableUnits(ctx.state, mine,
            [mine](const GameObject& o) { return o.controller == mine; });
        GameObjectId t = pickTarget(ctx, "Elnuk Herd: give a friendly unit +3 [M]", friendly);
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t != kInvalidId && ctx.state.objectExists(t)) ctx.executor.giveTemporaryMight(t, 3);
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        const bool from_facedown = me.card_counters["__elnuk_fd"] == 1;
        me.card_counters["__elnuk_fd"] = 0;
        if (from_facedown && me.location.has_value()) {
            ctx.executor.recycleCards(ctx.controller, {ctx.source});
            ctx.events.logTrace("ELNUK HERD: played from face down -> recycled");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1061;
        d.def_id = R"RB(rad-081-167)RB";
        d.name = R"RB(Elnuk Herd)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-081/167)RB";
        d.collector_number = 81;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Freljord)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 6;
        d.power_cost = 0;
        d.might = 5;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When you play me, give a friendly unit +3 [M] this turn, then recycle me if I was played from face down.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1799cd4c46fcf0243b4022de2d427b15e67a15ad-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1061(CardRegistry& r) {
    r.registerCard(1061, std::make_unique<ElnukHerd>());
}

}  // namespace riftbound
