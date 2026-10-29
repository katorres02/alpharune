#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// [Deploy]. "While you or an ally control this battlefield, that player may
// hide an additional card here." One more facedown slot at the trap's
// battlefield while its controller controls that battlefield (1v1: no
// allies). The capacity is reset to 1 at the start of every aura pass.
class BushwhackTrap : public DeployGear {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject& self) {
            if (self.attached_to.has_value()) return;
            auto here = self.battlefieldId();
            if (!here) return;
            for (auto& bf : state.battlefields) {
                if (bf.id != *here) continue;
                if (bf.controller.has_value() && *bf.controller == self.controller)
                    bf.facedown_max_occupancy += 1;
            }
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1084;
        d.def_id = R"RB(rad-108-167)RB";
        d.name = R"RB(Bushwhack Trap)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-108/167)RB";
        d.collector_number = 108;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        // Gallery said Fury + "Device" tag; the printed card is Chaos, no tag.
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.rarity = Rarity::Rare;
        d.energy_cost = 1;
        d.power_cost = 0;
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
While you or an ally control this battlefield, that player may hide an additional card here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/120762e23a599ee14d3f88856fcf707154338fb0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1084(CardRegistry& r) {
    r.registerCard(1084, std::make_unique<BushwhackTrap>());
}

}  // namespace riftbound
