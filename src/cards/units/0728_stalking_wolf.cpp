#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

// [Ambush] engine-handled.
// "As an additional cost to play me, kill a Bird, Cat, Dog, or Poro you
//  control. You may [Ambush] me to its battlefield, even if you don't have
//  other units there." (Vendetta errata text.)
//
// A mandatory object cost (OptionalAdditionalCost): the player picks the
// victim as I'm played, before my rune cost is paid. Without a Bird/Cat/Dog/
// Poro I can't be played. The Ambush clause needs no widening: the victim is
// still at its battlefield when the play is generated, so the engine's Ambush
// generator offers it. When I'm Ambushed to a battlefield I don't control
// where my only units are such candidates, I'm there only thanks to "its
// battlefield", so the victim must be one of them (objectCostCandidates).

class StalkingWolf : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    static bool isPrey(const GameObject& obj) {
        return hasTag(obj, "Bird") || hasTag(obj, "Cat") ||
               hasTag(obj, "Dog") || hasTag(obj, "Poro");
    }

    OptionalAdditionalCost optionalAdditionalCost() const override {
        OptionalAdditionalCost c;
        c.valid = true;
        c.mandatory = true;
        c.object_cost = OptionalAdditionalCost::ObjectCost::KillFriendlyUnit;
        return c;
    }

    std::vector<GameObjectId> objectCostCandidates(
        const GameState& state, PlayerId player, GameObjectId self,
        const std::optional<LocationId>& play_location) const override {
        std::vector<GameObjectId> prey, here;
        bool other_units_here = false;
        std::optional<BattlefieldId> bf;
        if (play_location && std::holds_alternative<BattlefieldLocation>(*play_location))
            bf = std::get<BattlefieldLocation>(*play_location).id;
        for (const auto& [id, obj] : state.objects) {
            if (id == self || !obj.isUnit() || obj.controller != player) continue;
            if (!obj.location.has_value()) continue;
            const auto at = obj.battlefieldId();
            const bool is_here = bf && at && *at == *bf;
            if (isPrey(obj)) {
                prey.push_back(id);
                if (is_here) here.push_back(id);
            } else if (is_here) {
                other_units_here = true;
            }
        }
        if (bf && !here.empty() && !other_units_here) {
            bool controlled = false;
            for (const auto& b : state.battlefields)
                if (b.id == *bf) controlled = b.controller.has_value() && *b.controller == player;
            if (!controlled) prey = here;   // Ambushed here only thanks to "its battlefield"
        }
        std::sort(prey.begin(), prey.end());
        return prey;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 728;
        d.def_id = R"RB(unl-166-219)RB";
        d.name = R"RB(Stalking Wolf)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-166/219)RB";
        d.collector_number = 166;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Dog)RB", R"RB(Freljord)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
As an additional cost to play me, kill a Bird, Cat, Dog, or Poro you control. You may [Ambush] me to its battlefield, even if you don't have other units there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5f5f66caad21d59bf966ddc501bb4f9c84a595c6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_728(CardRegistry& r) {
    r.registerCard(728, std::make_unique<StalkingWolf>());
}

} // namespace riftbound
