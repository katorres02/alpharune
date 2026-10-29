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

class NtofoStrikes : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Give a friendly unit [Shield 2] this turn, then deal damage equal to its
    // [Shield] to an enemy unit."
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool needsPlayTimeTargetPair() const override { return true; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId me = ctx.controller;
        auto friends = chooseableUnits(ctx.state, me, [me](const GameObject& o) {
            return o.controller == me;
        });
        auto enemies_fn = [&](GameObjectId) {
            return chooseableUnits(ctx.state, me, [me](const GameObject& o) {
                return o.controller != me;
            });
        };
        auto [friendly, enemy] = pickTargetPair(
            ctx, "Ntofo Strikes: a friendly unit, then an enemy unit", friends, enemies_fn);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (ctx.executor.hasPendingChoice()) return;
        if (friendly == kInvalidId || !ctx.state.objectExists(friendly)) return;
        ctx.executor.giveTemporaryKeyword(friendly, Keyword::Shield, 2);
        if (enemy == kInvalidId || !ctx.state.objectExists(enemy)) return;
        const int dmg = shieldOf(ctx.state.getObject(friendly));
        if (dmg <= 0) return;
        ctx.executor.dealDamage(enemy, dmg, ctx.source);
        if (ctx.state.objectExists(enemy) && ctx.state.getObject(enemy).location.has_value() &&
            ctx.state.getObject(enemy).hasLethalDamage())
            ctx.executor.killObject(enemy);
        ctx.events.logTrace("NTOFO STRIKES: " + std::to_string(dmg) + " damage");
    }
private:
    // [Shield] instances: printed + "this turn" (shield_value) plus continuous grants.
    static int shieldOf(const GameObject& u) {
        int n = u.shield_value;
        for (const auto& ae : u.aura_effects)
            if (ae.keyword == Keyword::Shield) n += ae.keyword_value > 0 ? ae.keyword_value : 1;
        return n;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 978;
        d.def_id = R"RB(rad-148-167)RB";
        d.name = R"RB(Ntofo Strikes)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-148/167)RB";
        d.collector_number = 148;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm,Domain::Body};
        d.tags = {R"RB(K'Sante)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Give a friendly unit [Shield 2] this turn, then deal damage equal to its [Shield] to an enemy unit. (A unit with Shield has +1 [M] for each instance of Shield while it's a defender.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ab5aa7d29bd49efe2c9da3b62a8065295c189991-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_978(CardRegistry& r) {
    r.registerCard(978, std::make_unique<NtofoStrikes>());
}

}  // namespace riftbound
