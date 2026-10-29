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

class ClangingSentry : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Your Mechs have [Disarm]." Each Sentry grants its own [Disarm] (the
    // engine sums Disarm values into one attack trigger, CR 831.2).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject&) {
            for (auto& [id, o] : state.objects)
                if (o.isUnit() && o.controller == controller && o.location.has_value() &&
                    hasTag(o, "Mech"))
                    o.aura_disarm_value += 1;
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1035;
        d.def_id = R"RB(rad-055-167)RB";
        d.name = R"RB(Clanging Sentry)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-055/167)RB";
        d.collector_number = 55;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Piltover)RB", R"RB(Mech)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 2;
        d.ability_text = R"RB(Your Mechs have [Disarm]. (When each of us attacks, give an enemy unit there -1 [M] this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5d4c22ef863e041887a00d34b2dae1f3ce658326-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1035(CardRegistry& r) {
    r.registerCard(1035, std::make_unique<ClangingSentry>());
}

}  // namespace riftbound
