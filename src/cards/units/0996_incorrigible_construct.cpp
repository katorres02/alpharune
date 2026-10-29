#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// "[Show Off] a Mech. I enter ready if you showed off."
class IncorrigibleConstruct : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasShowOff() const override { return true; }
    bool showOffMatches(const GameState&, PlayerId, const GameObject& candidate) const override {
        return hasTag(candidate, "Mech");
    }
    bool entersReadyOnPlay(const GameState&, PlayerId, const GameObject& self) const override {
        return self.showed_off_object != kInvalidId;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 996;
        d.def_id = R"RB(rad-011-167)RB";
        d.name = R"RB(Incorrigible Construct)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-011/167)RB";
        d.collector_number = 11;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Piltover)RB", R"RB(Mech)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 4;
        d.ability_text = R"RB([Show Off] a Mech. (As you play me, you may reveal a Mech from hand or pick another friendly Mech.)
I enter ready if you showed off.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e95470d5c2fbc9f7f8ab0b1c19f32fd704918c83-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_996(CardRegistry& r) {
    r.registerCard(996, std::make_unique<IncorrigibleConstruct>());
}

}  // namespace riftbound
