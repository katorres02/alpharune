#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class Cataclysm_1064 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Opponents can't play spells this turn. Move a friendly unit to an
    //  occupied enemy battlefield." The unit is chosen as the spell is played
    // (needsPlayTimeTarget); the destination — a battlefield with enemy units
    // other than the one it is at — as it resolves.
    static bool onBoard(const GameObject& o) {
        return o.location.has_value() && o.zone != ZoneType::Chain;
    }
    static std::vector<BattlefieldId> destinations(const GameState& state, PlayerId me,
                                                  const GameObject& unit) {
        std::vector<BattlefieldId> out;
        const auto here = unit.battlefieldId();
        for (const auto& bf : state.battlefields) {
            if (here && *here == bf.id) continue;
            bool enemy = false;
            for (const auto& [id, o] : state.objects) {
                if (!o.isUnit() || o.controller == me || !onBoard(o)) continue;
                auto obf = o.battlefieldId();
                if (obf && *obf == bf.id) { enemy = true; break; }
            }
            if (enemy) out.push_back(bf.id);
        }
        return out;
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(const GameState& state,
                                                    PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (auto t : Card::enumerateLegalTargets(state, controller)) {
            const auto& u = state.getObject(t);
            if (onBoard(u) && !destinations(state, controller, u).empty()) out.push_back(t);
        }
        return out;
    }
    bool needsPlayTimeTarget() const override { return true; }

    static constexpr int kAwaitDest = 20;
    static constexpr int kDone = 21;

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId opp = opponent(ctx.controller);
        if (!ctx.state.player(opp).cant_play_spells_this_turn) {
            ctx.state.player(opp).cant_play_spells_this_turn = true;
            ctx.events.logTrace("CATACLYSM: opponents can't play spells this turn");
        }
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId u = pickTarget(ctx, "Cataclysm: move which friendly unit?", legal);
        if (ctx.executor.hasPendingChoice()) return;
        if (u == kInvalidId || !ctx.state.objectExists(u)) return;
        auto dests = destinations(ctx.state, ctx.controller, ctx.state.getObject(u));
        if (!ctx.state.chain.resuming.has_value()) {       // direct invocation (tests)
            if (!dests.empty()) ctx.executor.moveToBattlefield(u, dests.front());
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        if (ri.resume_point < kAwaitDest) {
            if (dests.empty()) { ri.resume_point = kDone; return; }
            std::vector<Intent> opts;
            for (auto b : dests)
                opts.push_back(valChoice(ctx.controller, 1 + static_cast<int>(b),
                                         destLabel(1 + static_cast<int>(b))));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Cataclysm: move it to which occupied enemy battlefield?");
            ri.resume_point = kAwaitDest;
            return;
        }
        ri.resume_point = kDone;
        const int d = takenValue(ctx, -1);
        for (auto b : dests) {
            if (1 + static_cast<int>(b) != d) continue;
            ctx.executor.moveToBattlefield(u, b);
            ctx.events.logTrace("CATACLYSM: moved " + ctx.state.getObject(u).name + " to " +
                                destLabel(d));
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1064;
        d.def_id = R"RB(rad-084-167)RB";
        d.name = R"RB(Cataclysm)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-084/167)RB";
        d.collector_number = 84;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.rarity = Rarity::Rare;
        d.energy_cost = 4;
        d.power_cost = 1;
        d.ability_text = R"RB(Opponents can't play spells this turn.
Move a friendly unit to an occupied enemy battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/dc551ece6cf007c76a3a16feba5f26fb7dd4ed65-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1064(CardRegistry& r) {
    r.registerCard(1064, std::make_unique<Cataclysm_1064>());
}

}  // namespace riftbound
