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

class FrigidJewel : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you draw your second card each turn, give a friendly unit +2 [M]
    // this turn." WhenYouDrawACard fires per draw event; it triggers only once
    // the controller has drawn 2+ this turn and it hasn't resolved yet this
    // turn. The friendly unit is chosen as the ability is finalized.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouDrawACard; }
    static int sentinel(const GameState& state) { return state.turn.turn_number + 1; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId) const override {
        if (state.player(controller).draws_this_turn < 2) return false;
        if (!state.objectExists(source)) return false;
        const auto& cc = state.getObject(source).card_counters;
        auto it = cc.find("__frigid_fired_turn");
        return it == cc.end() || it->second != sentinel(state);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.state.player(ctx.controller).draws_this_turn < 2) return;
        std::vector<GameObjectId> friendly;
        for (auto& [id, obj] : ctx.state.objects)
            if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                friendly.push_back(id);
        GameObjectId tgt = pickTarget(ctx, "Frigid Jewel: +2 [M] to a friendly unit", friendly);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;   // none / waiting
        auto& self = ctx.state.getObject(ctx.source);
        if (self.card_counters["__frigid_fired_turn"] == sentinel(ctx.state)) return;  // once/turn
        self.card_counters["__frigid_fired_turn"] = sentinel(ctx.state);
        ctx.executor.giveTemporaryMight(tgt, 2);
        ctx.events.logTrace("FRIGID JEWEL: 2nd draw -> a friendly unit +2 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 636;
        d.def_id = R"RB(unl-074-219)RB";
        d.name = R"RB(Frigid Jewel)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-074/219)RB";
        d.collector_number = 74;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you draw your second card each turn, give a friendly unit +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f1a6c28649bf4478b4728379b6d548058739f0e9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_636(CardRegistry& r) {
    r.registerCard(636, std::make_unique<FrigidJewel>());
}

} // namespace riftbound
