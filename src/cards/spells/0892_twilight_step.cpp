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

class TwilightStep : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Move a unit with 3 [M] or less." The agent picks the unit, then where
    // it moves (its controller's base or any other battlefield).
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .max_might = 3};
    }
    bool needsPlayTimeTarget() const override { return true; }
    // The unit is the target chosen as the spell was played (the engine asks
    // and charges Deflect then; pickTarget returns it, or nothing if it is no
    // longer legal). Where it moves is chosen as the spell resolves.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        std::sort(legal.begin(), legal.end());
        GameObjectId u = pickTarget(ctx, "Twilight Step: move a unit with 3 [M] or less", legal);
        if (u == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting on the agent
        if (u == kInvalidId || !ctx.state.objectExists(u)) return;
        if (!ctx.state.getObject(u).location.has_value()) return;
        std::vector<std::pair<int, std::string>> opts;
        for (int d : moveDests(ctx.state, ctx.state.getObject(u))) opts.push_back({d, destLabel(d)});
        if (opts.empty()) return;
        int dest = opts.front().first;
        if (!stepChooseValue(ctx, ctx.controller, opts, "Twilight Step: move it where?",
                             20, 10, dest, dest))
            return;   // waiting on the agent
        bool ok = false;
        for (const auto& o : opts) if (o.first == dest) ok = true;
        if (!ok) dest = opts.front().first;
        moveTo(ctx, u, dest);
    }
    // "[Flow] [4][P]"
    FlowCost flowCost() const override { return flow(4, 1, Domain::Chaos); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 892;
        d.def_id = R"RB(ven-105-166)RB";
        d.name = R"RB(Twilight Step)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-105/166)RB";
        d.collector_number = 105;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB(Move a unit with 3 [M] or less.
[Flow] [4][P] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1c5c10009a9fb2605df5db1ff12647a47ac07d98-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_892(CardRegistry& r) {
    r.registerCard(892, std::make_unique<TwilightStep>());
}

}  // namespace riftbound
