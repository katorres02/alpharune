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

// "You may have me enter ready. If you do, I can't move this turn."
// The choice is made as the unit is played (onPlay runs during the play,
// before it lands): a synchronous yes/no, recorded for entersReadyOnPlay.
// GAP: "can't move" blocks the unit's own moves (standard moves); effects that
// move it this turn are not blocked.
class EntrenchedBoar : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onPlay(CardContext& ctx) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const bool ready = ctx.executor.confirmNow(ctx.controller,
            "Entrenched Boar: enter ready (and can't move this turn)?", /*fallback=*/false);
        auto& me = ctx.state.getObject(ctx.source);
        if (!ready) { me.card_counters.erase("__boar_ready_turn"); return; }
        me.card_counters["__boar_ready_turn"] = ctx.state.turn.turn_number + 1;
        me.cant_move_turn = ctx.state.turn.turn_number;
        ctx.events.logTrace("ENTRENCHED BOAR: enters ready, can't move this turn");
    }
    bool entersReadyOnPlay(const GameState& state, PlayerId /*controller*/,
                           const GameObject& self) const override {
        auto it = self.card_counters.find("__boar_ready_turn");
        return it != self.card_counters.end() && it->second == state.turn.turn_number + 1;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1011;
        d.def_id = R"RB(rad-026-167)RB";
        d.name = R"RB(Entrenched Boar)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-026/167)RB";
        d.collector_number = 26;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        // Gallery lists no domain (pre-release data); #26 sits in the Calm
        // block (Consult the Heavens #25, Iridescent Avian #27).
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Freljord)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 2;
        d.ability_text = R"RB(You may have me enter ready. If you do, I can't move this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8d8688f43e219f7f4b5f5fd17cff204dcebc3bdc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1011(CardRegistry& r) {
    r.registerCard(1011, std::make_unique<EntrenchedBoar>());
}

}  // namespace riftbound
