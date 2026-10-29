#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class AgonysEmbrace : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "Once each turn, when you play a card from face down, draw 1. Then, if
    // it's not your turn, discard 1."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayFromFacedown; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return !usedThisTurn(state, source);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& ri = ctx.state.chain.resuming;
        if (!ri.has_value() || ri->resume_point == 0) {
            // A second trigger queued before the first resolved does nothing.
            if (usedThisTurn(ctx.state, ctx.source)) return;
            ctx.state.getObject(ctx.source).card_counters[kUsed] = ctx.state.turn.turn_number + 1;
            ctx.executor.drawCards(ctx.controller, 1);
            if (ctx.state.turn.turn_player == ctx.controller || !ri.has_value()) return;
        }
        discardOneThenAct(ctx, "Agony's Embrace: discard 1", [](CardContext&, GameObjectId) {});
    }
private:
    static constexpr const char* kUsed = "__agony_used_turn";   // turn + 1
    static bool usedThisTurn(const GameState& state, GameObjectId self) {
        if (!state.objectExists(self)) return false;
        const auto& cc = state.getObject(self).card_counters;
        auto it = cc.find(kUsed);
        return it != cc.end() && it->second == state.turn.turn_number + 1;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 981;
        d.def_id = R"RB(rad-153-167)RB";
        d.name = R"RB(Agony's Embrace)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-153/167)RB";
        d.collector_number = 153;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body,Domain::Chaos};
        d.tags = {R"RB(Evelynn)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Once each turn, when you play a card from face down, draw 1. Then, if it's not your turn, discard 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ca217b57d0e8d2e04e7a35105deaa6be17e854f0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_981(CardRegistry& r) {
    r.registerCard(981, std::make_unique<AgonysEmbrace>());
}

}  // namespace riftbound
