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
#include <vector>

namespace riftbound {
namespace {

class Rakelstake : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // Gear is played at base in practice, so only units trigger this.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        std::string key = std::string("__rk_turn_p") + std::to_string(static_cast<int>(ctx.controller));
        if (me.card_counters[key] == ctx.state.turn.turn_number) return;
        me.card_counters[key] = ctx.state.turn.turn_number;
        ctx.events.logTrace("RAKELSTAKE: predict 1");
        ctx.executor.predict(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 968;
        d.def_id = R"RB(rad-165-167)RB";
        d.name = R"RB(Rakelstake)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-165/167)RB";
        d.collector_number = 165;
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(For each player, the first time they play a unit or gear here each turn, they [Predict].)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_968(CardRegistry& r) {
    r.registerCard(968, std::make_unique<Rakelstake>());
}

}  // namespace riftbound
