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
    // NOTE: "first unit or gear played here costs [1] less" is not modeled.
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
