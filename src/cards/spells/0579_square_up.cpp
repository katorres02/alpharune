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

class SquareUp : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Repeat] — Discard 1. Give a unit [Assault 4] this turn."
    // The engine's [Repeat] reads only Energy/Power costs, so the discard is
    // offered as an optional additional cost at play
    // (GameEngine::maybePayOptionalAdditionalCost); paying it repeats the
    // effect once, on a unit chosen as the repeat resolves.
    static constexpr const char* kRepeatPaid = "__square_up_repeat_paid";
    OptionalAdditionalCost optionalAdditionalCost() const override {
        OptionalAdditionalCost c;
        c.valid = true;
        c.discard_cards = 1;
        c.paid_flag = kRepeatPaid;
        return c;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ri = ctx.state.chain.resuming;
        // First effect, once (the repeat's pickTarget re-enters at 6..8).
        if (!ri.has_value() || ri->resume_point < 6) {
            if (!targets.empty() && ctx.state.objectExists(targets[0]) &&
                ctx.state.getObject(targets[0]).location.has_value())
                ctx.executor.giveTemporaryKeyword(targets[0], Keyword::Assault, 4);
        }
        bool paid = false;
        if (ctx.state.objectExists(ctx.source)) {
            auto& c = ctx.state.getObject(ctx.source).card_counters;
            auto it = c.find(kRepeatPaid);
            paid = it != c.end() && it->second == 1;
        }
        if (!paid) return;
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (obj.controller != ctx.controller && obj.untargetable_by_enemy) continue;
            legal.push_back(id);
        }
        GameObjectId again = pickTarget(ctx, "Square Up (repeat): give a unit [Assault 4]", legal);
        if (ctx.executor.hasPendingChoice()) return;   // waiting for the agent
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters.erase(kRepeatPaid);  // one play, one payment
        if (again == kInvalidId || !ctx.state.objectExists(again)) return;
        ctx.executor.giveTemporaryKeyword(again, Keyword::Assault, 4);
        ctx.events.logTrace("SQUARE UP: repeat -> [Assault 4]");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 579;
        d.def_id = R"RB(unl-017-219)RB";
        d.name = R"RB(Square Up)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-017/219)RB";
        d.collector_number = 17;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 4;
        d.rarity = Rarity::Uncommon;
        d.assault_value = 4;
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] — Discard 1 (You may pay the additional cost to repeat this spell's effect.)
Give a unit [Assault 4] this turn. (+4 [M] while it's an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5e14c09db2f064e5f6986f500a04335d73d459dd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_579(CardRegistry& r) {
    r.registerCard(579, std::make_unique<SquareUp>());
}

} // namespace riftbound
