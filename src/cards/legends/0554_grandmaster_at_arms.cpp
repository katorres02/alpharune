#include "cards/card.h"
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

class GrandmasterAtArms : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    // Both abilities choose an Equipment you control and then a unit you
    // control (pickTargetPair at activation — kFinalizeActivate — so the
    // player picks the Equipment instead of the engine taking the first one).
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility a0;  // [1],[E]: attach a detached Equipment
        a0.cost = ActivationCost{.exhaust = true, .energy = 1};
        ActivatedAbility a1;  // [E]: attach an attached Equipment elsewhere
        a1.cost = ActivationCost{.exhaust = true};
        return {a0, a1};
    }
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }

    static bool isEquipment(const GameObject& g) {
        if (!g.isGear()) return false;
        for (const auto& t : g.tags) if (t == "Equipment") return true;
        return false;
    }
    static std::vector<GameObjectId> equipment(const GameState& state, PlayerId me,
                                               bool attached) {
        std::vector<GameObjectId> out;
        for (auto& [id, g] : state.objects) {
            if (g.controller != me || !isEquipment(g) || !g.location.has_value()) continue;
            if (g.attached_to.has_value() != attached) continue;
            out.push_back(id);
        }
        return out;
    }
    static std::vector<GameObjectId> units(const GameState& state, PlayerId me,
                                           GameObjectId except = kInvalidId) {
        std::vector<GameObjectId> out;
        for (auto& [id, u] : state.objects)
            if (id != except && u.isUnit() && u.controller == me && u.location.has_value())
                out.push_back(id);
        return out;
    }
    // Offered only when there is an Equipment to attach and a unit to take it.
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!Card::canActivateAbilityAt(state, controller, source, ability_index)) return false;
        const bool attached = ability_index == 1;
        for (auto g : equipment(state, controller, attached)) {
            GameObjectId bearer = attached ? *state.getObject(g).attached_to : kInvalidId;
            if (!units(state, controller, bearer).empty()) return true;
        }
        return false;
    }

    void onActivate(CardContext& ctx, int ability_index,
                    const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId me = ctx.controller;
        const bool attached = ability_index == 1;
        auto bearerOf = [&](GameObjectId g) {
            return (ctx.state.objectExists(g) && ctx.state.getObject(g).attached_to.has_value())
                ? *ctx.state.getObject(g).attached_to : kInvalidId;
        };
        auto [gear, unit] = pickTargetPair(ctx, "Grandmaster: attach which Equipment, to which unit",
            equipment(ctx.state, me, attached),
            [&](GameObjectId g) { return units(ctx.state, me, bearerOf(g)); });
        if (ctx.executor.hasPendingChoice()) return;   // waiting for the agent
        if (finalizingChoices(ctx)) return;            // choices end here (CR 355)
        if (gear == kInvalidId || unit == kInvalidId ||
            !ctx.state.objectExists(unit) || !ctx.state.objectExists(gear)) return;
        auto& g = ctx.state.getObject(gear);
        // Detach from current bearer first (refund its might bonus).
        if (g.attached_to.has_value()) {
            ctx.executor.unattachGear(gear);
        }
        // Attach to the chosen unit (mirror Akshan's inline attach).
        auto& tgt = ctx.state.getObject(unit);
        g.attached_to = unit;
        tgt.attachments.push_back(gear);
        g.location = tgt.location;
        g.zone = tgt.zone;
        g.controller = me;
        tgt.attachment_might_bonus += g.might_bonus;
        tgt.recomputeMight();
        ctx.events.emit(ObjectStateChangedEvent{gear, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{unit, "equipped"});
        ctx.events.logTrace("GRANDMASTER AT ARMS: attached " + g.name +
                            " to " + tgt.name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 554;
        d.def_id = R"RB(sfd-245-221)RB";
        d.name = R"RB(Grandmaster at Arms)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-245/221)RB";
        d.collector_number = 245;
        d.artist = R"RB(Sugar Free)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(Jax)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB([1], [E]: Attach a detached Equipment you control to a unit you control.
[E]: Attach an attached Equipment you control to a unit you control.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/941f72a422e0143524b3dd0cba1fd87e4286ecb4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_554(CardRegistry& r) {
    r.registerCard(554, std::make_unique<GrandmasterAtArms>());
}

} // namespace riftbound
