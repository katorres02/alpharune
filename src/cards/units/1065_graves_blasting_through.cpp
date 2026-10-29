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

class GravesBlastingThrough : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When I attack or defend, deal damage equal to my Might to all enemy
    //  units here." No choices: every enemy unit at my battlefield.
    TriggerType triggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& me = ctx.state.getObject(ctx.source);
        const auto my_bf = me.battlefieldId();
        const int dmg = me.current_might;
        if (!my_bf || dmg <= 0) return;
        std::vector<GameObjectId> enemies;   // collect first, then damage
        for (const auto& [id, o] : ctx.state.objects) {
            if (!o.isUnit() || o.controller == ctx.controller || o.zone == ZoneType::Chain) continue;
            auto bf = o.battlefieldId();
            if (bf && *bf == *my_bf) enemies.push_back(id);
        }
        for (auto id : enemies) ctx.executor.dealDamage(id, dmg, ctx.source);
        ctx.events.logTrace("GRAVES, BLASTING THROUGH: " + std::to_string(dmg) + " to " +
                            std::to_string(enemies.size()) + " enemy unit(s) here");
    }
    // "I don't deal combat damage." — a self aura (GameObject::dealsNoCombatDamage).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            if (!self.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.suppress_combat_damage = true;
            self.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1065;
        d.def_id = R"RB(rad-085-167)RB";
        d.name = R"RB(Graves, Blasting Through)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-085/167)RB";
        d.collector_number = 85;
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Bilgewater)RB",R"RB(Graves)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.ability_text = R"RB(When I attack or defend, deal damage equal to my Might to all enemy units here.
I don't deal combat damage.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5bc0671f9f78630e2932052bb9f7f42ef2742c86-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1065(CardRegistry& r) {
    r.registerCard(1065, std::make_unique<GravesBlastingThrough>());
}

}  // namespace riftbound
