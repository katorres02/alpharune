#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class LadyOfClockwork : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "At the start of your Main Phase, you may pay [1] to choose a friendly
    // gear. It becomes a unit gear with Might equal to its Energy cost until
    // you choose another gear with me." (CR 178: GameObject::also_unit.)
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::AtStartOfMain; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto can = [&]() {
            return payableEnergy(ctx.state, ctx.controller) >= 1 &&
                   !friendlyGear(ctx.state, ctx.controller).empty();
        };
        const int go = confirmOptional(ctx, "Lady of Clockwork: pay [1] to choose a friendly gear?",
                                       [&]() { return costAlreadyPaid(ctx) || can(); });
        if (go <= 0) return;
        const GameObjectId gear = pickTarget(ctx, "Lady of Clockwork: a friendly gear",
                                             friendlyGear(ctx.state, ctx.controller));
        if (ctx.executor.hasPendingChoice()) return;
        if (gear == kInvalidId) return;
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here
        if (!ctx.state.objectExists(ctx.source) || !ctx.state.objectExists(gear)) return;
        auto& me = ctx.state.getObject(ctx.source);
        // The previous choice stops being a unit.
        const int prev = me.card_counters[kChosen] - 1;
        if (prev > 0 && static_cast<GameObjectId>(prev) != gear &&
            ctx.state.objectExists(static_cast<GameObjectId>(prev)))
            stopBeingUnit(ctx.state.getObject(static_cast<GameObjectId>(prev)));
        auto& g = ctx.state.getObject(gear);
        // Units can't be attached to anything: it detaches as it becomes one.
        if (g.attached_to.has_value()) ctx.executor.unattachGear(gear);
        g.also_unit = true;
        g.base_might = energyCostOf(ctx.executor.cardDB(), g);
        g.recomputeMight();
        me.card_counters[kChosen] = static_cast<int>(gear) + 1;
        ctx.events.logTrace("LADY OF CLOCKWORK: " + g.name + " becomes a unit gear (" +
                            std::to_string(g.base_might) + " [M])");
    }
    // Keeps the chosen gear a unit; the choice ends if it leaves the board.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject& me) {
            auto it = me.card_counters.find(kChosen);
            if (it == me.card_counters.end() || it->second <= 0) return;
            const auto gid = static_cast<GameObjectId>(it->second - 1);
            if (!state.objectExists(gid)) { me.card_counters.erase(it); return; }
            auto& g = state.getObject(gid);
            if (!g.isGear() || !g.location.has_value()) {
                stopBeingUnit(g);
                me.card_counters.erase(it);
                return;
            }
            g.also_unit = true;
        });
    }
private:
    static constexpr const char* kChosen = "__lady_unit_gear";   // gear id + 1
    static std::vector<GameObjectId> friendlyGear(const GameState& state, PlayerId player) {
        std::vector<GameObjectId> out;
        for (const auto& [id, o] : state.objects)
            if (o.isGear() && o.controller == player && o.location.has_value()) out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    static int energyCostOf(const CardDB& db, const GameObject& g) {
        if (g.card_def_id == kInvalidId) return 0;
        try { return db.get(g.card_def_id).energy_cost; } catch (...) { return 0; }
    }
    static void stopBeingUnit(GameObject& g) {
        if (!g.also_unit) return;
        g.also_unit = false;
        g.base_might = 0;
        g.damage_marked = 0;
        g.combat_designation = CombatDesignation::None;
        g.recomputeMight();
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 975;
        d.def_id = R"RB(rad-145-167)RB";
        d.name = R"RB(Lady of Clockwork)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-145/167)RB";
        d.collector_number = 145;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm,Domain::Mind};
        d.tags = {R"RB(Orianna)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(At the start of your Main Phase, you may pay [1] to choose a friendly gear. It becomes a unit gear with Might equal to its Energy cost until you choose another gear with me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d1f940425d4d913d8f56c017254343bc898f4892-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_975(CardRegistry& r) {
    r.registerCard(975, std::make_unique<LadyOfClockwork>());
}

}  // namespace riftbound
