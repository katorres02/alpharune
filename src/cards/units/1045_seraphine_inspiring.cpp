#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace riftbound {
namespace {

class SeraphineInspiring : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // [Disarm 2] — the engine queues the attack trigger (CR 831).
    int disarmValue() const override { return 2; }
    // "Once each turn, when you exhaust three or more friendly units at the
    // same time, you may draw 1." Units exhausted together = the units of one
    // standard move (GameEngine::executeStandardMove emits "exhausted_together"
    // with the group size in PlayerState::last_exhaust_group_size).
    // GAP: other simultaneous exhausts (an effect exhausting several units at
    // once) don't report a group yet; only standard moves do.
    TriggerType triggerType() const override { return TriggerType::WhenYouExhaustUnitsTogether; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId) const override {
        if (state.player(controller).last_exhaust_group_size < 3) return false;
        if (!state.objectExists(source)) return false;
        const auto& me = state.getObject(source);
        auto it = me.card_counters.find("__sera_insp_turn");
        return it == me.card_counters.end() || it->second != state.turn.turn_number;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        int go = confirmOptional(ctx, "Seraphine, Inspiring: draw 1?", [] { return true; });
        if (go == -1) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters["__sera_insp_turn"] = ctx.state.turn.turn_number;
        if (go == 1) ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1045;
        d.def_id = R"RB(rad-065-167)RB";
        d.name = R"RB(Seraphine, Inspiring)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-065/167)RB";
        d.collector_number = 65;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Piltover)RB",R"RB(Seraphine)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.ability_text = R"RB([Disarm 2] (When I attack, give an enemy unit here -2 [M] this turn.)
Once each turn, when you exhaust three or more friendly units at the same time, you may draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/60e0711c64958cc3a59a3bd3bf4c013758b1b92b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1045(CardRegistry& r) {
    r.registerCard(1045, std::make_unique<SeraphineInspiring>());
}

}  // namespace riftbound
