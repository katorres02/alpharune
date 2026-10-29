#include "engine/keyword_abilities.h"

#include "cards/card_helpers.h"
#include "core/events.h"
#include "engine/effect_executor.h"

namespace riftbound {
namespace {

const ChainItem* resolvingItem(const CardContext& ctx) {
    return ctx.state.chain.resuming.has_value() ? &*ctx.state.chain.resuming : nullptr;
}

// CR 831.1.d: "When I attack, give an enemy unit here -X [M] this turn."
// X is the summed Disarm Value captured when the ability triggered.
class DisarmAbility : public Card {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const ChainItem* item = resolvingItem(ctx);
        const int x = item ? item->keyword_value : 0;
        std::optional<BattlefieldId> here;
        if (ctx.state.objectExists(ctx.source)) {
            const auto& src = ctx.state.getObject(ctx.source);
            here = src.battlefieldId();
            if (!here && src.last_location &&
                std::holds_alternative<BattlefieldLocation>(*src.last_location))
                here = std::get<BattlefieldLocation>(*src.last_location).id;
        }
        const PlayerId me = ctx.controller;
        std::vector<GameObjectId> legal;
        if (here) {
            const BattlefieldId bf = *here;
            legal = chooseableUnits(ctx.state, me, [bf, me](const GameObject& o) {
                auto obf = o.battlefieldId();
                return o.controller != me && obf && *obf == bf;
            });
        }
        GameObjectId t = pickTarget(ctx, "[Disarm " + std::to_string(x) + "]", legal);
        if (finalizingChoices(ctx)) return;
        if (x <= 0 || t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.giveTemporaryMight(t, -x);
        ctx.events.logTrace("DISARM: " + ctx.state.getObject(t).name + " -" +
                            std::to_string(x) + " [M] this turn");
    }
private:
    const CardDef def_ = [] { CardDef d; d.name = "Disarm"; return d; }();
};

// CR 830.1.b: "When an opponent holds here, kill this."
class DeployHoldKillAbility : public Card {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& g = ctx.state.getObject(ctx.source);
        if (!g.location.has_value()) return;   // already gone
        ctx.events.logTrace("DEPLOY: an opponent held here, kill " + g.name);
        ctx.executor.killObject(ctx.source);
    }
private:
    const CardDef def_ = [] { CardDef d; d.name = "Deploy"; return d; }();
};

}  // namespace

Card* keywordAbilityCard(KeywordAbility kind) {
    static DisarmAbility disarm;
    static DeployHoldKillAbility deploy_kill;
    switch (kind) {
        case KeywordAbility::Disarm:         return &disarm;
        case KeywordAbility::DeployHoldKill: return &deploy_kill;
        case KeywordAbility::None:           break;
    }
    return nullptr;
}

int totalDisarmValue(const GameState& /*state*/, const Card* card, const GameObject& unit) {
    const int printed = card ? card->disarmValue() : 0;
    return printed + unit.aura_disarm_value + unit.temp_disarm_value;
}

}  // namespace riftbound
