#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// "I enter ready.
//  When you play me, ready all enemy units.
//  Other units enter ready.
//  Units don't ready during the Awaken Phase."
// The two continuous effects set TurnState::all_units_enter_ready and
// TurnState::units_dont_awaken while I'm on the board (reset every aura pass).
class FirebrandRaider : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool entersReadyOnPlay() const override { return true; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> enemies;
        for (const auto& [id, o] : ctx.state.objects)
            if (o.isUnit() && o.location.has_value() && o.controller != ctx.controller && o.is_exhausted)
                enemies.push_back(id);
        for (auto id : enemies) ctx.executor.readyObject(id);
        ctx.events.logTrace("FIREBRAND RAIDER: readied " + std::to_string(enemies.size()) +
                            " enemy unit(s)");
    }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (const auto& [id, o] : state.objects) {
            if (o.card_def_id != cardDefId() || o.controller != controller) continue;
            if (!o.location.has_value()) continue;
            state.turn.all_units_enter_ready = true;
            state.turn.units_dont_awaken = true;
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1004;
        d.def_id = R"RB(rad-019-167)RB";
        d.name = R"RB(Firebrand Raider)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-019/167)RB";
        d.collector_number = 19;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Freljord)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.ability_text = R"RB(I enter ready.
When you play me, ready all enemy units.
Other units enter ready.
Units don't ready during the Awaken Phase.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/922ef5a266378fa8ebd7e75bf83435bacd1a75d8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1004(CardRegistry& r) {
    r.registerCard(1004, std::make_unique<FirebrandRaider>());
}

}  // namespace riftbound
