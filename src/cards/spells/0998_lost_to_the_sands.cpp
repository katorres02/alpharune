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

std::vector<GameObjectId> lostSandsUnitsAtBf(const GameState& state, PlayerId me) {
    return chooseableUnits(state, me, [](const GameObject& o) { return o.isAtBattlefield(); });
}
std::vector<GameObjectId> lostSandsGear(const GameState& state, PlayerId me) {
    std::vector<GameObjectId> out;
    for (const auto& [id, o] : state.objects) {
        if (!o.isGear() || !o.location.has_value()) continue;
        if (o.controller != me && o.untargetable_by_enemy) continue;
        out.push_back(id);
    }
    return out;
}

// "[Action] Choose one — Deal 4 to a unit at a battlefield. / Kill a gear."
// Mode and target are chosen when the spell is finalized (CR 355).
class LostToTheSands : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !lostSandsUnitsAtBf(state, controller).empty() ||
               !lostSandsGear(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const auto units = lostSandsUnitsAtBf(ctx.state, ctx.controller);
        const auto gear = lostSandsGear(ctx.state, ctx.controller);
        uint32_t legal = 0;
        if (!units.empty()) legal |= 0x1u;
        if (!gear.empty()) legal |= 0x2u;
        const int mode = pickMode(ctx, "Lost to the Sands", 2,
                                  {"Deal 4 to a unit at a battlefield", "Kill a gear"}, legal);
        if (mode == -1) return;
        GameObjectId t = kInvalidId;
        if (mode == 0) t = pickTarget(ctx, "Lost to the Sands: deal 4 to a unit at a battlefield", units);
        else if (mode == 1) t = pickTarget(ctx, "Lost to the Sands: kill a gear", gear);
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        if (mode == 0) ctx.executor.dealDamage(t, 4, ctx.source);
        else if (mode == 1) ctx.executor.killObject(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 998;
        d.def_id = R"RB(rad-013-167)RB";
        d.name = R"RB(Lost to the Sands)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-013/167)RB";
        d.collector_number = 13;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 3;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose one —
Deal 4 to a unit at a battlefield.
Kill a gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/92071a0f4d495f5f7acdf78d060133b98d1cebae-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_998(CardRegistry& r) {
    r.registerCard(998, std::make_unique<LostToTheSands>());
}

}  // namespace riftbound
