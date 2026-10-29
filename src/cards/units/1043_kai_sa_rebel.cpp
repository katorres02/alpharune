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

class KaiSaRebel : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Disarm]" engine-handled. "While I'm in a showdown, your spells have
    // [Reaction]." -> PlayerState::spells_have_reaction while I'm at a
    // battlefield with a showdown (or combat) in progress.
    int disarmValue() const override { return 1; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject& self) {
            auto bf = self.battlefieldId();
            if (!bf) return;
            for (const auto& b : state.battlefields)
                if (b.id == *bf && (b.showdown_in_progress || b.combat_in_progress))
                    state.player(controller).spells_have_reaction = true;
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1043;
        d.def_id = R"RB(rad-063-167)RB";
        d.name = R"RB(Kai'Sa, Rebel)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-063/167)RB";
        d.collector_number = 63;
        d.artist = R"RB(Calf Art Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Kai'Sa)RB",R"RB(The Void)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.ability_text = R"RB([Disarm] (When I attack, give an enemy unit here -1 [M] this turn).
While I'm in a showdown, your spells have [Reaction]. (Play them any time, even before spells and abilities resolve.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/53a7fdbb66f0d05111f4a7f54853db30f62fa5c4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1043(CardRegistry& r) {
    r.registerCard(1043, std::make_unique<KaiSaRebel>());
}

}  // namespace riftbound
