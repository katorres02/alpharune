#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When you play me or when I attack, you may discard 1 to kill a unit or
// gear." "Discard 1" is the ability's cost (CR 383.3.b): whether to pay it,
// the unit or gear to kill and the card to discard are all decided as the
// ability is finalized; the kill happens on resolution (a target gone by then
// is a mistarget).
class MordekaiserDominating : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIAttack};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ps = ctx.state.player(ctx.controller);
        const bool paid = costAlreadyPaid(ctx);
        const int go = confirmOptional(ctx, "Mordekaiser, Dominating: discard 1 to kill a unit or gear?",
            [&]() { return (paid || !ps.hand.empty()) && !killable(ctx.state, ctx.controller).empty(); });
        if (go <= 0) return;
        const GameObjectId target = pickTarget(ctx, "Mordekaiser, Dominating: kill a unit or gear",
                                               killable(ctx.state, ctx.controller));
        if (ctx.executor.hasPendingChoice()) return;
        if (costDueNow(ctx)) {
            GameObjectId discard = kInvalidId;
            if (!stepChooseObject(ctx, ctx.controller, ps.hand,
                                  "Mordekaiser, Dominating: discard 1", 20, 10, discard))
                return;
            if (discard == kInvalidId) return;   // nothing to discard: can't pay
            ctx.executor.applyDiscard(ctx.controller, discard);
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (target == kInvalidId || !ctx.state.objectExists(target)) return;
        if (!ctx.state.getObject(target).location.has_value()) return;
        ctx.events.logTrace("MORDEKAISER, DOMINATING: kill " + ctx.state.getObject(target).name);
        ctx.executor.killObject(target);
    }

private:
    // Units and gear on the board this player may choose.
    static std::vector<GameObjectId> killable(const GameState& state, PlayerId me) {
        std::vector<GameObjectId> out;
        for (const auto& [id, o] : state.objects) {
            if (!o.location.has_value() || (!o.isUnit() && !o.isGear())) continue;
            if (o.controller != me && o.untargetable_by_enemy) continue;
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    const CardDef def_ = [] {
        CardDef d;
        d.id = 1113;
        d.def_id = R"RB(rad-137-167)RB";
        d.name = R"RB(Mordekaiser, Dominating)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-137/167)RB";
        d.collector_number = 137;
        d.artist = R"RB(JunHuan)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Mordekaiser)RB", R"RB(Noxus)RB", R"RB(Spirit)RB"};   // printed: + Spirit
        d.rarity = Rarity::Epic;
        d.energy_cost = 9;
        d.power_cost = 2;
        d.might = 8;
        d.ability_text = R"RB(When you play me or when I attack, you may discard 1 to kill a unit or gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/43068166258356908b364566abcfc57123e96caa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1113(CardRegistry& r) {
    r.registerCard(1113, std::make_unique<MordekaiserDominating>());
}

}  // namespace riftbound
