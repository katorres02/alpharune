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

// [Action]. "[Buff] a friendly unit and [Stun] an enemy unit."
class OneTwoPunch : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    static std::vector<GameObjectId> friendly(const GameState& s, PlayerId me) {
        return chooseableUnits(s, me, [me](const GameObject& o) { return o.controller == me; });
    }
    static std::vector<GameObjectId> enemy(const GameState& s, PlayerId me) {
        return chooseableUnits(s, me, [me](const GameObject& o) { return o.controller != me; });
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !friendly(state, controller).empty() && !enemy(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId me = ctx.controller;
        auto [ally, foe] = pickTargetPair(ctx, "One-Two Punch: buff a friendly unit, stun an enemy unit",
                                          friendly(ctx.state, me),
                                          [&](GameObjectId) { return enemy(ctx.state, me); });
        if (finalizingChoices(ctx)) return;
        if (ctx.state.chain.resuming.has_value() &&
            (ctx.state.chain.resuming->resume_point == 10 ||
             ctx.state.chain.resuming->resume_point == 12)) return;   // suspended
        if (ally != kInvalidId && ctx.state.objectExists(ally)) ctx.executor.buffUnit(ally);
        if (foe != kInvalidId && ctx.state.objectExists(foe)) ctx.executor.stunUnit(foe);
        ctx.events.logTrace("ONE-TWO PUNCH: buff + stun");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1097;
        d.def_id = R"RB(rad-121-167)RB";
        d.name = R"RB(One-Two Punch)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-121/167)RB";
        d.collector_number = 121;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        // Gallery lists no domain; the printed card is Order (yellow frame).
        d.domains = {Domain::Order};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
[Buff] a friendly unit and [Stun] an enemy unit. (Give the friendly unit a +1 [M] buff if it doesn't have one. The enemy unit doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1f246286c658d44dc4e7d51c158b2f822da39825-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1097(CardRegistry& r) {
    r.registerCard(1097, std::make_unique<OneTwoPunch>());
}

}  // namespace riftbound
