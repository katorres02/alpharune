#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class BattleMistress : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you recycle a rune, you may exhaust me to play a Gold gear token
    //  exhausted. When one or more enemy units die, ready me."
    TriggerType triggerType() const override { return TriggerType::WhenAnEnemyUnitDies; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouRecycleARune, TriggerType::WhenAnEnemyUnitDies};
    }
    // The "you may exhaust me" payment is decided at finalization (CR 383.3).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.firing_trigger == TriggerType::WhenYouRecycleARune) {
            auto can_exhaust = [&]() {
                return ctx.state.objectExists(ctx.source) &&
                       !ctx.state.getObject(ctx.source).is_exhausted;
            };
            int yes = confirmOptional(ctx, "Battle Mistress: exhaust to play a Gold token?",
                                      can_exhaust);
            if (yes != 1) return;
            if (costDueNow(ctx)) {
                if (!can_exhaust()) return;
                ctx.executor.exhaustObject(ctx.source);
            }
            if (finalizingChoices(ctx)) return;   // choices and costs end here
            createGoldExhausted(ctx);
            ctx.events.logTrace("BATTLE MISTRESS: recycled a rune -> Gold token");
            return;
        }
        if (finalizingChoices(ctx)) return;   // no choices on this branch
        ctx.executor.readyObject(ctx.source);
        ctx.events.logTrace("BATTLE MISTRESS: enemy unit died -> ready me");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 517;
        d.def_id = R"RB(sfd-203-221)RB";
        d.name = R"RB(Battle Mistress)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-203/221)RB";
        d.collector_number = 203;
        d.artist = R"RB(Zhongqi Li)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Body, Domain::Chaos};
        d.tags = {R"RB(Sivir)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you recycle a rune, you may exhaust me to play a Gold gear token exhausted.
When one or more enemy units die, ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fd060882c32a8deac04aea4241c6ab7b97236a05-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_517(CardRegistry& r) {
    r.registerCard(517, std::make_unique<BattleMistress>());
}

} // namespace riftbound
