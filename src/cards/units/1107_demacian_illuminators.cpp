#include "cards/card.h"
#include "cards/card_helpers.h"
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

// "When one or more friendly units die, [Buff] a friendly unit." Friendly
// units dying at the same time announce each death; while this card's ability
// from that group still waits to be finalized it doesn't trigger again (the
// same rule TriggerManager applies to "When you discard one or more cards").
// The unit to buff is chosen as the ability is finalized (CR 383.3).
class DemacianIlluminators : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitDies; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId, TriggerType,
                          GameObjectId) const override {
        for (const auto& ci : state.chain.items)
            if (ci.source == source && ci.status == ChainItemStatus::Pending &&
                ci.fired_trigger == TriggerType::WhenAFriendlyUnitDies)
                return false;
        return true;
    }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId me = ctx.controller;
        auto friendly = chooseableUnits(ctx.state, me,
            [me](const GameObject& o) { return o.controller == me; });
        GameObjectId t = pickTarget(ctx, "Demacian Illuminators: buff a friendly unit", friendly);
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        if (ctx.executor.buffUnit(t))
            ctx.events.logTrace("DEMACIAN ILLUMINATORS: buffed " + ctx.state.getObject(t).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1107;
        d.def_id = R"RB(rad-131-167)RB";
        d.name = R"RB(Demacian Illuminators)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-131/167)RB";
        d.collector_number = 131;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Demacia)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.ability_text = R"RB(When one or more friendly units die, [Buff] a friendly unit. (Give it a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/127c2700ee4e2557dfd463ed84c45f4600ad9723-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1107(CardRegistry& r) {
    r.registerCard(1107, std::make_unique<DemacianIlluminators>());
}

}  // namespace riftbound
