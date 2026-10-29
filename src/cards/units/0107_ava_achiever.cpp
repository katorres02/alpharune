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

class AvaAchiever : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    // "When I attack, you may pay [C] to play a card with [Hidden] from your
    // hand, ignoring its cost. If it's a unit, play it here." [C] = one Power of
    // my domain (Mind): floating Mind (or universal) power, else a Mind rune
    // recycled through payOnePower.
    bool canPayC(CardContext& ctx) const {
        return payableOnePower(ctx.state, ctx.controller, Domain::Mind);
    }
    bool payC(CardContext& ctx) const { return payOnePower(ctx, ctx.controller, Domain::Mind); }
    std::vector<GameObjectId> hiddenInHand(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        const auto& ps = ctx.state.player(ctx.controller);
        for (auto cid : ps.hand) {
            if (!ctx.state.objectExists(cid)) continue;
            if (ctx.state.getObject(cid).hasKeyword(Keyword::Hidden)) out.push_back(cid);
        }
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // CR 445.2: only offer a payment that can be made.
        auto still_legal = [&]() { return !hiddenInHand(ctx).empty() && canPayC(ctx); };
        int conf = confirmOptional(ctx, "Ava Achiever: pay [C] to play a Hidden card?",
                                   still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / no Hidden cards
        GameObjectId picked = pickTarget(ctx, "Ava Achiever (Hidden card)", hiddenInHand(ctx));
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended
        }
        // "[C] to [effect]" is paid as the trigger is finalized (CR 383.3),
        // once the card is chosen (a suspended pickTarget returned above, so
        // this line runs once per pass).
        if (costDueNow(ctx) && !payC(ctx)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto& ps = ctx.state.player(ctx.controller);
        if (std::find(ps.hand.begin(), ps.hand.end(), picked) == ps.hand.end())
            return;   // left the hand in response
        // "If it's a unit, play it here." (my battlefield); a gear goes where I
        // choose (CR 355.2: base, or a battlefield for [Deploy] gear).
        std::optional<LocationId> loc;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (ctx.state.getObject(picked).isUnit() && my_bf) {
            loc = LocationId{BattlefieldLocation{*my_bf}};
        } else {
            LocationId where;
            const int r = pickPlayLocation(ctx, picked, where, PlayerId::None,
                                           "Ava Achiever: play it where?");
            if (r == -1) return;   // waiting for the agent
            if (r == 0) return;    // nowhere to play it
            loc = where;
        }
        ps.hand.erase(std::find(ps.hand.begin(), ps.hand.end(), picked));
        ctx.executor.playIgnoringCost(ctx.controller, picked, loc);
        ctx.events.logTrace("AVA ACHIEVER: paid [C] -> played a Hidden card from hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 107;
        d.def_id = R"RB(ogn-107-298)RB";
        d.name = R"RB(Ava Achiever)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-107/298)RB";
        d.collector_number = 107;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Yordle)RB"};
        d.energy_cost = 5;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I attack, you may pay [C] to play a card with [Hidden] from your hand, ignoring its cost. If it's a unit, play it here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/93e91aa99eb09baa68dd95b0013a89d9ffde5240-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_107(CardRegistry& r) {
    r.registerCard(107, std::make_unique<AvaAchiever>());
}

} // namespace riftbound
