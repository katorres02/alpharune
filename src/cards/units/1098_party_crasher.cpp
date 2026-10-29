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

// "I cost [1] less for each exhausted unit you control." [Tank].
class PartyCrasher : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        int n = 0;
        for (const auto& [id, o] : state.objects)
            if (o.isUnit() && o.controller == player && o.location.has_value() && o.is_exhausted) ++n;
        return n;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1098;
        d.def_id = R"RB(rad-122-167)RB";
        d.name = R"RB(Party Crasher)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-122/167)RB";
        d.collector_number = 122;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Zaun)RB"};   // as printed
        d.rarity = Rarity::Common;
        d.energy_cost = 9;
        d.power_cost = 1;
        d.might = 6;
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB(I cost [1] less for each exhausted unit you control.
[Tank] (I must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3413fc46086d559c2a60657e4729f22475bbdec0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1098(CardRegistry& r) {
    r.registerCard(1098, std::make_unique<PartyCrasher>());
}

}  // namespace riftbound
