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
#include <vector>

namespace riftbound {
namespace {

// "When a friendly gear at a battlefield dies, you may pay [1] to return it to
// its owner's hand." Triggers only for a gear that was at a battlefield
// (attached Equipment included) as it died. The "you may pay" is decided and
// paid as the ability is finalized (CR 383.3); at resolution the gear returns
// from its owner's trash if it is still there (a token gear ceased to exist).
class BattlefieldArmorer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyGearDies; }
    bool triggerCondition(const GameState& state, GameObjectId, PlayerId, TriggerType,
                          GameObjectId subject) const override {
        if (!state.objectExists(subject)) return false;
        const auto& g = state.getObject(subject);
        return g.last_location.has_value() &&
               std::holds_alternative<BattlefieldLocation>(*g.last_location);
    }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const GameObjectId gear = ctx.state.chain.resuming.has_value()
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        const bool paid = costAlreadyPaid(ctx);
        int go = confirmOptional(ctx, "Battlefield Armorer: pay [1] to return the gear to hand?",
                                 [&]() { return paid || payableEnergy(ctx.state, ctx.controller) >= 1; });
        if (go <= 0) return;
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (gear == kInvalidId || !ctx.state.objectExists(gear)) return;
        auto& g = ctx.state.getObject(gear);
        if (g.zone != ZoneType::Trash) return;
        auto& trash = ctx.state.player(g.owner).trash;
        auto it = std::find(trash.begin(), trash.end(), gear);
        if (it == trash.end()) return;
        trash.erase(it);
        g.zone = ZoneType::Hand;
        g.location = std::nullopt;
        ctx.state.player(g.owner).hand.push_back(gear);
        ctx.events.logTrace("BATTLEFIELD ARMORER: paid [1] -> " + g.name + " returned to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1106;
        d.def_id = R"RB(rad-130-167)RB";
        d.name = R"RB(Battlefield Armorer)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-130/167)RB";
        d.collector_number = 130;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Demacia)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.ability_text = R"RB(When a friendly gear at a battlefield dies, you may pay [1] to return it to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/eb04c39763de8fb8e56153d1f0e4f799e15dced9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1106(CardRegistry& r) {
    r.registerCard(1106, std::make_unique<BattlefieldArmorer>());
}

}  // namespace riftbound
