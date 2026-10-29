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

class RivenBrokenBlade : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIBecomeExhausted; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        if (me.card_counters["__rbb_turn"] != ctx.state.turn.turn_number) {
            me.card_counters["__rbb_turn"] = ctx.state.turn.turn_number;
            me.card_counters["__rbb_mask"] = 0;
        }
        uint32_t used = static_cast<uint32_t>(me.card_counters["__rbb_mask"]);
        const PlayerId mine = ctx.controller;
        std::vector<GameObjectId> enemies = chooseableUnits(ctx.state, mine,
            [mine](const GameObject& o) { return o.controller != mine; });
        uint32_t legal = (~used) & 0x7u;
        if (me.buff_count <= 0) legal &= 0x1u;          // modes 1,2 spend my buff
        if (enemies.empty()) legal &= ~0x2u;
        int mode = pickMode(ctx, "Riven, Broken Blade", 3,
                            {"Buff me", "Spend buff: stun an enemy unit", "Spend buff: ready me"}, legal);
        if (mode < 0) return;
        ctx.state.getObject(ctx.source).card_counters["__rbb_mask"] = static_cast<int>(used | (1u << mode));
        GameObjectId tgt = kInvalidId;
        if (mode == 1) {
            tgt = pickTarget(ctx, "Riven, Broken Blade: stun an enemy unit", enemies);
            if (tgt == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) return;
        }
        if (finalizingChoices(ctx)) return;
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        switch (mode) {
        case 0:
            if (self.buff_count <= 0) ctx.executor.buffUnit(ctx.source);
            ctx.events.logTrace("RIVEN, BROKEN BLADE: buff me");
            break;
        case 1:
            if (!ctx.executor.spendBuff(ctx.source)) return;
            if (tgt != kInvalidId && ctx.state.objectExists(tgt)) ctx.executor.stunUnit(tgt);
            ctx.events.logTrace("RIVEN, BROKEN BLADE: spend buff -> stun");
            break;
        case 2:
            if (!ctx.executor.spendBuff(ctx.source)) return;
            ctx.executor.readyObject(ctx.source);
            ctx.events.logTrace("RIVEN, BROKEN BLADE: spend buff -> ready me");
            break;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 957;
        d.def_id = R"RB(rad-043-167)RB";
        d.name = R"RB(Riven, Broken Blade)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-043/167)RB";
        d.collector_number = 43;
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Riven)RB", R"RB(Noxus)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I become exhausted, choose one that hasn't been chosen this turn — [Buff] me. Spend my buff to [Stun] an enemy unit. Spend my buff to ready me.)RB";
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 4;
        return d;
    }();
};

}  // anonymous namespace

void register_card_957(CardRegistry& r) {
    r.registerCard(957, std::make_unique<RivenBrokenBlade>());
}

}  // namespace riftbound
