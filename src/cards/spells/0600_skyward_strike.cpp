#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class SkywardStrike : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    // "Move an enemy unit. [Level 6][>] [Stun] an enemy unit."
    // The stun is an additional effect with its own target (it may be the
    // same unit). Targets: pickTarget (move only) or pickTargetPair (move,
    // stun) at Level 6; then the move's destination (the unit's base or
    // another battlefield) via a choice at resume_points 20/21.
    static constexpr int kDestAsked = 20;
    static constexpr int kDestDone = 21;
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        auto& ri = ctx.state.chain.resuming;
        const bool level6 = ctx.state.player(ctx.controller).xp >= 6;
        GameObjectId mover = kInvalidId, stunned = kInvalidId;
        if (!ri.has_value() || ri->resume_point < kDestAsked) {
            if (level6) {
                auto pr = pickTargetPair(ctx, "Skyward Strike: move / stun", legal,
                    [&](GameObjectId) { return enumerateLegalTargets(ctx.state, ctx.controller); });
                if (ctx.executor.hasPendingChoice()) return;
                mover = pr.first; stunned = pr.second;
            } else {
                mover = pickTarget(ctx, "Skyward Strike: move an enemy unit", legal);
                if (ctx.executor.hasPendingChoice()) return;
            }
        } else if (level6 && ri->resume_data.size() >= 5) {
            mover = static_cast<GameObjectId>(ri->resume_data[3]);
            stunned = static_cast<GameObjectId>(ri->resume_data[4]);
        } else if (ri->resume_data.size() >= 3) {
            mover = static_cast<GameObjectId>(ri->resume_data[2]);
        }

        // Destinations for the move: its base, or a battlefield it isn't at.
        std::vector<LocationId> dests;
        if (mover != kInvalidId && ctx.state.objectExists(mover) &&
            ctx.state.getObject(mover).location.has_value()) {
            const auto& m = ctx.state.getObject(mover);
            LocationId base{BaseLocation{m.controller}};
            if (!(*m.location == base)) dests.push_back(base);
            for (const auto& bf : ctx.state.battlefields) {
                LocationId l{BattlefieldLocation{bf.id}};
                if (!(*m.location == l)) dests.push_back(l);
            }
        }
        int dest = 0;
        if (ri.has_value() && !dests.empty()) {
            if (ri->resume_point < kDestAsked) {
                std::vector<Intent> opts;
                for (int i = 0; i < static_cast<int>(dests.size()); ++i) {
                    Intent o; o.type = IntentType::MakeChoice; o.player = ctx.controller;
                    o.chosen_value = i;
                    o.choice_label = std::holds_alternative<BaseLocation>(dests[i])
                        ? std::string("base")
                        : "battlefield " + std::to_string(std::get<BattlefieldLocation>(dests[i]).id);
                    opts.push_back(o);
                }
                ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                           "Skyward Strike: move it where?");
                ri->resume_point = kDestAsked;
                return;
            }
            if (ri->resume_point == kDestAsked) {
                auto picked = ctx.executor.takeChoice();
                dest = picked ? picked->chosen_value.value_or(0) : 0;
                if (dest < 0 || dest >= static_cast<int>(dests.size())) dest = 0;
                ri->resume_point = kDestDone;
            }
        }
        if (!dests.empty()) {
            if (std::holds_alternative<BaseLocation>(dests[dest])) {
                ctx.executor.moveToBase(mover);
            } else {
                ctx.executor.moveToBattlefield(mover, std::get<BattlefieldLocation>(dests[dest]).id);
            }
            ctx.events.logTrace("SKYWARD STRIKE: moved an enemy unit");
        }
        if (level6 && stunned != kInvalidId && ctx.state.objectExists(stunned) &&
            ctx.state.getObject(stunned).location.has_value()) {
            ctx.executor.stunUnitBy(stunned, ctx.source);
            ctx.events.logTrace("SKYWARD STRIKE: Level 6 -> stun");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 600;
        d.def_id = R"RB(unl-038-219)RB";
        d.name = R"RB(Skyward Strike)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-038/219)RB";
        d.collector_number = 38;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB(Move an enemy unit.
[Level 6][>] [Stun] an enemy unit. (While you have 6+ XP, get the effect. A stunned unit doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/998740e590fec024f204ad26ed7e2dad2a0c8d66-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_600(CardRegistry& r) {
    r.registerCard(600, std::make_unique<SkywardStrike>());
}

} // namespace riftbound
