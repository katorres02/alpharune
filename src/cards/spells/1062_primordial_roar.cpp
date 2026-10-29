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
#include <variant>
#include <vector>

namespace riftbound {
namespace {

class PrimordialRoar : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Show Off] a unit." The engine asks as the spell is played.
    bool hasShowOff() const override { return true; }
    bool showOffMatches(const GameState&, PlayerId, const GameObject& candidate) const override {
        return candidate.isUnit();
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    // "If you showed off a unit, deal damage equal to that unit's Might to a unit."
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId t = pickTarget(ctx, "Primordial Roar", legal);
        if (ctx.executor.hasPendingChoice()) return;
        const GameObjectId shown = shownOff(ctx.state, ctx.source);
        if (shown == kInvalidId || !ctx.state.objectExists(shown)) return;
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        const auto& s = ctx.state.getObject(shown);
        int might = s.current_might;
        if (!s.location.has_value() && s.card_def_id != kInvalidId)
            might = ctx.executor.cardDB().get(s.card_def_id).might;   // revealed from hand
        if (might <= 0) return;
        ctx.executor.dealDamage(t, might, ctx.source);
        if (ctx.state.objectExists(t) && ctx.state.getObject(t).hasLethalDamage())
            ctx.executor.killObject(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1062;
        d.def_id = R"RB(rad-082-167)RB";
        d.name = R"RB(Primordial Roar)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-082/167)RB";
        d.collector_number = 82;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 4;
        d.power_cost = 2;
        d.ability_text = R"RB([Show Off] a unit. (As you play this, you may reveal a unit from hand or pick a friendly unit.)
If you showed off a unit, deal damage equal to that unit's Might to a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/945633b0c2ed7f303245c917ffbcdb80dcfb524e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1062(CardRegistry& r) {
    r.registerCard(1062, std::make_unique<PrimordialRoar>());
}

}  // namespace riftbound
