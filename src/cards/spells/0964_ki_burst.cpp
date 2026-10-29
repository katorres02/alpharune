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

class KiBurst : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "As an additional cost to play this, exhaust a friendly unit." A
    // mandatory object cost (CR 356.2.a): the player picks the ready friendly
    // unit as the spell is played, before it reaches the chain; with none the
    // spell can't be played (GameEngine::canAfford / payObjectAdditionalCost).
    OptionalAdditionalCost optionalAdditionalCost() const override {
        OptionalAdditionalCost c;
        c.valid = true;
        c.object_cost = OptionalAdditionalCost::ObjectCost::ExhaustFriendlyUnit;
        c.mandatory = true;
        c.min_objects = 1;
        c.max_objects = 1;
        return c;
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};   // "a unit": friendly or enemy
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty() && ctx.state.objectExists(targets[0])) ctx.executor.stunUnit(targets[0]);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 964;
        d.def_id = R"RB(rad-028-167)RB";
        d.name = R"RB(Ki Burst)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-028/167)RB";
        d.collector_number = 28;
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.ability_text = R"RB([Action] As an additional cost to play this, exhaust a friendly unit. [Stun] a unit.)RB";
        d.energy_cost = 1;
        d.power_cost = 0;
        d.keywords = []{KeywordSet k; k.set(Keyword::Action); return k;}();
        return d;
    }();
};

}  // anonymous namespace

void register_card_964(CardRegistry& r) {
    r.registerCard(964, std::make_unique<KiBurst>());
}

}  // namespace riftbound
