#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class BannerOfConquest : public DeployGear {
public:
    const CardDef& def() const override { return def_; }
    // [Deploy] comes from DeployGear.
    // "Friendly units here have [Assault]."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject& self) {
            const auto here = self.battlefieldId();
            if (!here || self.attached_to.has_value()) return;
            for (auto& [uid, u] : state.objects) {
                if (!u.isUnit() || u.controller != controller) continue;
                if (u.battlefieldId() != here) continue;
                auraKeyword(u, sid, Keyword::Assault);
            }
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 987;
        d.def_id = R"RB(rad-002-167)RB";
        d.name = R"RB(Banner of Conquest)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-002/167)RB";
        d.collector_number = 2;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 1;
        d.power_cost = 1;
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
Friendly units here have [Assault]. (+1 [M] while they're attackers.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1e2a4e0a8cf533c82e9c3f4d507dd7eb63ece943-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_987(CardRegistry& r) {
    r.registerCard(987, std::make_unique<BannerOfConquest>());
}

}  // namespace riftbound
