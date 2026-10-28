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
#include <map>

namespace riftbound {
namespace {

class BedevilingStranger : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId mine = ctx.controller;
        std::map<BattlefieldId,int> count;
        for (auto& [id, o] : ctx.state.objects)
            if (o.isUnit()) if (auto bf = o.battlefieldId()) count[*bf]++;
        auto cands = chooseableUnits(ctx.state, mine, [&](const GameObject& o) {
            auto bf = o.battlefieldId();
            return o.controller != mine && bf && count[*bf] >= 4; });
        if (cands.empty()) return;
        GameObjectId t = pickTarget(ctx, "Bedeviling Stranger: return an enemy unit", cands);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.bounceToHand(t);
        ctx.events.logTrace("BEDEVILING STRANGER: returned a unit to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 959;
        d.def_id = R"RB(rad-107-167)RB";
        d.name = R"RB(Bedeviling Stranger)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-107/167)RB";
        d.collector_number = 107;
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([Hidden] When you play me, choose a battlefield. If there are 4 or more units there, you may return an enemy unit there to its owner's hand.)RB";
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 2;
        d.keywords = []{KeywordSet k; k.set(Keyword::Hidden); return k;}();
        return d;
    }();
};

}  // anonymous namespace

void register_card_959(CardRegistry& r) {
    r.registerCard(959, std::make_unique<BedevilingStranger>());
}

}  // namespace riftbound
