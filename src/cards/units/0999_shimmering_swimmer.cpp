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

constexpr const char* kSwimmerPaid = "__swimmer_paid_kill";

// "You may kill a friendly unit or gear as an additional cost to play me.
//  When you play me, if you paid the additional cost, deal 2 to a unit."
// The additional cost is offered during the play itself (onPlay, CR 355.1 —
// the same synchronous decision point the engine uses for Accelerate and the
// other optional additional costs); the play trigger reads the paid flag.
class ShimmeringSwimmer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onPlay(CardContext& ctx) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.state.getObject(ctx.source).card_counters[kSwimmerPaid] = 0;
        std::vector<GameObjectId> fodder;
        for (const auto& [id, o] : ctx.state.objects) {
            if (id == ctx.source || o.controller != ctx.controller) continue;
            if (!o.location.has_value() || !(o.isUnit() || o.isGear())) continue;
            fodder.push_back(id);
        }
        if (fodder.empty()) return;
        if (!ctx.executor.confirmNow(ctx.controller,
                "Shimmering Swimmer: kill a friendly unit or gear as an additional cost", false))
            return;
        const GameObjectId victim = ctx.executor.chooseObjectNow(ctx.controller, fodder);
        if (victim == kInvalidId || !ctx.state.objectExists(victim)) return;
        ctx.executor.killObject(victim);
        ctx.state.getObject(ctx.source).card_counters[kSwimmerPaid] = 1;
        ctx.events.logTrace("SHIMMERING SWIMMER: paid the additional cost");
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        auto it = me.card_counters.find(kSwimmerPaid);
        const bool paid = it != me.card_counters.end() && it->second == 1;
        std::vector<GameObjectId> units;
        if (paid) units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        GameObjectId t = pickTarget(ctx, "Shimmering Swimmer: deal 2 to a unit", units);
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;
        if (!paid || t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.dealDamage(t, 2, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 999;
        d.def_id = R"RB(rad-014-167)RB";
        d.name = R"RB(Shimmering Swimmer)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-014/167)RB";
        d.collector_number = 14;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Bilgewater)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.ability_text = R"RB(You may kill a friendly unit or gear as an additional cost to play me.
When you play me, if you paid the additional cost, deal 2 to a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/02ef2a5c93cf60048bb932a709d3a5b17b93bb1d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_999(CardRegistry& r) {
    r.registerCard(999, std::make_unique<ShimmeringSwimmer>());
}

}  // namespace riftbound
