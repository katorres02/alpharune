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

class BloodscentSwarm : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "I cost [2] less for each battlefield you've conquered this turn."
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        int n = 0;
        for (const auto& bf : state.battlefields)
            if (bf.conquered_on_turn == state.turn.turn_number && bf.conquered_by_player == player) ++n;
        return 2 * n;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1060;
        d.def_id = R"RB(rad-080-167)RB";
        d.name = R"RB(Bloodscent Swarm)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-080/167)RB";
        d.collector_number = 80;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Shurima)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 8;
        d.ability_text = R"RB(I cost [2] less for each battlefield you've conquered this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/775ccea752fb404be2433c4b870c7877e2445a8c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1060(CardRegistry& r) {
    r.registerCard(1060, std::make_unique<BloodscentSwarm>());
}

}  // namespace riftbound
