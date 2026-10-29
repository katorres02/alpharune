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

class SeraphicWyvern : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "If you would channel runes, channel that many runes plus 1 exhausted
    //  rune instead." Replacement: each Wyvern adds 1 to
    // PlayerState::channel_extra_exhausted, which both channel paths
    // (EffectExecutor::channelRunes, GameEngine::channelRunes) apply.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject& self) {
            if (self.zone == ZoneType::Chain) return;
            state.player(controller).channel_extra_exhausted += 1;
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1072;
        d.def_id = R"RB(rad-092-167)RB";
        d.name = R"RB(Seraphic Wyvern)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-092/167)RB";
        d.collector_number = 92;
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Mount Targon)RB", R"RB(Dragon)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 7;
        d.power_cost = 0;
        d.might = 7;
        d.ability_text = R"RB(If you would channel runes, channel that many runes plus 1 exhausted rune instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ad1793554440d8d5017eadc565ca6c787a969de1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1072(CardRegistry& r) {
    r.registerCard(1072, std::make_unique<SeraphicWyvern>());
}

}  // namespace riftbound
