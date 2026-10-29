#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// "I enter ready if a friendly unit or gear died this turn."
class LingeringAnimus : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        const auto& ps = state.player(controller);
        return ps.unit_died_this_turn || ps.gear_died_this_turn;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 990;
        d.def_id = R"RB(rad-005-167)RB";
        d.name = R"RB(Lingering Animus)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-005/167)RB";
        d.collector_number = 5;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Spirit)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 2;
        d.ability_text = R"RB(I enter ready if a friendly unit or gear died this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5453008d27b3bb4f00941cb5a0d9ca52e4eb0245-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_990(CardRegistry& r) {
    r.registerCard(990, std::make_unique<LingeringAnimus>());
}

}  // namespace riftbound
