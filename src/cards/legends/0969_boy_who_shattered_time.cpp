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

class BoyWhoShatteredTime : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Empower] — [1], [E]"
    EmpowerSpec empowerSpec() const override {
        EmpowerSpec s = empowerCost(1);
        s.cost.exhaust = true;
        return s;
    }
    // "Disempower me, [E]: Reduce the cost of the next unit you play this turn
    // by [2][A] and give it "Kill me at end of turn.""
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        c.disempower_self = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerState::CostModifier m;
        m.source = ctx.source;
        m.energy_reduction = 2;
        m.power_reduction = 1;
        m.next_unit_only = true;
        m.this_turn_only = true;
        ctx.state.player(ctx.controller).cost_modifiers.push_back(m);
        // The same next unit gets "Kill me at end of turn": a delayed ability
        // that fires when that unit is played (its subject).
        addDelayed(ctx, TriggerType::WhenYouPlayAUnit);
        ctx.events.logTrace("BOY WHO SHATTERED TIME: next unit costs [2][A] less, dies at end of turn");
    }
    // Delayed parts: "When you play it" (mark it) and "at end of turn" (kill it).
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        if (ctx.firing_trigger == TriggerType::WhenYouPlayAUnit) {
            const auto& ri = ctx.state.chain.resuming;
            const GameObjectId unit = ri.has_value() ? ri->triggering_subject : kInvalidId;
            if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
            const bool armed = !me.tracked_objects.empty();
            me.tracked_objects.push_back(unit);
            if (!armed) addDelayed(ctx, TriggerType::AtEndOfTurn);
            ctx.events.logTrace("BOY WHO SHATTERED TIME: " + ctx.state.getObject(unit).name +
                                " has \"Kill me at end of turn\"");
            return;
        }
        if (ctx.firing_trigger == TriggerType::AtEndOfTurn) {
            auto units = std::move(me.tracked_objects);
            me.tracked_objects.clear();
            for (auto u : units) {
                if (!ctx.state.objectExists(u)) continue;
                const auto& o = ctx.state.getObject(u);
                if (!o.isUnit() || !o.location.has_value()) continue;
                ctx.executor.killObject(u);
            }
        }
    }
private:
    void addDelayed(CardContext& ctx, TriggerType t) const {
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = t;
        da.expires_on_turn = ctx.state.turn.turn_number;
        ctx.state.delayed_abilities.push_back(da);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 969;
        d.def_id = R"RB(rad-139-167)RB";
        d.name = R"RB(Boy Who Shattered Time)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-139/167)RB";
        d.collector_number = 139;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury,Domain::Mind};
        d.tags = {R"RB(Ekko)RB"};
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] — [1], [E] (Use only if not Empowered.)
Disempower me, [E]: Reduce the cost of the next unit you play this turn by [2][A] and give it "Kill me at end of turn.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8ca3ef446631784ce1d261e30f6a163843ffcb2b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_969(CardRegistry& r) {
    r.registerCard(969, std::make_unique<BoyWhoShatteredTime>());
}

}  // namespace riftbound
