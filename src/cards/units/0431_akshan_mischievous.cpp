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
#include "cards/units/weaponmaster_base.h"

namespace riftbound {
namespace {

class AkshanMischievous : public WeaponmasterUnit {
public:
    const CardDef& def() const override { return def_; }
    // "You may pay [O][O] as an additional cost to play me." [O] is Body.
    OptionalAdditionalCost optionalAdditionalCost() const override {
        return {/*valid=*/true, /*energy=*/0, /*power=*/2, Domain::Body,
                /*any_domain=*/false, /*paid_flag=*/"__akshan_paid"};
    }
    // Two play effects: [Weaponmaster] and "When you play me, if you paid the
    // additional cost, move an enemy gear to your base. ...".
    // GAP: the engine fires one chain item per trigger type, so both play
    // effects share one item: the Weaponmaster choices are made at
    // finalization, then (as it resolves) the gear is stolen, then the
    // Weaponmaster equip is paid. Their relative order is not the player's.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId wm_gear = weaponmasterChoice(ctx);   // slots 0-8
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)

        if (!stealEnemyGear(ctx)) return;     // waiting for the gear pick
        weaponmasterEquip(ctx, wm_gear);
    }

private:
    /// Returns false while waiting for the agent's pick.
    bool stealEnemyGear(CardContext& ctx) {
        if (!ctx.state.objectExists(ctx.source)) return true;
        auto& self_obj = ctx.state.getObject(ctx.source);
        if (self_obj.card_counters["__akshan_paid"] != 1) return true;
        const PlayerId me = ctx.controller;
        std::vector<GameObjectId> enemy_gear;
        for (auto& [id, obj] : ctx.state.objects)
            if (obj.isGear() && obj.controller != me && obj.location.has_value())
                enemy_gear.push_back(id);
        std::sort(enemy_gear.begin(), enemy_gear.end());
        // Not a "choose": the gear is picked as the ability resolves. Uses the
        // pickTargetPair slots (9-13) since the Weaponmaster choice holds 0-8.
        auto picked = pickTargetPair(ctx, "Akshan, Mischievous: move which enemy gear",
            enemy_gear, [](GameObjectId) { return std::vector<GameObjectId>{}; });
        if (ctx.executor.hasPendingChoice()) return false;
        const GameObjectId target_gear = picked.first;
        if (target_gear == kInvalidId || !ctx.state.objectExists(target_gear)) return true;
        auto& gear = ctx.state.getObject(target_gear);
        if (gear.controller == me || !gear.location.has_value()) return true;
        const bool is_equipment = isEquipment(gear);
        if (gear.attached_to.has_value()) ctx.executor.unattachGear(target_gear);

        // "You control it until I leave the board." Reverts to the original
        // owner via GameEngine::revertLapsedControl once this Akshan is gone.
        ctx.executor.takeControlUntilSourceLeaves(target_gear, me, ctx.source);
        ctx.executor.moveToBase(target_gear);
        ctx.events.logTrace("AKSHAN: stole enemy gear until I leave the board");

        // "If it's an Equipment, attach it to me."
        if (is_equipment && ctx.state.objectExists(target_gear) &&
            ctx.state.objectExists(ctx.source) &&
            ctx.state.getObject(ctx.source).location.has_value()) {
            ctx.state.getObject(target_gear).controller = me;
            attachFree(ctx, target_gear, ctx.source);
            ctx.events.logTrace("AKSHAN: attached stolen Equipment to me");
        }
        return true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 431;
        d.def_id = R"RB(sfd-109-221)RB";
        d.name = R"RB(Akshan, Mischievous)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-109/221)RB";
        d.collector_number = 109;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Akshan)RB", R"RB(Shurima)RB", R"RB(Sentinel)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster]
You may pay [O][O] as an additional cost to play me.
When you play me, if you paid the additional cost, move an enemy gear to your base. You control it until I leave the board. If it's an Equipment, attach it to me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f9e79f88463c1d516b9f1b053661937676e0e1f4-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_431(CardRegistry& r) {
    r.registerCard(431, std::make_unique<AkshanMischievous>());
}

} // namespace riftbound
