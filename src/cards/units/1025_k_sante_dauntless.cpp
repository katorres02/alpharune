#include "cards/card.h"
#include "cards/card_helpers.h"
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

// "[Shield]. When I attack or defend, double the [Shield] of a unit you
//  control here this turn."
// Doubling gives the unit [Shield X] this turn, X = its Shield value as the
// ability resolves (printed + this-turn + continuous grants).
class KSanteDauntless : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> mine_here;
        if (ctx.state.objectExists(ctx.source)) {
            const auto& me = ctx.state.getObject(ctx.source);
            if (me.location.has_value()) {
                const LocationId loc = *me.location;
                const PlayerId p = ctx.controller;
                mine_here = chooseableUnits(ctx.state, p, [&loc, p](const GameObject& o) {
                    return o.controller == p && o.location.has_value() && *o.location == loc;
                });
            }
        }
        GameObjectId t = pickTarget(ctx, "K'Sante, Dauntless: double a unit's [Shield]", mine_here);
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        const int x = shieldValue(ctx.state.getObject(t));
        if (x <= 0) return;
        ctx.executor.giveTemporaryKeyword(t, Keyword::Shield, x);
        ctx.events.logTrace("K'SANTE, DAUNTLESS: doubled " + ctx.state.getObject(t).name +
                            "'s [Shield] (+" + std::to_string(x) + ")");
    }
private:
    static int shieldValue(const GameObject& o) {
        int v = o.shield_value;
        if (v <= 0 && o.keywords.has(Keyword::Shield)) v = 1;
        for (const auto& ae : o.aura_effects)
            if (ae.keyword == Keyword::Shield) v += std::max(1, ae.keyword_value);
        return v;
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1025;
        d.def_id = R"RB(rad-045-167)RB";
        d.name = R"RB(K'Sante, Dauntless)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-045/167)RB";
        d.collector_number = 45;
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Shurima)RB", R"RB(K'Sante)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.keywords.set(Keyword::Shield);
        d.shield_value = 1;
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.)
When I attack or defend, double the [Shield] of a unit you control here this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f56e9f9db199f915e32ea264b7dab70202f97957-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1025(CardRegistry& r) {
    r.registerCard(1025, std::make_unique<KSanteDauntless>());
}

}  // namespace riftbound
