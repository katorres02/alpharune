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

class SirensSong : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Reaction] Move an enemy unit that moved this turn." The unit is chosen
    // as the spell is played; its destination (its base or another
    // battlefield) as it resolves.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(const GameState& state,
                                                    PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (auto t : Card::enumerateLegalTargets(state, controller)) {
            const auto& u = state.getObject(t);
            if (u.location.has_value() && u.zone != ZoneType::Chain && u.moves_this_turn > 0)
                out.push_back(t);
        }
        return out;
    }
    bool needsPlayTimeTarget() const override { return true; }

    static constexpr int kAwaitDest = 20;
    static constexpr int kDone = 21;

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId u = pickTarget(ctx, "Siren's Song: move which enemy unit?", legal);
        if (ctx.executor.hasPendingChoice()) return;
        if (u == kInvalidId || !ctx.state.objectExists(u)) return;
        auto dests = moveDests(ctx.state, ctx.state.getObject(u));
        if (!ctx.state.chain.resuming.has_value()) {        // direct invocation (tests)
            if (!dests.empty()) moveTo(ctx, u, dests.front());
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        if (ri.resume_point < kAwaitDest) {
            if (dests.empty()) { ri.resume_point = kDone; return; }
            std::vector<Intent> opts;
            for (int d : dests) opts.push_back(valChoice(ctx.controller, d, destLabel(d)));
            ctx.executor.requestChoice(ctx.controller, std::move(opts), "Siren's Song: move it where?");
            ri.resume_point = kAwaitDest;
            return;
        }
        ri.resume_point = kDone;
        const int d = takenValue(ctx, -1);
        if (std::find(dests.begin(), dests.end(), d) == dests.end()) return;
        moveTo(ctx, u, d);
        ctx.events.logTrace("SIREN'S SONG: moved " + ctx.state.getObject(u).name + " to " +
                            destLabel(d));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1077;
        d.def_id = R"RB(rad-098-167)RB";
        d.name = R"RB(Siren's Song)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-098/167)RB";
        d.collector_number = 98;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Move an enemy unit that moved this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/742967b3e85f51b242fc04b8db4c4e3cd7b88672-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1077(CardRegistry& r) {
    r.registerCard(1077, std::make_unique<SirensSong>());
}

}  // namespace riftbound
