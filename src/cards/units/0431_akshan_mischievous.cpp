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
    // Two "When you play me" abilities, each its own chain item
    // (Card::triggeredAbilityCount; the player orders them as they are
    // finalized): 0 = [Weaponmaster]; 1 = "if you paid the additional cost,
    // move an enemy gear to your base. You control it until I leave the
    // board. If it's an Equipment, attach it to me." — which only triggers
    // if the cost was paid.
    int triggeredAbilityCount(const GameState& state, GameObjectId source,
                              TriggerType fired) const override {
        if (fired != TriggerType::WhenYouPlayMe) return 1;
        if (!state.objectExists(source)) return 1;
        const auto& c = state.getObject(source).card_counters;
        auto it = c.find("__akshan_paid");
        return (it != c.end() && it->second == 1) ? 2 : 1;
    }
    std::string triggeredAbilityLabel(TriggerType, int index) const override {
        return index == 0 ? "[Weaponmaster]" : "Move an enemy gear to your base";
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.trigger_index == 0) {
            weaponmaster(ctx);   // choices at finalization, then the equip
            return;
        }
        // "if you paid the additional cost" — checked again as it resolves.
        if (triggeredAbilityCount(ctx.state, ctx.source, TriggerType::WhenYouPlayMe) < 2) return;
        // The gear is a target, chosen as the ability is finalized (CR 383.3).
        const GameObjectId gear = pickTarget(ctx, "Akshan, Mischievous: move which enemy gear",
                                             enemyGear(ctx));
        if (ctx.executor.hasPendingChoice()) return;   // waiting for the agent
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        stealEnemyGear(ctx, gear);
    }

private:
    std::vector<GameObjectId> enemyGear(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : ctx.state.objects)
            if (obj.isGear() && obj.controller != ctx.controller && obj.location.has_value())
                out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    void stealEnemyGear(CardContext& ctx, GameObjectId target_gear) {
        if (!ctx.state.objectExists(ctx.source)) return;
        const PlayerId me = ctx.controller;
        if (target_gear == kInvalidId || !ctx.state.objectExists(target_gear)) return;
        auto& gear = ctx.state.getObject(target_gear);
        if (gear.controller == me || !gear.location.has_value()) return;
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
