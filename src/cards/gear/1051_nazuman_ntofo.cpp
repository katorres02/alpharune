#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace riftbound {
namespace {

class NazumanNtofo : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "Equip [1]": pay 1 Energy, then attach to a unit you control.
    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        if (!ctx.state.objectExists(unit)) return false;
        if (!payEnergy(ctx, ctx.controller, 1)) return false;
        attachFree(ctx, ctx.source, unit);
        return true;
    }
    // Effect text "[Shield 2]" is given to the equipped unit.
    int equippedShield() const override { return 2; }
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Shield); return k; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1051;
        d.def_id = R"RB(rad-071-167)RB";
        d.name = R"RB(Nazuman Ntofo)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-071/167)RB";
        d.collector_number = 71;
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Equipment)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 1;
        d.power_cost = 0;
        d.might_bonus = 0;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB(Equip [1] ([1]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB([Shield 2] (+2 [M] while I'm a defender.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9d518a6db3ae174096b09cf80cece2997f685ede-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1051(CardRegistry& r) {
    r.registerCard(1051, std::make_unique<NazumanNtofo>());
}

}  // namespace riftbound
