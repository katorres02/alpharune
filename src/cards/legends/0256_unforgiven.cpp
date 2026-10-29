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

class Unforgiven : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "[2], [E]: Move a friendly unit to or from its base." A unit at a
    // battlefield goes to its base; a unit at its base goes to a battlefield
    // the player chooses (resume_point 20 = destination published).
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        const GameObjectId u = targets[0];
        const auto& unit = ctx.state.getObject(u);
        if (!unit.location.has_value()) return;
        if (!unit.isAtBase()) {
            ctx.executor.moveToBase(u);
            ctx.events.logTrace("UNFORGIVEN: moved " + unit.name + " to its base");
            return;
        }
        std::vector<int> dests;
        for (int d : moveDests(ctx.state, unit)) if (d > 0) dests.push_back(d);
        if (dests.empty()) return;
        int d = dests.front();
        if (ctx.state.chain.resuming.has_value()) {
            auto& ri = *ctx.state.chain.resuming;
            if (ri.resume_point < 20) {
                std::vector<Intent> options;
                for (int x : dests) options.push_back(valChoice(ctx.controller, x, destLabel(x)));
                ctx.executor.requestChoice(ctx.controller, std::move(options),
                                           "Unforgiven: move " + unit.name + " to which battlefield?");
                ri.resume_point = 20;
                return;
            }
            if (ri.resume_point != 20) return;
            ri.resume_point = 21;
            d = takenValue(ctx, d);
            if (std::find(dests.begin(), dests.end(), d) == dests.end()) d = dests.front();
        }
        moveTo(ctx, u, d);
        ctx.events.logTrace("UNFORGIVEN: moved a unit from its base to " + destLabel(d));
    }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true, .energy = 2}; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 256;
        d.def_id = R"RB(ogn-259-298)RB";
        d.name = R"RB(Unforgiven)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-259/298)RB";
        d.collector_number = 259;
        d.artist = R"RB(TSWCK逍)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Chaos};
        d.tags = {R"RB(Yasuo)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([2], [E]: Move a friendly unit to or from its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/68e4d3230b785738ae9d86f780f7f5607ef11807-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_256(CardRegistry& r) {
    r.registerCard(256, std::make_unique<Unforgiven>());
}

} // namespace riftbound
