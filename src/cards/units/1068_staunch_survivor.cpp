#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class StaunchSurvivor : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // Vanilla: [Assault 2], [Shield 2] (engine keywords).
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1068;
        d.def_id = R"RB(rad-088-167)RB";
        d.name = R"RB(Staunch Survivor)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-088/167)RB";
        d.collector_number = 88;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Freljord)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 2;
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Shield);
        d.assault_value = 2;
        d.shield_value = 2;
        d.ability_text = R"RB([Assault 2], [Shield 2] (+2 [M] while I'm an attacker or defender.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/91c4670531d7773cae9e7b4bfd5ac4e259602162-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1068(CardRegistry& r) {
    r.registerCard(1068, std::make_unique<StaunchSurvivor>());
}

}  // namespace riftbound
