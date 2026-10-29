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

// "[Reaction]. As an additional cost to play this, exhaust a friendly unit.
//  Draw 1 and channel 1 rune exhausted."
// The unit to exhaust is picked and exhausted while the spell is finalized
// (kFinalizeResolve + costDueNow). It is a cost, not a choice of target, so it
// is asked with a plain object pick (resume points 30-32, resume_data[8]).
class LifeFromStone : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool canBePlayed(const GameState& state, PlayerId player) const override {
        return !readyFriendlyUnits(state, player).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        GameObjectId pay = pickCostUnit(ctx);
        if (ctx.executor.hasPendingChoice()) return;
        if (costDueNow(ctx)) {
            if (pay == kInvalidId || !ctx.state.objectExists(pay) ||
                ctx.state.getObject(pay).is_exhausted) return;   // cost can't be paid
            ctx.executor.exhaustObject(pay);
            ctx.events.logTrace("LIFE FROM STONE: exhausted " + ctx.state.getObject(pay).name +
                                " (additional cost)");
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
    }
private:
    static std::vector<GameObjectId> readyFriendlyUnits(const GameState& state, PlayerId p) {
        std::vector<GameObjectId> out;
        for (const auto& [id, o] : state.objects)
            if (o.isUnit() && o.controller == p && o.location.has_value() && !o.is_exhausted)
                out.push_back(id);
        return out;
    }
    GameObjectId pickCostUnit(CardContext& ctx) const {
        auto legal = readyFriendlyUnits(ctx.state, ctx.controller);
        if (!ctx.state.chain.resuming.has_value())
            return legal.empty() ? kInvalidId : legal.front();
        auto& ri = *ctx.state.chain.resuming;
        while (ri.resume_data.size() < 9) ri.resume_data.push_back(0);
        if (ri.resume_point < 30) {
            if (legal.empty()) {
                ri.resume_point = 32;
                ri.resume_data[8] = static_cast<int32_t>(kInvalidId);
                return kInvalidId;
            }
            std::vector<Intent> opts;
            for (auto id : legal) opts.push_back(objChoice(ctx.controller, id));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Life from Stone: exhaust a friendly unit (cost)");
            ri.resume_point = 31;
            return kInvalidId;
        }
        if (ri.resume_point == 31) {
            GameObjectId picked = takenObject(ctx);
            if (std::find(legal.begin(), legal.end(), picked) == legal.end())
                picked = legal.empty() ? kInvalidId : legal.front();
            ri.resume_data[8] = static_cast<int32_t>(picked);
            ri.resume_point = 32;
            return picked;
        }
        return static_cast<GameObjectId>(ri.resume_data[8]);
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1013;
        d.def_id = R"RB(rad-029-167)RB";
        d.name = R"RB(Life from Stone)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-029/167)RB";
        d.collector_number = 29;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 1;
        d.power_cost = 0;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
As an additional cost to play this, exhaust a friendly unit.
Draw 1 and channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6a849c0425f0fecb8d00e1b9494e975c4dbbba75-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1013(CardRegistry& r) {
    r.registerCard(1013, std::make_unique<LifeFromStone>());
}

}  // namespace riftbound
