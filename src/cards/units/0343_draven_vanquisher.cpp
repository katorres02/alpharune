#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class DravenVanquisher : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // "When I win a combat, play a Gold gear token exhausted.
    //  When I attack or defend, you may pay [C] to give me +2 [M] this turn."
    //  (Vendetta errata text.)
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenIWinCombat, TriggerType::WhenIAttackOrDefend};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenIWinCombat) {
            if (finalizingChoices(ctx)) return;   // no choices
            createGoldExhausted(ctx);
            ctx.events.logTrace("DRAVEN: won combat -> Gold gear token (exhausted)");
            return;
        }
        if (ctx.firing_trigger == TriggerType::WhenIAttackOrDefend) {
            if (!ctx.state.objectExists(ctx.source)) return;
            // "you may pay [C]" ([C] = one Fury power): floating power, or a
            // Fury rune at base (ready or exhausted) recycled for it.
            auto has_fury_power = [&]() -> bool {
                return payableOnePower(ctx.state, ctx.controller, Domain::Fury);
            };
            int conf = confirmOptional(ctx, "Draven: pay [C] for +2 [M]?",
                                       has_fury_power);
            if (conf == -1) return;  // waiting for agent
            if (conf == 0) return;   // declined / can't pay
            if (costDueNow(ctx) && !payOnePower(ctx, ctx.controller, Domain::Fury)) return;
            if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
            ctx.executor.giveTemporaryMight(ctx.source, 2);
            ctx.events.logTrace("DRAVEN: paid [C] -> +2 [M] this turn");
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 343;
        d.def_id = R"RB(sfd-020-221)RB";
        d.name = R"RB(Draven, Vanquisher)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-020/221)RB";
        d.collector_number = 20;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Draven)RB", R"RB(Noxus)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I win a combat, play a Gold gear token exhausted.
When I attack or defend, you may pay [C] to give me +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8e49e0b83df68f37a15bf704b83475144fd9ef92-744x1039.png)RB";
        d.banned = true;  // tournament ban (formerly cards/ban-list.csv)
        return d;
    }();
};

}  // anonymous namespace

void register_card_343(CardRegistry& r) {
    r.registerCard(343, std::make_unique<DravenVanquisher>());
}

} // namespace riftbound
