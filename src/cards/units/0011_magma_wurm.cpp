#include "cards/card.h"
#include "cards/card_registry.h"
#include "cards/card_helpers.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class MagmaWurm : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Other friendly units enter ready." Asked by resolvePermanent for each
    // unit its controller plays while I'm on the board.
    // GAP: units that enter the board other than by being played (tokens an
    // effect creates) still use their own entry state.
    bool grantsEnterReadyTo(const GameState& state, GameObjectId self,
                            const GameObject& played) const override {
        if (!state.objectExists(self) || played.id == self || !played.isUnit()) return false;
        return state.getObject(self).location.has_value();
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 11;
        d.def_id = R"RB(ogn-011-298)RB";
        d.name = R"RB(Magma Wurm)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-011/298)RB";
        d.collector_number = 11;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 8;
        d.power_cost = 1;
        d.might = 8;
        d.ability_text = R"RB(Other friendly units enter ready.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f6565b15f65f538804e6a56623c8aa2eedeffc22-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_11(CardRegistry& r) {
    r.registerCard(11, std::make_unique<MagmaWurm>());
}

} // namespace riftbound
