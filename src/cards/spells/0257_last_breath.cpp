#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class LastBreath : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Ready a friendly unit. It deals damage equal to its Might to an enemy
    // unit at a battlefield." Two targets, chosen as the spell is played.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool needsPlayTimeTargetPair() const override { return true; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.controller == controller && obj.location.has_value())
                return true;
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal_a = chooseableUnits(ctx.state, ctx.controller,
            [&](const GameObject& o) { return o.controller == ctx.controller; });
        auto [friendly, enemy] = pickTargetPair(ctx, "Last Breath: friendly unit, then enemy unit",
            legal_a, [&](GameObjectId) {
                return chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                    return o.controller != ctx.controller && o.isAtBattlefield();
                });
            });
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (friendly == kInvalidId || !ctx.state.objectExists(friendly)) return;
        ctx.executor.readyObject(friendly);
        if (enemy == kInvalidId || !ctx.state.objectExists(enemy)) return;
        int dmg = ctx.state.getObject(friendly).current_might;
        ctx.executor.dealDamage(enemy, dmg, friendly);
        if (ctx.state.objectExists(enemy) && ctx.state.getObject(enemy).location.has_value() &&
            ctx.state.getObject(enemy).hasLethalDamage())
            ctx.executor.killObject(enemy);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 257;
        d.def_id = R"RB(ogn-260-298)RB";
        d.name = R"RB(Last Breath)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-260/298)RB";
        d.collector_number = 260;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Chaos};
        d.tags = {R"RB(Yasuo)RB"};
        d.energy_cost = 3;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Ready a friendly unit. It deals damage equal to its Might to an enemy unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e7e6dc840210124825f787095541a66ae5cea178-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_257(CardRegistry& r) {
    r.registerCard(257, std::make_unique<LastBreath>());
}

} // namespace riftbound
