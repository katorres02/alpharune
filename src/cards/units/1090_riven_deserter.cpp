#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When another friendly unit becomes exhausted, give me +1 [M] this turn."
// "Exhaust a friendly unit: Ready a unit. Use this ability only once each turn."
// The cost unit (any ready friendly unit, me included) and the unit to ready
// are both chosen as the ability is activated; the cost is paid then.
class RivenDeserter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }

    TriggerType triggerType() const override {
        return TriggerType::WhenAnotherFriendlyUnitBecomesExhausted;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;
        ctx.executor.giveTemporaryMight(ctx.source, 1);
        ctx.events.logTrace("RIVEN, DESERTER: another friendly unit exhausted -> +1 [M]");
    }

    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {ActivatedAbility{.cost = {}, .targets = {}, .is_action = false,
                                 .is_reaction = false, .needs_activation_time_target = true}};
    }
    static std::vector<GameObjectId> readyFriendly(const GameState& state, PlayerId p) {
        std::vector<GameObjectId> out;
        for (const auto& [id, o] : state.objects)
            if (o.isUnit() && o.controller == p && o.location.has_value() && !o.is_exhausted)
                out.push_back(id);
        return out;
    }
    static bool usedThisTurn(const GameState& state, GameObjectId src) {
        if (!state.objectExists(src)) return true;
        const auto& c = state.getObject(src).card_counters;
        auto it = c.find("__rd_used_turn");
        return it != c.end() && it->second == state.turn.turn_number + 1;
    }
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!Card::canActivateAbilityAt(state, controller, source, ability_index)) return false;
        return !usedThisTurn(state, source) && !readyFriendly(state, controller).empty();
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId me = ctx.controller;
        const bool paid = costAlreadyPaid(ctx);
        // At resolution the cost unit is exhausted already; accept any friendly unit.
        std::vector<GameObjectId> cost_units = paid
            ? chooseableUnits(ctx.state, me, [me](const GameObject& o) { return o.controller == me; })
            : readyFriendly(ctx.state, me);
        auto [cost_unit, to_ready] = pickTargetPair(
            ctx, "Riven, Deserter: exhaust a friendly unit, then ready a unit", cost_units,
            [&](GameObjectId) {
                return chooseableUnits(ctx.state, me, [](const GameObject&) { return true; });
            });
        if (!paid) {
            if (cost_unit == kInvalidId || to_ready == kInvalidId) return;   // suspended / none
            if (costDueNow(ctx)) {
                if (!ctx.state.objectExists(cost_unit) ||
                    ctx.state.getObject(cost_unit).is_exhausted) return;
                ctx.executor.exhaustObject(cost_unit);
                if (ctx.state.objectExists(ctx.source))
                    ctx.state.getObject(ctx.source).card_counters["__rd_used_turn"] =
                        ctx.state.turn.turn_number + 1;
            }
        }
        if (finalizingChoices(ctx)) return;
        if (to_ready == kInvalidId || !ctx.state.objectExists(to_ready)) return;
        ctx.executor.readyObject(to_ready);
        ctx.events.logTrace("RIVEN, DESERTER: ready " + ctx.state.getObject(to_ready).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1090;
        d.def_id = R"RB(rad-114-167)RB";
        d.name = R"RB(Riven, Deserter)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-114/167)RB";
        d.collector_number = 114;
        d.artist = R"RB(Yeon Art Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Riven)RB", R"RB(Noxus)RB"};   // as printed
        d.rarity = Rarity::Epic;
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.ability_text = R"RB(When another friendly unit becomes exhausted, give me +1 [M] this turn.
Exhaust a friendly unit: Ready a unit. Use this ability only once each turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/59c080321a697164c9eeb6f0ad297ff0d0b85808-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1090(CardRegistry& r) {
    r.registerCard(1090, std::make_unique<RivenDeserter>());
}

}  // namespace riftbound
