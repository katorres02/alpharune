#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
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

class ResourceExtractor : public DeployGear {
public:
    const CardDef& def() const override { return def_; }

    // "[Deploy]" (DeployGear). "At the end of your turn, if you control this
    // battlefield, play a Gold gear token exhausted."
    static bool controlsHere(const GameState& state, GameObjectId self, PlayerId p) {
        if (!state.objectExists(self)) return false;
        auto bf = state.getObject(self).battlefieldId();
        if (!bf) return false;
        for (const auto& b : state.battlefields)
            if (b.id == *bf) return b.controller.has_value() && *b.controller == p;
        return false;
    }
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId) const override {
        return controlsHere(state, source, controller);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!controlsHere(ctx.state, ctx.source, ctx.controller)) return;   // re-checked (intervening if)
        createGoldExhausted(ctx);
        ctx.events.logTrace("RESOURCE EXTRACTOR: controls this battlefield -> Gold token exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1032;
        d.def_id = R"RB(rad-052-167)RB";
        d.name = R"RB(Resource Extractor)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-052/167)RB";
        d.collector_number = 52;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 1;
        d.power_cost = 0;
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
At the end of your turn, if you control this battlefield, play a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A]."))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d5cc2f85febe2260e51b88bddfeab6ce700c85dd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1032(CardRegistry& r) {
    r.registerCard(1032, std::make_unique<ResourceExtractor>());
}

}  // namespace riftbound
