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

class RabadonSDeathcrown : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasEquipAbility() const override { return true; }
    // [Weaponmaster] prices it through this (CR 821.1.c).
    EquipCostSpec equipCost(const GameState&, GameObjectId, GameObjectId) const override {
        return {true, 0, {{}}};   // [Equip] [A]
    }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        // [A] equip: recycle any rune for power.
        return standardEquip(ctx, ctx.source, unit, std::nullopt);
    }
    // "Your spells and abilities deal 3 Bonus Damage (while this is attached)."
    // Raises the controller's PlayerState::bonus_damage_dealt (reset each aura
    // pass; EffectExecutor::dealDamage adds it to spell/ability damage).
    // [Unique] keeps it to one copy, so one instance's +3 is applied per pass.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (const auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            if (!obj.location.has_value() || !obj.attached_to.has_value()) continue;
            state.player(controller).bonus_damage_dealt += 3;
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 508;
        d.def_id = R"RB(sfd-191-221)RB";
        d.name = R"RB(Rabadon's Deathcrown)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-191/221)RB";
        d.collector_number = 191;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Ornn)RB", R"RB(Equipment)RB"};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.might_bonus = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::Unique);
        d.ability_text = R"RB([Unique] (Your deck can have only 1 card with this name.)
[Equip] [A] ([A]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(Your spells and abilities deal 3 Bonus Damage (while this is attached).)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e93d11bdae446762b1932f0327d2ffe489fa4481-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_508(CardRegistry& r) {
    r.registerCard(508, std::make_unique<RabadonSDeathcrown>());
}

} // namespace riftbound
