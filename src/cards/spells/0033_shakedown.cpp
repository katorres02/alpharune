#include "cards/card.h"
#include "cards/card_registry.h"
#include "cards/card_helpers.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class Shakedown : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_enemy = true};
    }
    // "Choose an enemy unit. Deal 6 to it unless its controller has you draw 2."
    // The unit's controller decides (resume points 20/21).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        const GameObjectId t = targets[0];
        const PlayerId them = ctx.state.getObject(t).controller;
        auto& ri_opt = ctx.state.chain.resuming;
        int answer = 0;
        if (ri_opt.has_value()) {
            auto& ri = *ri_opt;
            if (ri.resume_point < 20) {
                std::vector<Intent> opts = {valChoice(them, 0, "Take 6 damage"),
                                            valChoice(them, 1, "Have them draw 2")};
                ctx.executor.requestChoice(them, std::move(opts),
                                           "Shakedown: take 6 or let them draw 2?");
                ri.resume_point = 20;
                return;
            }
            if (ri.resume_point != 20) return;
            ri.resume_point = 21;
            answer = takenValue(ctx, 0);
        }
        if (answer == 1) {
            ctx.executor.drawCards(ctx.controller, 2);
            ctx.events.logTrace("SHAKEDOWN: opponent let the caster draw 2");
            return;
        }
        ctx.executor.dealDamage(t, 6, ctx.source);
        if (ctx.state.objectExists(t) && ctx.state.getObject(t).hasLethalDamage())
            ctx.executor.killObject(t);
        ctx.events.logTrace("SHAKEDOWN: deal 6");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 33;
        d.def_id = R"RB(ogn-033-298)RB";
        d.name = R"RB(Shakedown)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-033/298)RB";
        d.collector_number = 33;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose an enemy unit. Deal 6 to it unless its controller has you draw 2.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ab71d92c94bf609e2fa6efc8fec06fe1e8b10108-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_33(CardRegistry& r) {
    r.registerCard(33, std::make_unique<Shakedown>());
}

} // namespace riftbound
