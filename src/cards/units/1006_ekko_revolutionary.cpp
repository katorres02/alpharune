#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// "I enter ready.
//  [Assault]
//  [Action][>] Discard 1, [1][R]: Banish me, then play me to your base,
//  ignoring my cost."
// Banishing makes me a new object; I am played again from banishment (a play,
// so my play triggers and "when you play a unit" effects see it) and, by my
// first ability, I enter ready.
class EkkoRevolutionary : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool entersReadyOnPlay() const override { return true; }
    bool hasActivatedAbility() const override { return true; }
    bool isActionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.energy = 1, .power = 1, .power_domain = Domain::Fury,
                              .discard = true, .discard_count = 1};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const GameObjectId self = ctx.source;
        if (!ctx.state.objectExists(self)) return;
        if (!ctx.state.getObject(self).location.has_value()) return;
        ctx.executor.banishObject(self);
        auto& me = ctx.state.getObject(self);
        if (me.zone != ZoneType::Banishment) return;
        auto& ban = ctx.state.player(me.owner).banishment;
        ban.erase(std::remove(ban.begin(), ban.end(), self), ban.end());
        // A new object: no damage, buffs, statuses or "this turn" effects.
        me.damage_marked = 0;
        me.buff_count = 0;
        me.temp_might_bonus = 0;
        me.is_stunned = false;
        me.combat_designation = CombatDesignation::None;
        ctx.executor.playIgnoringCost(ctx.controller, self, LocationId{BaseLocation{ctx.controller}});
        if (ctx.state.objectExists(self)) {
            auto& e = ctx.state.getObject(self);
            if (e.location.has_value()) e.is_exhausted = false;   // "I enter ready."
        }
        ctx.events.logTrace("EKKO, REVOLUTIONARY: banished and played again to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1006;
        d.def_id = R"RB(rad-021-167)RB";
        d.name = R"RB(Ekko, Revolutionary)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-021/167)RB";
        d.collector_number = 21;
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Ekko)RB", R"RB(Zaun)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.keywords.set(Keyword::Assault);
        d.assault_value = 1;
        d.ability_text = R"RB(I enter ready.
[Assault] (+1 [M] while I'm an attacker.)
[Action][>] Discard 1, [1][R]: Banish me, then play me to your base, ignoring my cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d13964c46506f9f36430dfe91ee02d258a304908-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1006(CardRegistry& r) {
    r.registerCard(1006, std::make_unique<EkkoRevolutionary>());
}

}  // namespace riftbound
