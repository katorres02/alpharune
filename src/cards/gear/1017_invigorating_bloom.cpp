#include "cards/card.h"
#include "cards/card_helpers.h"
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

// "When you play this, give a friendly unit +3 [M] this turn.
//  Exhaust a friendly [Mighty] unit, [E]: Draw 1."
// The Mighty unit is a cost: it is picked (plain object pick, resume points
// 30-32 + resume_data[8]) and exhausted while the ability is finalized.
class InvigoratingBloom : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger | kFinalizeActivate; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId mine = ctx.controller;
        auto units = chooseableUnits(ctx.state, mine,
            [mine](const GameObject& o) { return o.controller == mine; });
        GameObjectId t = pickTarget(ctx, "Invigorating Bloom: give a friendly unit +3 [M]", units);
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.giveTemporaryMight(t, 3);
        ctx.events.logTrace("INVIGORATING BLOOM: +3 [M] to " + ctx.state.getObject(t).name);
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility a;
        a.cost.exhaust = true;
        return {a};
    }
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (readyMightyUnits(state, controller).empty()) return false;
        return Card::canActivateAbilityAt(state, controller, source, ability_index);
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>&) override {
        GameObjectId pay = pickCostUnit(ctx);
        if (ctx.executor.hasPendingChoice()) return;
        if (costDueNow(ctx)) {
            if (pay == kInvalidId || !ctx.state.objectExists(pay) ||
                ctx.state.getObject(pay).is_exhausted) return;   // cost can't be paid
            ctx.executor.exhaustObject(pay);
            ctx.events.logTrace("INVIGORATING BLOOM: exhausted " + ctx.state.getObject(pay).name +
                                " (cost)");
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    static std::vector<GameObjectId> readyMightyUnits(const GameState& state, PlayerId p) {
        std::vector<GameObjectId> out;
        for (const auto& [id, o] : state.objects)
            if (o.isUnit() && o.controller == p && o.location.has_value() && !o.is_exhausted &&
                isMighty(o))
                out.push_back(id);
        return out;
    }
    GameObjectId pickCostUnit(CardContext& ctx) const {
        auto legal = readyMightyUnits(ctx.state, ctx.controller);
        if (!ctx.state.chain.resuming.has_value())
            return legal.empty() ? kInvalidId : legal.front();
        auto& ri = *ctx.state.chain.resuming;
        while (ri.resume_data.size() < 9) ri.resume_data.push_back(0);
        if (ri.resume_point < 30) {
            if (legal.empty()) {
                ri.resume_point = 32;
                ri.resume_data[8] = static_cast<int32_t>(kInvalidId);
                return kInvalidId;
            }
            std::vector<Intent> opts;
            for (auto id : legal) opts.push_back(objChoice(ctx.controller, id));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Invigorating Bloom: exhaust a friendly Mighty unit (cost)");
            ri.resume_point = 31;
            return kInvalidId;
        }
        if (ri.resume_point == 31) {
            GameObjectId picked = takenObject(ctx);
            if (std::find(legal.begin(), legal.end(), picked) == legal.end())
                picked = legal.empty() ? kInvalidId : legal.front();
            ri.resume_data[8] = static_cast<int32_t>(picked);
            ri.resume_point = 32;
            return picked;
        }
        return static_cast<GameObjectId>(ri.resume_data[8]);
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1017;
        d.def_id = R"RB(rad-035-167)RB";
        d.name = R"RB(Invigorating Bloom)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-035/167)RB";
        d.collector_number = 35;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.ability_text = R"RB(When you play this, give a friendly unit +3 [M] this turn.
Exhaust a friendly [Mighty] unit, [E]: Draw 1. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/66d4436ef5d7a7a81c60ee9b1b3ebae06162b7d2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1017(CardRegistry& r) {
    r.registerCard(1017, std::make_unique<InvigoratingBloom>());
}

}  // namespace riftbound
