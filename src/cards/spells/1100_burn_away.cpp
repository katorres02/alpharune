#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "Banish a unit with 3 [M] or less or a gear. [Burn 2]."
class BurnAway : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool needsPlayTimeTarget() const override { return true; }
    static std::vector<GameObjectId> candidates(const GameState& state, PlayerId me) {
        std::vector<GameObjectId> out;
        for (const auto& [id, o] : state.objects) {
            if (!o.location.has_value()) continue;
            if (o.controller != me && o.untargetable_by_enemy) continue;
            if ((o.isUnit() && o.current_might <= 3) || o.isGear()) out.push_back(id);
        }
        return out;
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !candidates(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        GameObjectId t = pickTarget(ctx, "Burn Away: banish a unit with 3 [M] or less or a gear",
                                    candidates(ctx.state, ctx.controller));
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) return;   // suspended
        if (t != kInvalidId && ctx.state.objectExists(t)) {
            ctx.events.logTrace("BURN AWAY: banish " + ctx.state.getObject(t).name);
            ctx.executor.banishObject(t);
        }
        ctx.executor.burn(ctx.controller, 2);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1100;
        d.def_id = R"RB(rad-124-167)RB";
        d.name = R"RB(Burn Away)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-124/167)RB";
        d.collector_number = 124;
        d.artist = R"RB(Michal Ivan)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        // Gallery said Body; the printed card's power pip and frame are Order.
        d.domains = {Domain::Order};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 3;
        d.power_cost = 1;
        d.ability_text = R"RB(Banish a unit with 3 [M] or less or a gear. [Burn 2]. (Put the top 2 cards of your Main Deck into your trash.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/751629a34ee6ba6476ce2f0a821b9f84b5562acd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1100(CardRegistry& r) {
    r.registerCard(1100, std::make_unique<BurnAway>());
}

}  // namespace riftbound
