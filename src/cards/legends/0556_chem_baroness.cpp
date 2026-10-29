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

class ChemBaroness : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When you or an ally hold" — a hold only (WhenIHold legend sweep in
    // TriggerManager::onScore), not a conquer.
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Optional: exhaust me to play a Gold gear token exhausted.
        if (!ctx.state.objectExists(ctx.source)) return;
        auto can_exhaust = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   !ctx.state.getObject(ctx.source).is_exhausted;
        };
        int yes = confirmOptional(ctx, "Chem-Baroness: exhaust to play a Gold token?",
                                  can_exhaust);
        if (yes != 1) return;
        if (costDueNow(ctx)) {
            if (!can_exhaust()) return;
            ctx.executor.exhaustObject(ctx.source);
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        createGoldExhausted(ctx);   // bound to the Gold card (its Add ability)
        ctx.events.logTrace("CHEM-BARONESS: played a Gold token exhausted");
    }
    // "While your score is within 3 points of the Victory Score, your Gold
    // [Add] an additional [1]." Read by Gold's ability (0326_gold.cpp).
    // Victory Score here is the mode's; battlefield modifiers are not counted.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        auto& ps = state.player(controller);
        if (ps.score + 3 >= state.mode.victory_score) ps.gold_extra_energy = 1;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 556;
        d.def_id = R"RB(sfd-249-221)RB";
        d.name = R"RB(Chem-Baroness)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-249/221)RB";
        d.collector_number = 249;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(Renata Glasc)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When you or an ally hold, you may exhaust me to play a Gold gear token exhausted.
While your score is within 3 points of the Victory Score, your Gold [ADD] an additional [1].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5df25d5a1351d0a97e103ef8e155991297b86ca9-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_556(CardRegistry& r) {
    r.registerCard(556, std::make_unique<ChemBaroness>());
}

} // namespace riftbound
