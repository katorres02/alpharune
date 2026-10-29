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

class DemonShade : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // [Hidden] is engine-handled. "Choose a friendly unit, then choose one —
    //  Move it to an open battlefield. / Give it [Ganking] this turn if there
    //  are no open battlefields." Open = unoccupied and uncontrolled (CR
    //  170.11.c). The unit is chosen as the spell is played (from face down,
    //  the engine restricts it to that battlefield and passes it in
    //  `targets`); the mode and the destination as it resolves.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    bool needsPlayTimeTarget() const override { return true; }

    static std::vector<BattlefieldId> openBattlefields(const GameState& state) {
        std::vector<BattlefieldId> out;
        for (const auto& bf : state.battlefields) {
            if (bf.controller.has_value()) continue;
            bool occupied = false;
            for (const auto& [id, o] : state.objects) {
                if (!o.isUnit() || o.zone == ZoneType::Chain) continue;
                auto obf = o.battlefieldId();
                if (obf && *obf == bf.id) { occupied = true; break; }
            }
            if (!occupied) out.push_back(bf.id);
        }
        return out;
    }

    static constexpr int kAwaitDest = 20;
    static constexpr int kDone = 21;

    // The chosen unit: from the play intent when it carries one the engine did
    // not record as a finalize-time choice (a play from face down), else via
    // pickTarget. Call after pickMode (it moves resume_point to 8).
    GameObjectId chosenUnit(CardContext& ctx, const std::vector<GameObjectId>& targets) {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        auto& ri_opt = ctx.state.chain.resuming;
        if (ri_opt.has_value() && ri_opt->resume_point < 8 && !targets.empty() &&
            !(ri_opt->resume_data.size() > 5 && ri_opt->resume_data[5] == 1)) {
            GameObjectId t = targets.front();
            if (std::find(legal.begin(), legal.end(), t) == legal.end()) t = kInvalidId;
            while (ri_opt->resume_data.size() < 3) ri_opt->resume_data.push_back(0);
            ri_opt->resume_data[2] = static_cast<int32_t>(t);
            ri_opt->resume_point = 8;
            return t;
        }
        return pickTarget(ctx, "Demon Shade: choose a friendly unit", legal);
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        const int mode = pickMode(ctx, "Demon Shade", 2,
                                  {"Move it to an open battlefield",
                                   "Give it [Ganking] this turn if there are no open battlefields"});
        if (mode < 0) return;
        GameObjectId u = chosenUnit(ctx, targets);
        if (ctx.executor.hasPendingChoice()) return;
        if (u == kInvalidId || !ctx.state.objectExists(u) ||
            !ctx.state.getObject(u).location.has_value()) return;
        auto open = openBattlefields(ctx.state);
        if (mode == 1) {
            if (!open.empty()) return;
            ctx.executor.giveTemporaryKeyword(u, Keyword::Ganking, 0);
            ctx.events.logTrace("DEMON SHADE: no open battlefields -> " +
                                ctx.state.getObject(u).name + " has [Ganking] this turn");
            return;
        }
        if (!ctx.state.chain.resuming.has_value()) {        // direct invocation (tests)
            if (!open.empty()) ctx.executor.moveToBattlefield(u, open.front());
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        if (ri.resume_point < kAwaitDest) {
            if (open.empty()) { ri.resume_point = kDone; return; }
            std::vector<Intent> opts;
            for (auto b : open)
                opts.push_back(valChoice(ctx.controller, 1 + static_cast<int>(b),
                                         destLabel(1 + static_cast<int>(b))));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Demon Shade: move it to which open battlefield?");
            ri.resume_point = kAwaitDest;
            return;
        }
        ri.resume_point = kDone;
        const int d = takenValue(ctx, -1);
        for (auto b : open) {
            if (1 + static_cast<int>(b) != d) continue;
            ctx.executor.moveToBattlefield(u, b);
            ctx.events.logTrace("DEMON SHADE: moved " + ctx.state.getObject(u).name + " to " +
                                destLabel(d));
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1080;
        d.def_id = R"RB(rad-102-167)RB";
        d.name = R"RB(Demon Shade)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-102/167)RB";
        d.collector_number = 102;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
Choose a friendly unit, then choose one —
Move it to an open battlefield.
Give it [Ganking] this turn if there are no open battlefields. (It can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e9dd313886624aafc7e0c7f9e1689225c042b484-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1080(CardRegistry& r) {
    r.registerCard(1080, std::make_unique<DemonShade>());
}

}  // namespace riftbound
