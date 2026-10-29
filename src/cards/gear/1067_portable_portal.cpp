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

class PortablePortal : public DeployGear {
public:
    const CardDef& def() const override { return def_; }

    // [Deploy]: DeployGear + engine (play location, cleanup, opponent-hold kill).
    // "When you play this, you may pay [O] to ready a unit here." The "you
    // may", the [O] payment and the unit are decided as the trigger is
    // finalized (CR 383.3).
    static std::vector<GameObjectId> unitsHere(const GameState& state, PlayerId chooser,
                                               GameObjectId self) {
        if (!state.objectExists(self)) return {};
        const auto bf = state.getObject(self).battlefieldId();
        if (!bf) return {};
        return chooseableUnits(state, chooser, [&](const GameObject& o) {
            auto obf = o.battlefieldId();
            return o.zone != ZoneType::Chain && obf && *obf == *bf;
        });
    }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto still_legal = [&]() {
            return !unitsHere(ctx.state, ctx.controller, ctx.source).empty() &&
                   (costAlreadyPaid(ctx) ||
                    payableOnePower(ctx.state, ctx.controller, Domain::Body));
        };
        const int go = confirmOptional(ctx, "Portable Portal: pay [O] to ready a unit here?",
                                       still_legal);
        if (go <= 0) return;
        GameObjectId t = pickTarget(ctx, "Portable Portal: ready a unit here",
                                    unitsHere(ctx.state, ctx.controller, ctx.source));
        if (ctx.executor.hasPendingChoice()) return;
        if (costDueNow(ctx) && !payOnePower(ctx, ctx.controller, Domain::Body)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.readyObject(t);
        ctx.events.logTrace("PORTABLE PORTAL: paid [O] -> ready " + ctx.state.getObject(t).name);
    }
    // "Friendly units here have [Ganking]."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            if (self.zone == ZoneType::Chain) continue;
            const auto bf = self.battlefieldId();
            if (!bf) continue;
            for (auto& [uid, u] : state.objects) {
                if (!u.isUnit() || u.controller != controller || u.zone == ZoneType::Chain) continue;
                auto ubf = u.battlefieldId();
                if (ubf && *ubf == *bf) auraKeyword(u, sid, Keyword::Ganking);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1067;
        d.def_id = R"RB(rad-087-167)RB";
        d.name = R"RB(Portable Portal)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-087/167)RB";
        d.collector_number = 87;
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.rarity = Rarity::Rare;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When you play this, you may pay [O] to ready a unit here.
Friendly units here have [Ganking]. (They can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/dba5c879aae5c614198eb8408e0104a1b0618102-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1067(CardRegistry& r) {
    r.registerCard(1067, std::make_unique<PortablePortal>());
}

}  // namespace riftbound
