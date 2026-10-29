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
#include <vector>

namespace riftbound {
namespace {

class HuntersCircle : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "For each player, the first unit or gear they play here on their turn
    // costs [1] less." BattlefieldState::first_play_here_discount; the engine
    // takes it off the first such play per player per turn (stamped in
    // first_play_here_turn) as it is paid. The play generators price each
    // location with it (GameEngine::firstPlayHereDiscount), so a card payable
    // only with the discount is offered to this battlefield.
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        for (auto& bf : state.battlefields) {
            if (!state.objectExists(bf.card_object_id)) continue;
            if (state.getObject(bf.card_object_id).card_def_id != cardDefId()) continue;
            bf.first_play_here_discount = 1;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 966;
        d.def_id = R"RB(rad-163-167)RB";
        d.name = R"RB(Hunters' Circle)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-163/167)RB";
        d.collector_number = 163;
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(For each player, the first unit or gear they play here on their turn costs [1] less.)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_966(CardRegistry& r) {
    r.registerCard(966, std::make_unique<HuntersCircle>());
}

}  // namespace riftbound
