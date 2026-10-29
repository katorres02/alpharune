#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
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

class EkkoIngenious : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "As an additional cost to play me, return a friendly unit to its
    // owner's hand." A mandatory object cost, paid as I'm played (before my
    // rune cost); without a friendly unit I can't be played. "[Ganking]" is a
    // keyword.
    OptionalAdditionalCost optionalAdditionalCost() const override {
        OptionalAdditionalCost c;
        c.valid = true;
        c.mandatory = true;
        c.object_cost = OptionalAdditionalCost::ObjectCost::ReturnFriendlyUnit;
        return c;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1041;
        d.def_id = R"RB(rad-061-167)RB";
        d.name = R"RB(Ekko, Ingenious)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-061/167)RB";
        d.collector_number = 61;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Ekko)RB",R"RB(Zaun)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 4;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(As an additional cost to play me, return a friendly unit to its owner's hand.
[Ganking] (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3129469742d7aa5c6921ebae841ccee1923bb279-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1041(CardRegistry& r) {
    r.registerCard(1041, std::make_unique<EkkoIngenious>());
}

}  // namespace riftbound
