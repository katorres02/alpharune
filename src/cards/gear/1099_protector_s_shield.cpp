#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// [Quick-Draw] (engine: Reaction + attach on play). "[Equip] [1]". Grants
// [Tank] and +1 [M] to the equipped unit.
class ProtectorsShield : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasEquipAbility() const override { return true; }
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Tank); return k; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        if (!ctx.state.objectExists(unit)) return false;
        if (payableEnergy(ctx.state, ctx.controller) < 1) return false;
        if (!payEnergy(ctx, ctx.controller, 1)) return false;
        attachFree(ctx, ctx.source, unit);
        return true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1099;
        d.def_id = R"RB(rad-123-167)RB";
        d.name = R"RB(Protector's Shield)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-123/167)RB";
        d.collector_number = 123;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        // Gallery lists no domain/tag; the printed card is Order (power pip) Equipment.
        d.domains = {Domain::Order};
        d.tags = {R"RB(Equipment)RB"};
        d.rarity = Rarity::Common;
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might_bonus = 1;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::QuickDraw);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Quick-Draw] (This has [Reaction]. When you play it, attach it to a unit you control.)
[Equip] [1] ([1]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB([Tank] (I must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/97618bc30e9b65a2e5223e6f94db58d7098d30a6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1099(CardRegistry& r) {
    r.registerCard(1099, std::make_unique<ProtectorsShield>());
}

}  // namespace riftbound
