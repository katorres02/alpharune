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

class HunkerDown : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    // "Ready a friendly unit. It can't move this turn. Draw 1."
    // GameObject::forbidMovesThisTurn blocks its standard move and effect
    // moves this turn.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty() && ctx.state.objectExists(targets[0]) &&
            ctx.state.getObject(targets[0]).location.has_value()) {
            ctx.executor.readyObject(targets[0]);
            ctx.state.getObject(targets[0]).forbidMovesThisTurn(ctx.state.turn.turn_number);
        }
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 962;
        d.def_id = R"RB(rad-034-167)RB";
        d.name = R"RB(Hunker Down)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-034/167)RB";
        d.collector_number = 34;
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB([Reaction] Ready a friendly unit. It can't move this turn. Draw 1.)RB";
        d.energy_cost = 1;
        d.power_cost = 0;
        d.keywords = []{KeywordSet k; k.set(Keyword::Reaction); return k;}();
        return d;
    }();
};

}  // anonymous namespace

void register_card_962(CardRegistry& r) {
    r.registerCard(962, std::make_unique<HunkerDown>());
}

}  // namespace riftbound
