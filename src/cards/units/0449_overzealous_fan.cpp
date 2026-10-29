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

class OverzealousFan : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIDefend; }
    // "When I defend, you may kill me to move an attacking unit to its base."
    // At finalization: the "may", which attacking unit, and the cost (kill
    // me). At resolution: move that unit to its base if it is still on the
    // board (combat designations may already be cleared by then).
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId me = ctx.controller;
        const bool resolving_after_finalize =
            ctx.state.chain.resuming.has_value() && !ctx.state.chain.resuming->finalizing &&
            ctx.state.chain.resuming->choices_finalized;
        auto attackers = [&]() {
            return chooseableUnits(ctx.state, me, [&](const GameObject& u) {
                if (u.controller == me) return false;
                if (resolving_after_finalize) return u.isAtBattlefield();
                return u.combat_designation == CombatDesignation::Attacker;
            });
        };
        auto self_on_board = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   ctx.state.getObject(ctx.source).location.has_value();
        };
        int conf = confirmOptional(ctx,
            "Overzealous Fan: kill me to move an attacking unit to its base?",
            [&]() { return (costAlreadyPaid(ctx) || self_on_board()) && !attackers().empty(); });
        if (conf == -1) return;
        if (conf == 0) { finalizingChoices(ctx); return; }
        GameObjectId attacker = pickTarget(ctx, "Overzealous Fan: an attacking unit", attackers());
        if (ctx.executor.hasPendingChoice()) return;
        if (costDueNow(ctx)) {
            if (attacker == kInvalidId || !self_on_board()) return;
            ctx.executor.killObject(ctx.source);   // cost: "kill me"
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (attacker == kInvalidId || !ctx.state.objectExists(attacker)) return;
        if (!ctx.state.getObject(attacker).location.has_value()) return;
        std::string attacker_name = ctx.state.getObject(attacker).name;
        ctx.executor.moveToBase(attacker);
        ctx.events.logTrace("OVERZEALOUS FAN: killed self, moved " + attacker_name + " to its base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 449;
        d.def_id = R"RB(sfd-128-221)RB";
        d.name = R"RB(Overzealous Fan)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-128/221)RB";
        d.collector_number = 128;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.ability_text = R"RB(When I defend, you may kill me to move an attacking unit to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/922d7f337fbf14f0d62d43f69ca2a1e7480dd022-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_449(CardRegistry& r) {
    r.registerCard(449, std::make_unique<OverzealousFan>());
}

} // namespace riftbound
