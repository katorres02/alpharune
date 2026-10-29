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

class RealmOfDeath : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Give an enemy unit -2 [M] this turn and a friendly unit +2 [M] this
    // turn. They deal damage equal to their Mights to each other."
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool needsPlayTimeTargetPair() const override { return true; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId me = ctx.controller;
        auto enemies = chooseableUnits(ctx.state, me, [me](const GameObject& o) {
            return o.controller != me;
        });
        auto friends_fn = [&](GameObjectId) {
            return chooseableUnits(ctx.state, me, [me](const GameObject& o) {
                return o.controller == me;
            });
        };
        auto [enemy, friendly] = pickTargetPair(
            ctx, "Realm of Death: an enemy unit, then a friendly unit", enemies, friends_fn);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (ctx.executor.hasPendingChoice()) return;

        const bool e_ok = enemy != kInvalidId && ctx.state.objectExists(enemy);
        const bool f_ok = friendly != kInvalidId && ctx.state.objectExists(friendly);
        if (e_ok) ctx.executor.giveTemporaryMight(enemy, -2);
        if (f_ok) ctx.executor.giveTemporaryMight(friendly, 2);
        if (!e_ok || !f_ok) return;
        const auto& eo = ctx.state.getObject(enemy);
        const auto& fo = ctx.state.getObject(friendly);
        if (!eo.location.has_value() || !fo.location.has_value()) return;
        // Simultaneous: read both Mights first.
        const int em = eo.current_might;
        const int fm = fo.current_might;
        ctx.executor.dealDamage(enemy, fm, friendly);
        ctx.executor.dealDamage(friendly, em, enemy);
        for (auto id : {enemy, friendly})
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).location.has_value() &&
                ctx.state.getObject(id).hasLethalDamage())
                ctx.executor.killObject(id);
        ctx.events.logTrace("REALM OF DEATH: " + std::to_string(fm) + " vs " + std::to_string(em));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 974;
        d.def_id = R"RB(rad-144-167)RB";
        d.name = R"RB(Realm of Death)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-144/167)RB";
        d.collector_number = 144;
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury,Domain::Order};
        d.tags = {R"RB(Mordekaiser)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 2;
        d.power_cost = 1;
        d.ability_text = R"RB(Give an enemy unit -2 [M] this turn and a friendly unit +2 [M] this turn. They deal damage equal to their Mights to each other.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fd3b4c6939b6a80b33956da7e3a9102eba9c32a8-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_974(CardRegistry& r) {
    r.registerCard(974, std::make_unique<RealmOfDeath>());
}

}  // namespace riftbound
