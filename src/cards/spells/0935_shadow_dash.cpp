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

// Battlefields where `player` has at least one unit.
std::vector<BattlefieldId> battlefieldsWithUnitsOf(const GameState& state, PlayerId player) {
    std::vector<BattlefieldId> out;
    for (const auto& bf : state.battlefields) {
        for (const auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != player) continue;
            auto at = obj.battlefieldId();
            if (at.has_value() && *at == bf.id) { out.push_back(bf.id); break; }
        }
    }
    return out;
}
// Battlefields the enemy unit `u` could be moved to by Shadow Dash.
std::vector<BattlefieldId> dashDests(const GameState& state, PlayerId player,
                                     const GameObject& u) {
    std::vector<BattlefieldId> out;
    for (auto bf : battlefieldsWithUnitsOf(state, player)) {
        auto at = u.battlefieldId();
        if (at.has_value() && *at == bf) continue;
        out.push_back(bf);
    }
    return out;
}
std::vector<GameObjectId> dashTargets(const GameState& state, PlayerId player) {
    auto out = chooseableUnits(state, player, [&](const GameObject& o) {
        return o.controller != player && !dashDests(state, player, o).empty();
    });
    std::sort(out.begin(), out.end());
    return out;
}

class ShadowDash : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Move an enemy unit to a battlefield where you have units. If you have
    //  exactly two units there, they each get +1 [M] this turn."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        return dashTargets(state, controller);
    }
    // The enemy unit is the target chosen as the spell was played (the engine
    // asks and charges Deflect then; pickTarget returns it, or nothing if it is
    // no longer legal). The destination battlefield is chosen as it resolves.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        GameObjectId u = pickTarget(ctx, "Shadow Dash: move an enemy unit",
                                    dashTargets(ctx.state, ctx.controller));
        if (u == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting on the agent
        if (u == kInvalidId || !ctx.state.objectExists(u) ||
            !ctx.state.getObject(u).location.has_value())
            return;
        auto dests = dashDests(ctx.state, ctx.controller, ctx.state.getObject(u));
        if (dests.empty()) return;
        std::vector<std::pair<int, std::string>> opts;
        for (auto bf : dests)
            opts.push_back({1 + static_cast<int>(bf), "Battlefield #" + std::to_string(bf)});
        int v = opts.front().first;
        if (!stepChooseValue(ctx, ctx.controller, opts, "Shadow Dash: to which battlefield?",
                             20, 10, v, v))
            return;   // waiting on the agent
        auto bf = static_cast<BattlefieldId>(v - 1);
        if (v < 1 || std::find(dests.begin(), dests.end(), bf) == dests.end())
            bf = dests.front();
        ctx.executor.moveToBattlefield(u, bf);
        // "If you have exactly two units there, they each get +1 [M] this turn."
        std::vector<GameObjectId> mine;
        for (const auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto at = obj.battlefieldId();
            if (at.has_value() && *at == bf) mine.push_back(id);
        }
        if (mine.size() == 2)
            for (auto id : mine) ctx.executor.giveTemporaryMight(id, 1);
    }
    // "[Flow] [5][A][A]"
    FlowCost flowCost() const override { return flow(5, 2, Domain::Fury, /*any_domain=*/true); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 935;
        d.def_id = R"RB(ven-148-166)RB";
        d.name = R"RB(Shadow Dash)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-148/166)RB";
        d.collector_number = 148;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Order};
        d.tags = {R"RB(Shen)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB(Move an enemy unit to a battlefield where you have units. If you have exactly two units there, they each get +1 [M] this turn.
[Flow] [5][A][A] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c95a4862c3af5f3ec1cee1f1ae7291c8724df7a1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_935(CardRegistry& r) {
    r.registerCard(935, std::make_unique<ShadowDash>());
}

}  // namespace riftbound
