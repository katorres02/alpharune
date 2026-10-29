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

class SatchelCharge : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Action] Move a friendly unit, then play a Bomb gear token to a
    //  battlefield." The unit is chosen as the spell is played; where it moves
    // and where the Bomb goes, as it resolves.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    bool needsPlayTimeTarget() const override { return true; }

    static constexpr int kAwaitDest = 20;
    static constexpr int kAwaitBomb = 21;
    static constexpr int kDone = 22;

    static void promptBomb(CardContext& ctx, ChainItem& ri) {
        if (ctx.state.battlefields.empty()) { ri.resume_point = kDone; return; }
        std::vector<Intent> opts;
        for (const auto& bf : ctx.state.battlefields)
            opts.push_back(valChoice(ctx.controller, 1 + static_cast<int>(bf.id),
                                     destLabel(1 + static_cast<int>(bf.id))));
        ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                   "Satchel Charge: play a Bomb to which battlefield?");
        ri.resume_point = kAwaitBomb;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId u = pickTarget(ctx, "Satchel Charge: move which friendly unit?", legal);
        if (ctx.executor.hasPendingChoice()) return;
        const bool unit_ok = u != kInvalidId && ctx.state.objectExists(u) &&
                             ctx.state.getObject(u).location.has_value();
        if (!ctx.state.chain.resuming.has_value()) {        // direct invocation (tests)
            if (unit_ok) {
                auto dests = moveDests(ctx.state, ctx.state.getObject(u));
                if (!dests.empty()) moveTo(ctx, u, dests.front());
            }
            if (!ctx.state.battlefields.empty())
                createBombToken(ctx, ctx.controller, ctx.state.battlefields.front().id);
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        if (ri.resume_point < kAwaitDest) {
            if (unit_ok) {
                auto dests = moveDests(ctx.state, ctx.state.getObject(u));
                if (!dests.empty()) {
                    std::vector<Intent> opts;
                    for (int d : dests) opts.push_back(valChoice(ctx.controller, d, destLabel(d)));
                    ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                               "Satchel Charge: move it where?");
                    ri.resume_point = kAwaitDest;
                    return;
                }
            }
            promptBomb(ctx, ri);
            return;
        }
        if (ri.resume_point == kAwaitDest) {
            const int d = takenValue(ctx, -1);
            if (unit_ok) {
                auto dests = moveDests(ctx.state, ctx.state.getObject(u));
                if (std::find(dests.begin(), dests.end(), d) != dests.end()) {
                    moveTo(ctx, u, d);
                    ctx.events.logTrace("SATCHEL CHARGE: moved " + ctx.state.getObject(u).name +
                                        " to " + destLabel(d));
                }
            }
            promptBomb(ctx, ri);
            return;
        }
        if (ri.resume_point == kAwaitBomb) {
            ri.resume_point = kDone;
            const int d = takenValue(ctx, -1);
            for (const auto& bf : ctx.state.battlefields) {
                if (1 + static_cast<int>(bf.id) != d) continue;
                createBombToken(ctx, ctx.controller, bf.id);
                ctx.events.logTrace("SATCHEL CHARGE: Bomb at " + destLabel(d));
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1076;
        d.def_id = R"RB(rad-096-167)RB";
        d.name = R"RB(Satchel Charge)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-096/167)RB";
        d.collector_number = 96;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 3;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Move a friendly unit, then play a Bomb gear token to a battlefield. (It has [Deploy]. When it dies, deal 2 to an enemy there and kill your other Bombs there.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b07c26438f600b87c92e4ed778a1c8b073b47f5c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1076(CardRegistry& r) {
    r.registerCard(1076, std::make_unique<SatchelCharge>());
}

}  // namespace riftbound
