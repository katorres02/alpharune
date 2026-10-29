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

// Bomb gear token (CR 187.12): domainless, "[Deploy]" and
// "[Deathknell][>] Deal 2 to an enemy unit here and kill all other Bombs you
// control here." Created with createBombToken (card_helpers.h).
class BombToken : public DeployGear {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::optional<BattlefieldId> here;
        if (ctx.state.objectExists(ctx.source)) {
            const auto& me = ctx.state.getObject(ctx.source);
            if (me.last_location && std::holds_alternative<BattlefieldLocation>(*me.last_location))
                here = std::get<BattlefieldLocation>(*me.last_location).id;
        }
        const PlayerId mine = ctx.controller;
        std::vector<GameObjectId> enemies;
        if (here) {
            const BattlefieldId bf = *here;
            enemies = chooseableUnits(ctx.state, mine, [bf, mine](const GameObject& o) {
                auto obf = o.battlefieldId();
                return o.controller != mine && obf && *obf == bf;
            });
        }
        GameObjectId t = pickTarget(ctx, "Bomb: deal 2 to an enemy unit here", enemies);
        if (finalizingChoices(ctx)) return;
        if (t != kInvalidId && ctx.state.objectExists(t))
            ctx.executor.dealDamage(t, 2, ctx.source);
        if (!here) return;
        std::vector<GameObjectId> others;
        for (const auto& [id, o] : ctx.state.objects) {
            if (id == ctx.source || !o.isGear() || o.controller != mine) continue;
            if (o.card_def_id != kBombTokenCardDefId) continue;
            auto obf = o.battlefieldId();
            if (obf && *obf == *here) others.push_back(id);
        }
        for (auto id : others) ctx.executor.killObject(id);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = kBombTokenCardDefId;
        d.def_id = R"RB(rad-token-bomb)RB";
        d.name = R"RB(Bomb)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::Token;
        d.tags = {R"RB(Bomb)RB"};
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deploy] (This may be played or created to any battlefield. When an opponent holds here, kill this.)
[Deathknell][>] Deal 2 to an enemy unit here and kill all other Bombs you control here.)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_985(CardRegistry& r) {
    r.registerCard(kBombTokenCardDefId, std::make_unique<BombToken>());
}

}  // namespace riftbound
