#include "cards/card.h"
#include "cards/card_helpers.h"
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

// "You may kill a friendly unit as an additional cost to play me. If you do,
// I cost [2] less." An optional object cost (CR 356.2): the player picks the
// unit (or declines) as I'm played, before my rune cost is paid; one kill
// takes [2] off my Energy cost. [Deflect] is an engine keyword.
class DuneDevourer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    OptionalAdditionalCost optionalAdditionalCost() const override {
        OptionalAdditionalCost c;
        c.valid = true;
        c.object_cost = OptionalAdditionalCost::ObjectCost::KillFriendlyUnit;
        c.min_objects = 1;
        c.max_objects = 1;
        c.reduce_energy_each = 2;
        return c;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1101;
        d.def_id = R"RB(rad-125-167)RB";
        d.name = R"RB(Dune Devourer)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-125/167)RB";
        d.collector_number = 125;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(The Void)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 7;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB(You may kill a friendly unit as an additional cost to play me. If you do, I cost [2] less.
[Deflect] (Opponents must pay [A] to choose me with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4f49bf38f0d3abb71e148783ebec97f446db8166-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1101(CardRegistry& r) {
    r.registerCard(1101, std::make_unique<DuneDevourer>());
}

}  // namespace riftbound
