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

class AmateurDemolitionist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, play a Bomb gear token to a battlefield."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<std::string> labels;
        const auto bfs = battlefieldOptions(ctx.state, &labels);
        auto where = pickSeveral(ctx, ctx.controller, "Amateur Demolitionist: Bomb to which battlefield",
                                 bfs, true, labels, 1, 1, 8);
        if (!where || where->empty()) return;
        createBombToken(ctx, ctx.controller, static_cast<BattlefieldId>((*where)[0]));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 986;
        d.def_id = R"RB(rad-001-167)RB";
        d.name = R"RB(Amateur Demolitionist)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-001/167)RB";
        d.collector_number = 1;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bandle City)RB", R"RB(Yordle)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 1;
        d.ability_text = R"RB(When you play me, play a Bomb gear token to a battlefield. (It has [Deploy]. When it dies, deal 2 to an enemy there and kill your other Bombs there.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3dca5e04ff8dff983993f87d5a82a7fd56d7bc9c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_986(CardRegistry& r) {
    r.registerCard(986, std::make_unique<AmateurDemolitionist>());
}

}  // namespace riftbound
