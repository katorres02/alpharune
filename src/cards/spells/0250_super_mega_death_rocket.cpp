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

class SuperMegaDeathRocket : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty()) {
            ctx.executor.dealDamage(targets[0], 5, ctx.source);
            if (ctx.state.objectExists(targets[0]) &&
                ctx.state.getObject(targets[0]).hasLethalDamage()) {
                ctx.executor.killObject(targets[0]);
            }
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }

    // "When you conquer, you may discard 1 to return this from your trash to
    // your hand." Fires from the trash (TriggerManager: WhenYouConquerFromTrash).
    // The "you may" and the discard (a cost) are decided at finalization.
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerFromTrash; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto inMyTrash = [&]() {
            const auto& t = ctx.state.player(ctx.controller).trash;
            return std::find(t.begin(), t.end(), ctx.source) != t.end();
        };
        int go = confirmOptional(ctx, "Super Mega Death Rocket!: discard 1 to return this to hand?",
            [&]() { return inMyTrash() && !ctx.state.player(ctx.controller).hand.empty(); });
        if (go == -1) return;   // waiting on the agent
        if (go == 0) { if (finalizingChoices(ctx)) return; return; }
        if (!costAlreadyPaid(ctx)) {
            auto hand = ctx.state.player(ctx.controller).hand;
            GameObjectId d = pickTarget(ctx, "Super Mega Death Rocket!: discard which card", hand);
            if (d == kInvalidId && ctx.executor.hasPendingChoice()) return;  // suspended
            if (d == kInvalidId) { if (finalizingChoices(ctx)) return; return; }  // can't pay
            if (costDueNow(ctx)) ctx.executor.applyDiscard(ctx.controller, d);
        }
        if (finalizingChoices(ctx)) return;   // choices and the cost end here (CR 383.3)
        if (!inMyTrash()) return;
        auto& ps = ctx.state.player(ctx.controller);
        ps.trash.erase(std::remove(ps.trash.begin(), ps.trash.end(), ctx.source), ps.trash.end());
        auto& self = ctx.state.getObject(ctx.source);
        self.zone = ZoneType::Hand;
        self.location = std::nullopt;
        ps.hand.push_back(ctx.source);
        ctx.events.logTrace("SUPER MEGA DEATH ROCKET!: returned from trash to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 250;
        d.def_id = R"RB(ogn-252-298)RB";
        d.name = R"RB(Super Mega Death Rocket!)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-252/298)RB";
        d.collector_number = 252;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Jinx)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Deal 5 to a unit.
When you conquer, you may discard 1 to return this from your trash to your hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/95d6fd4d7944e759cfe7ee5e208fa329b719333b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_250(CardRegistry& r) {
    r.registerCard(250, std::make_unique<SuperMegaDeathRocket>());
}

} // namespace riftbound
