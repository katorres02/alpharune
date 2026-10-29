#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "For each player, the first time each turn a non-token unit they control
// dies here, they play a 1 [M] Recruit unit token to their base."
// TriggerManager::onUnitDied fires WhenAFriendlyUnitDies on the battlefield
// the unit died at, controlled by the dying unit's controller (subject = the
// unit). "First time each turn" is kept per player on this card object.
class BlackRoseSanctum : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitDies; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId subject) const override {
        if (subject == kInvalidId || !state.objectExists(subject)) return false;
        if (state.getObject(subject).isToken()) return false;
        return !usedThisTurn(state, source, controller);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (usedThisTurn(ctx.state, ctx.source, ctx.controller)) return;   // not the first
        ctx.state.getObject(ctx.source).card_counters[key(ctx.controller)] =
            ctx.state.turn.turn_number + 1;
        auto tok = ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1,
                                            {"Recruit"}, KeywordSet{},
                                            LocationId{BaseLocation{ctx.controller}}, false);
        if (ctx.state.objectExists(tok)) ctx.state.getObject(tok).card_def_id = 954;
        ctx.events.logTrace("BLACK ROSE SANCTUM: first death here this turn -> Recruit to base");
    }

private:
    static std::string key(PlayerId p) {
        return "__brs_turn_p" + std::to_string(static_cast<int>(p));
    }
    static bool usedThisTurn(const GameState& state, GameObjectId self, PlayerId p) {
        if (!state.objectExists(self)) return false;
        const auto& c = state.getObject(self).card_counters;
        auto it = c.find(key(p));
        return it != c.end() && it->second == state.turn.turn_number + 1;
    }

    const CardDef def_ = [] {
        CardDef d;
        d.id = 1118;
        d.def_id = R"RB(rad-159-167)RB";
        d.name = R"RB(Black Rose Sanctum)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-159/167)RB";
        d.collector_number = 159;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(For each player, the first time each turn a non-token unit they control dies here, they play a 1 [M] Recruit unit token to their base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/03ce18a231c7c919875b436b06bbe4e1a99c4831-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1118(CardRegistry& r) {
    r.registerCard(1118, std::make_unique<BlackRoseSanctum>());
}

}  // namespace riftbound
