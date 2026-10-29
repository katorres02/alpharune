#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When you play me, deal 1 to each of up to 2 units." The number of units
// (0-2) and the units are chosen when the ability is put on the chain
// (CR 383.3): first how many, then the units.
class BilgewaterTracker : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId me = ctx.controller;
        auto units = chooseableUnits(ctx.state, me, [](const GameObject&) { return true; });
        uint32_t legal = 0x1u;                           // none
        if (!units.empty()) legal |= 0x2u;              // one
        if (units.size() >= 2) legal |= 0x4u;           // two
        const int n = pickMode(ctx, "Bilgewater Tracker: deal 1 to how many units", 3,
                               {"No units", "One unit", "Two units"}, legal);
        if (n == -1) return;
        GameObjectId a = kInvalidId, b = kInvalidId;
        if (n == 1) {
            a = pickTarget(ctx, "Bilgewater Tracker: deal 1 to a unit", units);
            if (a == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) return;
        } else if (n == 2) {
            auto pr = pickTargetPair(ctx, "Bilgewater Tracker: deal 1 to each of two units", units,
                [units](GameObjectId first) {
                    std::vector<GameObjectId> rest;
                    for (auto u : units) if (u != first) rest.push_back(u);
                    return rest;
                });
            if (ctx.executor.hasPendingChoice()) return;
            a = pr.first; b = pr.second;
        }
        if (finalizingChoices(ctx)) return;
        for (auto t : {a, b})
            if (t != kInvalidId && ctx.state.objectExists(t) && ctx.state.getObject(t).location.has_value())
                ctx.executor.dealDamage(t, 1, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 988;
        d.def_id = R"RB(rad-003-167)RB";
        d.name = R"RB(Bilgewater Tracker)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-003/167)RB";
        d.collector_number = 3;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Bilgewater)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.ability_text = R"RB(When you play me, deal 1 to each of up to 2 units.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b660759a6bcb9c7ca41ea969cf11463259e284c9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_988(CardRegistry& r) {
    r.registerCard(988, std::make_unique<BilgewaterTracker>());
}

}  // namespace riftbound
