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

class SirenOfTheShallows : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    // [Disarm]: When I attack, give an enemy unit here -1 [M] this turn.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        const BattlefieldId here = *my_bf; const PlayerId me = ctx.controller;
        GameObjectId t = pickTarget(ctx, "Siren of the Shallows [Disarm]",
            chooseableUnits(ctx.state, me, [here, me](const GameObject& o) {
                auto bf = o.battlefieldId(); return o.controller != me && bf && *bf == here; }));
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.giveTemporaryMight(t, -1);
        ctx.events.logTrace("SIREN OF THE SHALLOWS: disarm -1 might");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 960;
        d.def_id = R"RB(rad-097-167)RB";
        d.name = R"RB(Siren of the Shallows)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-097/167)RB";
        d.collector_number = 97;
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bilgewater)RB"};
        d.rarity = Rarity::Common;
        d.ability_text = R"RB([Disarm] (When I attack, give an enemy unit here -1 [M] this turn.) [Ganking])RB";
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 1;
        d.keywords = []{KeywordSet k; k.set(Keyword::Ganking); return k;}();
        return d;
    }();
};

}  // anonymous namespace

void register_card_960(CardRegistry& r) {
    r.registerCard(960, std::make_unique<SirenOfTheShallows>());
}

}  // namespace riftbound
