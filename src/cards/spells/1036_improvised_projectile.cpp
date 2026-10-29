#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
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

class ImprovisedProjectile : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Choose a friendly gear. Deal damage equal to its Energy cost to a unit
    // at a battlefield." Both chosen as the spell is played (CR 383.3); the
    // gear's Energy cost is read as the spell resolves.
    static std::vector<GameObjectId> friendlyGear(const GameState& state, PlayerId p) {
        std::vector<GameObjectId> out;
        for (const auto& [id, o] : state.objects)
            if (o.isGear() && o.controller == p && o.location.has_value()) out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    static std::vector<GameObjectId> bfUnits(const GameState& state, PlayerId p) {
        auto out = chooseableUnits(state, p, [](const GameObject& o) {
            return o.battlefieldId().has_value();
        });
        std::sort(out.begin(), out.end());
        return out;
    }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool needsPlayTimeTargetPair() const override { return true; }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !friendlyGear(state, player).empty() && !bfUnits(state, player).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto [gear, tgt] = pickTargetPair(ctx, "Improvised Projectile: choose a friendly gear, then a unit",
            friendlyGear(ctx.state, ctx.controller),
            [&](GameObjectId) { return bfUnits(ctx.state, ctx.controller); });
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (gear == kInvalidId || tgt == kInvalidId) return;
        if (!ctx.state.objectExists(gear) || !ctx.state.objectExists(tgt)) return;
        const auto& g = ctx.state.getObject(gear);
        // Tokens have no CardDef-backed cost: Energy cost 0.
        int dmg = 0;
        if (!g.isToken() && g.card_def_id != kInvalidId)
            dmg = ctx.executor.cardDB().get(g.card_def_id).energy_cost;
        if (dmg <= 0) return;
        ctx.executor.dealDamage(tgt, dmg, ctx.source);
        if (ctx.state.objectExists(tgt) && ctx.state.getObject(tgt).hasLethalDamage())
            ctx.executor.killObject(tgt);
        ctx.events.logTrace("IMPROVISED PROJECTILE: dealt " + std::to_string(dmg));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1036;
        d.def_id = R"RB(rad-056-167)RB";
        d.name = R"RB(Improvised Projectile)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-056/167)RB";
        d.collector_number = 56;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 3;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose a friendly gear. Deal damage equal to its Energy cost to a unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ecbb970f955b8e12369921f30ad08cab04e59a3f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1036(CardRegistry& r) {
    r.registerCard(1036, std::make_unique<ImprovisedProjectile>());
}

}  // namespace riftbound
