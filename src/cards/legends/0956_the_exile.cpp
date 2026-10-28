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

class TheExile : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 956;
        d.def_id = R"RB(rad-149-167)RB";
        d.name = R"RB(The Exile)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-149/167)RB";
        d.collector_number = 149;
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm,Domain::Chaos};
        d.tags = {R"RB(Riven)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(Units you play to battlefields you control enter ready.)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_956(CardRegistry& r) {
    r.registerCard(956, std::make_unique<TheExile>());
}

}  // namespace riftbound
