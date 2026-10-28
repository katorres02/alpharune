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

class RecklessInnovator : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIBecomeExhausted; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        uint32_t legal = 0x1u;
        if (payableOnePower(ctx.state, ctx.controller, Domain::Chaos)) legal |= 0x2u;
        int mode = pickMode(ctx, "Reckless Innovator", 2,
                            {"Play a Gold gear token exhausted", "Pay [P] to draw 1"}, legal);
        if (mode < 0) return;
        if (mode == 1 && costDueNow(ctx) && !payOnePower(ctx, ctx.controller, Domain::Chaos)) return;
        if (finalizingChoices(ctx)) return;
        if (mode == 0) { createGoldExhausted(ctx); ctx.events.logTrace("RECKLESS INNOVATOR: Gold token"); }
        else { ctx.executor.drawCards(ctx.controller, 1); ctx.events.logTrace("RECKLESS INNOVATOR: draw 1"); }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 961;
        d.def_id = R"RB(rad-105-167)RB";
        d.name = R"RB(Reckless Innovator)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-105/167)RB";
        d.collector_number = 105;
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB([Accelerate] When I become exhausted, choose one — Play a Gold gear token exhausted. Pay [P] to draw 1.)RB";
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.keywords = []{KeywordSet k; k.set(Keyword::Accelerate); return k;}();
        return d;
    }();
};

}  // anonymous namespace

void register_card_961(CardRegistry& r) {
    r.registerCard(961, std::make_unique<RecklessInnovator>());
}

}  // namespace riftbound
