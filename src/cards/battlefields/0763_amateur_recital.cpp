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

class AmateurRecital : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you hold here, you may move a unit at a battlefield to its base."
    // The "you may" and the unit are decided as the ability goes on the chain
    // (CR 383.3); triggered abilities get no engine-filled targets.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto candidates = [&]() {
            auto out = chooseableUnits(ctx.state, ctx.controller,
                                       [](const GameObject& o) { return o.isAtBattlefield(); });
            std::sort(out.begin(), out.end());
            return out;
        };
        int go = confirmOptional(ctx, "Amateur Recital: move a unit at a battlefield to its base",
                                 [&]() { return !candidates().empty(); });
        if (go <= 0) return;   // waiting (-1) or declined / nothing to move (0)
        GameObjectId t = pickTarget(ctx, "Amateur Recital: unit to move to its base", candidates());
        if (t == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting on the agent
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        if (!ctx.state.getObject(t).isAtBattlefield()) return;
        ctx.executor.moveToBase(t);
        ctx.events.logTrace("AMATEUR RECITAL: moved " + ctx.state.getObject(t).name + " to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 763;
        d.def_id = R"RB(unl-207-219)RB";
        d.name = R"RB(Amateur Recital)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-207/219)RB";
        d.collector_number = 207;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, you may move a unit at a battlefield to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9372a1574aa077e8b02cc6f00a1add060ae3b520-1039x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_763(CardRegistry& r) {
    r.registerCard(763, std::make_unique<AmateurRecital>());
}

} // namespace riftbound
