#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class DemacianStandard : public DeployGear {
public:
    const CardDef& def() const override { return def_; }
    // [Deploy] comes from DeployGear.
    // "When one or more friendly units attack here, give one of them +2 [M]
    // this turn." (fires on this gear as WhenIAttack) and "When you hold here,
    // you may return this to its owner's hand." (WhenYouHoldHere).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenIAttack, TriggerType::WhenYouHoldHere};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.firing_trigger == TriggerType::WhenYouHoldHere) {
            const int go = confirmOptional(ctx, "Demacian Standard: return this to its owner's hand?",
                [&]() {
                    return ctx.state.objectExists(ctx.source) &&
                           ctx.state.getObject(ctx.source).location.has_value();
                });
            if (go <= 0) return;
            if (finalizingChoices(ctx)) return;
            ctx.executor.bounceToHand(ctx.source);
            return;
        }
        // WhenIAttack: one of the friendly units attacking here.
        const GameObjectId unit = pickTarget(ctx, "Demacian Standard: +2 [M] to an attacker",
                                             attackersHere(ctx.state, ctx.source, ctx.controller));
        if (finalizingChoices(ctx)) return;
        if (ctx.executor.hasPendingChoice()) return;
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
        ctx.executor.giveTemporaryMight(unit, 2);
    }
private:
    static std::vector<GameObjectId> attackersHere(const GameState& state, GameObjectId self,
                                                   PlayerId me) {
        if (!state.objectExists(self)) return {};
        const auto here = state.getObject(self).battlefieldId();
        if (!here) return {};
        const BattlefieldId bf = *here;
        auto at = chooseableUnits(state, me, [me, bf](const GameObject& o) {
            auto obf = o.battlefieldId();
            return o.controller == me && obf && *obf == bf;
        });
        std::vector<GameObjectId> attacking;
        for (auto id : at)
            if (state.getObject(id).combat_designation == CombatDesignation::Attacker)
                attacking.push_back(id);
        return attacking.empty() ? at : attacking;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 984;
        d.def_id = R"RB(rad-156-167)RB";
        d.name = R"RB(Demacian Standard)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-156/167)RB";
        d.collector_number = 156;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body,Domain::Order};
        d.tags = {R"RB(Jarvan IV)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 2;
        d.power_cost = 1;
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When one or more friendly units attack here, give one of them +2 [M] this turn.
When you hold here, you may return this to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/315b0f7901a83df38b9765f12b1edfdc7de566a3-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_984(CardRegistry& r) {
    r.registerCard(984, std::make_unique<DemacianStandard>());
}

}  // namespace riftbound
