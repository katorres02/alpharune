#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/deploy_base.h"
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

class PiltovanGuidebot : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you play a card, you may have my base Might become equal to that
    // card's Energy cost this turn." Modelled like Dragon Form: a this-turn
    // Might delta of (cost - printed base). A later "become" replaces the
    // earlier one, so the delta applied this turn is tracked and swapped.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayACard; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        GameObjectId played = ctx.state.chain.resuming.has_value()
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        if (played == kInvalidId || !ctx.state.objectExists(played)) return;
        int go = confirmOptional(ctx, "Piltovan Guidebot: base Might becomes that card's Energy cost?",
                                 [&] { return ctx.state.objectExists(ctx.source); });
        if (go == -1) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (go != 1 || !ctx.state.objectExists(ctx.source) || !ctx.state.objectExists(played)) return;
        const auto& p = ctx.state.getObject(played);
        int cost = 0;
        if (!p.isToken() && p.card_def_id != kInvalidId)
            cost = ctx.executor.cardDB().get(p.card_def_id).energy_cost;
        auto& me = ctx.state.getObject(ctx.source);
        const int turn = ctx.state.turn.turn_number;
        int old_delta = 0;
        if (me.card_counters["__pgb_turn"] == turn + 1) old_delta = me.card_counters["__pgb_delta"];
        const int delta = cost - me.base_might;
        me.temp_might_bonus += delta - old_delta;
        me.card_counters["__pgb_turn"] = turn + 1;
        me.card_counters["__pgb_delta"] = delta;
        me.recomputeMight();
        ctx.events.logTrace("PILTOVAN GUIDEBOT: base Might becomes " + std::to_string(cost) +
                            " this turn -> " + std::to_string(me.current_might) + "M");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1037;
        d.def_id = R"RB(rad-057-167)RB";
        d.name = R"RB(Piltovan Guidebot)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-057/167)RB";
        d.collector_number = 57;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Piltover)RB", R"RB(Mech)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 3;
        d.ability_text = R"RB(When you play a card, you may have my base Might become equal to that card's Energy cost this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9558ced7593f5a26911eb6e1393aae3289842e20-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1037(CardRegistry& r) {
    r.registerCard(1037, std::make_unique<PiltovanGuidebot>());
}

}  // namespace riftbound
