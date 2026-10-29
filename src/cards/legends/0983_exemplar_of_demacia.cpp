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

class ExemplarOfDemacia : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play a gear with [Deploy], you may exhaust me and pay [A] to
    // move a friendly unit to that battlefield."
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayACard; }
    // [Deploy] gear is played only to a battlefield (other gear only to base),
    // so a gear that lands at a battlefield is a Deploy gear; onTrigger checks
    // the card itself when the registry is at hand.
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId subject) const override {
        if (subject == kInvalidId || !state.objectExists(subject)) return false;
        const auto& g = state.getObject(subject);
        return g.isGear() && g.isAtBattlefield() && !g.attached_to.has_value();
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto& ri = ctx.state.chain.resuming;
        const GameObjectId gear = ri.has_value() ? ri->triggering_subject : kInvalidId;
        if (gear == kInvalidId || !ctx.state.objectExists(gear)) return;
        if (ctx.registry) {
            const Card* gc = ctx.registry->get(ctx.state.getObject(gear).card_def_id);
            if (!gc || !gc->hasDeploy()) return;
        }
        const auto bf = gearBattlefield(ctx.state.getObject(gear));
        if (!bf) return;
        auto movers = [&]() {
            const PlayerId me = ctx.controller;
            const BattlefieldId to = *bf;
            return chooseableUnits(ctx.state, me, [me, to](const GameObject& o) {
                auto obf = o.battlefieldId();
                return o.controller == me && !(obf && *obf == to);
            });
        };
        auto can_pay = [&]() {
            return ctx.state.objectExists(ctx.source) && !ctx.state.getObject(ctx.source).is_exhausted &&
                   ctx.executor.payableAnyPower(ctx.controller) >= 1 && !movers().empty();
        };
        const int go = confirmOptional(ctx, "Exemplar of Demacia: exhaust me and pay [A] to move a unit?",
                                       [&]() { return costAlreadyPaid(ctx) || can_pay(); });
        if (go <= 0) return;
        const GameObjectId unit = pickTarget(ctx, "Exemplar of Demacia: a friendly unit to move", movers());
        if (ctx.executor.hasPendingChoice()) return;
        if (unit == kInvalidId) return;
        if (costDueNow(ctx)) {
            if (!can_pay()) return;
            ctx.executor.exhaustObject(ctx.source);
            ctx.executor.payAnyPower(ctx.controller, 1);
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here
        if (!ctx.state.objectExists(unit) || !ctx.state.getObject(unit).location.has_value()) return;
        ctx.executor.moveToBattlefield(unit, *bf);
        ctx.events.logTrace("EXEMPLAR OF DEMACIA: moved " + ctx.state.getObject(unit).name);
    }
private:
    static std::optional<BattlefieldId> gearBattlefield(const GameObject& g) {
        if (auto bf = g.battlefieldId()) return bf;
        if (g.last_location && std::holds_alternative<BattlefieldLocation>(*g.last_location))
            return std::get<BattlefieldLocation>(*g.last_location).id;
        return std::nullopt;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 983;
        d.def_id = R"RB(rad-155-167)RB";
        d.name = R"RB(Exemplar of Demacia)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-155/167)RB";
        d.collector_number = 155;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body,Domain::Order};
        d.tags = {R"RB(Jarvan IV)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play a gear with [Deploy], you may exhaust me and pay [A] to move a friendly unit to that battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e85c005f938e44655f926bb406a47359ae51ecc1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_983(CardRegistry& r) {
    r.registerCard(983, std::make_unique<ExemplarOfDemacia>());
}

}  // namespace riftbound
