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

// [Disarm] (engine-owned, CR 831). "When I hold, if you control a facedown
// card here, score 1 point."
class EvelynnInControl : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    int disarmValue() const override { return 1; }
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }

    static bool controlsFacedownHere(const GameState& state, GameObjectId self, PlayerId controller) {
        if (!state.objectExists(self)) return false;
        auto here = state.getObject(self).battlefieldId();
        if (!here) return false;
        for (const auto& bf : state.battlefields) {
            if (bf.id != *here) continue;
            for (auto fid : bf.facedown)
                if (state.objectExists(fid) && state.getObject(fid).controller == controller)
                    return true;
        }
        return false;
    }
    // Intervening "if": checked as it triggers and again as it resolves.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId) const override {
        return controlsFacedownHere(state, source, controller);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!controlsFacedownHere(ctx.state, ctx.source, ctx.controller)) return;
        ctx.events.logTrace("EVELYNN, IN CONTROL: hold with a facedown card here -> score 1");
        ctx.executor.scorePoints(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1085;
        d.def_id = R"RB(rad-109-167)RB";
        d.name = R"RB(Evelynn, In Control)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-109/167)RB";
        d.collector_number = 109;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Evelynn)RB", R"RB(Demon)RB"};   // printed card shows Demon too
        d.rarity = Rarity::Rare;
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.ability_text = R"RB([Disarm] (When I attack, give an enemy unit here -1 [M] this turn.)
When I hold, if you control a facedown card here, score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8c17158e6153eddb4b56abd8a09cbbd20600b049-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1085(CardRegistry& r) {
    r.registerCard(1085, std::make_unique<EvelynnInControl>());
}

}  // namespace riftbound
