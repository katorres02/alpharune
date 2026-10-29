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

class RideTheWind : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    // "Move a friendly unit and ready it." The unit is chosen as the spell is
    // played; where it moves (its base or another battlefield) as it resolves
    // (resume points 20/21).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Ride the Wind", legal);
        auto& ri_opt = ctx.state.chain.resuming;
        if (ri_opt.has_value() && ri_opt->resume_point < 8) return;   // waiting
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto dests = moveDests(ctx.state, ctx.state.getObject(picked));
        int dest = -1;
        if (!ri_opt.has_value()) {
            if (!dests.empty()) dest = dests.front();
        } else {
            auto& ri = *ri_opt;
            if (ri.resume_point == 8) {
                if (dests.empty()) { ri.resume_point = 21; }
                else {
                    std::vector<Intent> opts;
                    for (int d : dests) opts.push_back(valChoice(ctx.controller, d, destLabel(d)));
                    ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                               "Ride the Wind: move it where?");
                    ri.resume_point = 20;
                    return;
                }
            } else if (ri.resume_point == 20) {
                ri.resume_point = 21;
                if (!dests.empty()) {
                    dest = takenValue(ctx, dests.front());
                    if (std::find(dests.begin(), dests.end(), dest) == dests.end())
                        dest = dests.front();
                }
            } else {
                return;
            }
        }
        if (dest >= 0) moveTo(ctx, picked, dest);
        ctx.executor.readyObject(picked);
        ctx.events.logTrace("RIDE THE WIND: moved + readied " +
                             ctx.state.getObject(picked).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 173;
        d.def_id = R"RB(ogn-173-298)RB";
        d.name = R"RB(Ride the Wind)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-173/298)RB";
        d.collector_number = 173;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Move a friendly unit and ready it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/864d9f8992db7f9a9e795d9438d941ca564ddde7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_173(CardRegistry& r) {
    r.registerCard(173, std::make_unique<RideTheWind>());
}

} // namespace riftbound
