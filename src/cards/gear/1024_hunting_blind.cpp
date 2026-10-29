#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// "[Deploy]. When you play this, you may move an enemy unit to this
//  battlefield. Friendly units here have [Shield 2]."
class HuntingBlind : public DeployGear {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto here = [&ctx]() -> std::optional<BattlefieldId> {
            if (!ctx.state.objectExists(ctx.source)) return std::nullopt;
            return ctx.state.getObject(ctx.source).battlefieldId();
        };
        const PlayerId mine = ctx.controller;
        auto enemies = [&]() {
            auto bf = here();
            if (!bf) return std::vector<GameObjectId>{};
            const BattlefieldId b = *bf;
            return chooseableUnits(ctx.state, mine, [b, mine](const GameObject& o) {
                auto obf = o.battlefieldId();
                return o.controller != mine && !(obf && *obf == b);
            });
        };
        int go = confirmOptional(ctx, "Hunting Blind: move an enemy unit to this battlefield?",
                                 [&]() { return !enemies().empty(); });
        if (go == -1) return;
        if (go == 0) { finalizingChoices(ctx); return; }
        GameObjectId t = pickTarget(ctx, "Hunting Blind: move an enemy unit here", enemies());
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        auto bf = here();
        if (t == kInvalidId || !ctx.state.objectExists(t) || !bf) return;
        ctx.executor.moveToBattlefield(t, *bf);
        ctx.events.logTrace("HUNTING BLIND: moved " + ctx.state.getObject(t).name + " here");
    }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject& self) {
            auto bf = self.battlefieldId();
            if (!bf) return;
            for (auto& [id, o] : state.objects) {
                if (!o.isUnit() || o.controller != controller) continue;
                auto obf = o.battlefieldId();
                if (obf && *obf == *bf) auraKeyword(o, sid, Keyword::Shield, 2);
            }
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1024;
        d.def_id = R"RB(rad-044-167)RB";
        d.name = R"RB(Hunting Blind)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-044/167)RB";
        d.collector_number = 44;
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.rarity = Rarity::Epic;
        d.energy_cost = 4;
        d.power_cost = 2;
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When you play this, you may move an enemy unit to this battlefield.
Friendly units here have [Shield 2]. (+2 [M] while they're defenders.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0a625c22d828c83302132b0465dceb3ab11a08bc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1024(CardRegistry& r) {
    r.registerCard(1024, std::make_unique<HuntingBlind>());
}

}  // namespace riftbound
