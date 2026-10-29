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

class ParallelConvergence : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "As an additional cost to play this, return a friendly unit to its
    // owner's hand. Deal 5 to a unit at a battlefield."
    // The friendly unit and the target are chosen as the spell is put on the
    // chain (kFinalizeResolve); the return is paid then too (costDueNow).
    static std::vector<GameObjectId> friendlyUnits(const GameState& state, PlayerId p) {
        std::vector<GameObjectId> out;
        for (const auto& [id, o] : state.objects)
            if (o.isUnit() && o.controller == p && o.location.has_value()) out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    static std::vector<GameObjectId> bfUnits(const GameState& state, PlayerId p, GameObjectId except) {
        auto out = chooseableUnits(state, p, [except](const GameObject& o) {
            return o.id != except && o.battlefieldId().has_value();
        });
        std::sort(out.begin(), out.end());
        return out;
    }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool needsPlayTimeTargetPair() const override { return true; }
    bool canBePlayed(const GameState& state, PlayerId player) const override {
        return !friendlyUnits(state, player).empty();
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        for (auto f : friendlyUnits(state, player))
            if (!bfUnits(state, player, f).empty()) return true;
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto [ret, tgt] = pickTargetPair(ctx, "Parallel Convergence: return a friendly unit (cost), then deal 5",
            friendlyUnits(ctx.state, ctx.controller),
            [&](GameObjectId a) { return bfUnits(ctx.state, ctx.controller, a); });
        if (ret != kInvalidId && costDueNow(ctx) && ctx.state.objectExists(ret) &&
            ctx.state.getObject(ret).location.has_value()) {
            ctx.executor.bounceToHand(ret);
            ctx.events.logTrace("PARALLEL CONVERGENCE: returned a friendly unit (additional cost)");
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        ctx.executor.dealDamage(tgt, 5, ctx.source);
        if (ctx.state.objectExists(tgt) && ctx.state.getObject(tgt).hasLethalDamage())
            ctx.executor.killObject(tgt);
        ctx.events.logTrace("PARALLEL CONVERGENCE: dealt 5");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1031;
        d.def_id = R"RB(rad-051-167)RB";
        d.name = R"RB(Parallel Convergence)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-051/167)RB";
        d.collector_number = 51;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 2;
        d.power_cost = 1;
        d.ability_text = R"RB(As an additional cost to play this, return a friendly unit to its owner's hand.
Deal 5 to a unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/55e382346c6fa890936a1cf14abaf57b8ee0b8f2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1031(CardRegistry& r) {
    r.registerCard(1031, std::make_unique<ParallelConvergence>());
}

}  // namespace riftbound
