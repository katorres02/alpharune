#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// "[Accelerate] [Assault 3]
//  Once each turn, if I would die, you may kill a friendly unit to heal me,
//  exhaust me, and recall me instead."
// A replacement effect (Card::replaceOwnDeath, asked by both kill paths). The
// replacement can't suspend the chain, so its yes/no and the unit to kill are
// asked synchronously (EffectExecutor::confirmNow / chooseObjectNow).
class MordekaiserTwiceSlain : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool replaceOwnDeath(CardContext& ctx) override {
        const GameObjectId self = ctx.source;
        if (!ctx.state.objectExists(self)) return false;
        auto& me = ctx.state.getObject(self);
        if (!me.location.has_value()) return false;
        const int turn = ctx.state.turn.turn_number;
        auto used = me.card_counters.find("__mordekaiser_ts_turn");
        if (used != me.card_counters.end() && used->second == turn) return false;
        std::vector<GameObjectId> fodder;
        for (const auto& [id, o] : ctx.state.objects)
            if (id != self && o.isUnit() && o.location.has_value() && o.controller == ctx.controller)
                fodder.push_back(id);
        if (fodder.empty()) return false;
        if (!ctx.executor.confirmNow(ctx.controller,
                "Mordekaiser, Twice Slain: kill a friendly unit to heal, exhaust and recall me instead",
                false))
            return false;
        const GameObjectId victim = ctx.executor.chooseObjectNow(ctx.controller, fodder);
        if (victim == kInvalidId || !ctx.state.objectExists(victim)) return false;
        ctx.state.getObject(self).card_counters["__mordekaiser_ts_turn"] = turn;
        ctx.executor.killObject(victim);
        if (!ctx.state.objectExists(self)) return true;
        auto& m = ctx.state.getObject(self);
        // Heal, exhaust, recall (the attached gear comes along, CR 719.3.a).
        ctx.executor.healObject(self);
        m.damage_marked = 0;
        if (!m.is_exhausted) ctx.executor.exhaustObject(self);
        m.combat_designation = CombatDesignation::None;
        if (!m.isAtBase()) {
            const auto from = m.location.value_or(LocationId{BaseLocation{ctx.controller}});
            m.location = BaseLocation{ctx.controller};
            m.zone = ZoneType::Base;
            for (auto g : m.attachments)
                if (ctx.state.objectExists(g)) {
                    ctx.state.getObject(g).location = BaseLocation{ctx.controller};
                    ctx.state.getObject(g).zone = ZoneType::Base;
                }
            ctx.events.emit(UnitMovedEvent{self, ctx.controller, from,
                                           BaseLocation{ctx.controller}, false});
        }
        m.recomputeMight();
        ctx.events.logTrace("MORDEKAISER, TWICE SLAIN: killed a friendly unit; healed, exhausted, recalled instead of dying");
        return true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1005;
        d.def_id = R"RB(rad-020-167)RB";
        d.name = R"RB(Mordekaiser, Twice Slain)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-020/167)RB";
        d.collector_number = 20;
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Mordekaiser)RB", R"RB(Noxus)RB", R"RB(Spirit)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 5;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Assault);
        d.assault_value = 3;
        d.ability_text = R"RB([Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.)
[Assault 3] (+3 [M] while I'm an attacker.)
Once each turn, if I would die, you may kill a friendly unit to heal me, exhaust me, and recall me instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/66db9510de566ac43157ecdcc96f33b5f553a38d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1005(CardRegistry& r) {
    r.registerCard(1005, std::make_unique<MordekaiserTwiceSlain>());
}

}  // namespace riftbound
