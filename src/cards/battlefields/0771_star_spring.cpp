#include "cards/card.h"
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

class StarSpring : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "The first time a player plays a non-token unit here each turn, they may
    // move another unit they control here to its base." The battlefield's
    // WhenYouPlayAUnit trigger carries the played unit as its subject.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId subject) const override {
        if (subject == kInvalidId || !state.objectExists(subject)) return false;
        if (state.getObject(subject).isToken()) return false;
        if (!state.objectExists(source)) return false;
        const auto& c = state.getObject(source).card_counters;
        auto it = c.find(firstKey(controller));
        return it == c.end() || it->second != state.turn.turn_number;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        BattlefieldId here = kInvalidId;
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { here = bf.id; break; }
        }
        if (here == kInvalidId) return;
        const GameObjectId played = ctx.state.chain.resuming
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        if (played == kInvalidId) return;
        // This is that player's first non-token play here this turn.
        ctx.state.getObject(ctx.source).card_counters[firstKey(ctx.controller)] =
            ctx.state.turn.turn_number;

        // "Another unit they control here" (not the played one).
        std::vector<GameObjectId> movable;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == played) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == here) movable.push_back(id);
        }
        std::sort(movable.begin(), movable.end());

        int decision = confirmOptional(ctx, "Star Spring: move another unit here to base",
            [&]() { return !movable.empty(); });
        if (decision < 1) return;  // waiting on the agent, declined or nothing to move
        GameObjectId pick = pickTarget(ctx, "Star Spring: unit to move to its base", movable);
        if (pick == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;  // waiting on the agent
        if (pick == kInvalidId || !ctx.state.objectExists(pick)) return;
        ctx.executor.moveToBase(pick);
        ctx.events.logTrace("STAR SPRING: moved " + ctx.state.getObject(pick).name + " to its base");
    }
private:
    static std::string firstKey(PlayerId p) {
        return "__starspring_p" + std::to_string(playerIndex(p));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 771;
        d.def_id = R"RB(unl-215-219)RB";
        d.name = R"RB(Star Spring)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-215/219)RB";
        d.collector_number = 215;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(The first time a player plays a non-token unit here each turn, they may move another unit they control here to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0c4fe88ffb5c1b02b58b2c6b4f02de441ac451d6-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_771(CardRegistry& r) {
    r.registerCard(771, std::make_unique<StarSpring>());
}

} // namespace riftbound
