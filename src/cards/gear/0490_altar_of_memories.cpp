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

// "When a friendly unit dies, you may exhaust me to draw 1, then put a card
//  from your hand on the top or bottom of your Main Deck."

class AltarOfMemories : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitDies; }
    // "you may exhaust me to draw 1" is a [cost] to [effect] choice made when
    // the trigger is finalized (CR 383.3); the exhaust is paid then.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto still_legal = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   ctx.state.getObject(ctx.source).location.has_value() &&
                   !ctx.state.getObject(ctx.source).is_exhausted;
        };
        int conf = confirmOptional(ctx, "Altar of Memories: exhaust me to draw 1?", still_legal);
        if (conf == -1) return;  // waiting on agent
        if (conf == 0) return;   // declined / not legal
        // The exhaust and the draw happen once: the later choices (pickMode =
        // resume_points 3..5, pickTarget = 6..8) re-enter this callback with a
        // higher resume_point.
        auto& ri = ctx.state.chain.resuming;
        const bool first_pass = !ri.has_value() || ri->resume_point < 3;
        if (first_pass && costDueNow(ctx)) {
            if (!still_legal()) return;
            ctx.executor.exhaustObject(ctx.source);
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here

        if (first_pass) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("ALTAR OF MEMORIES: exhausted -> draw 1");
        }
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.hand.empty()) return;
        int mode = pickMode(ctx, "Altar: top or bottom of deck?", 2, {"top", "bottom"});
        if (mode < 0) return;   // suspend / no legal mode
        std::vector<GameObjectId> hand(ps.hand.begin(), ps.hand.end());
        GameObjectId card = pickTarget(ctx, "Altar: put which hand card onto deck?", hand);
        if (card == kInvalidId) return;  // suspend or empty
        if (!ctx.state.objectExists(card)) return;
        auto it = std::find(ps.hand.begin(), ps.hand.end(), card);
        if (it == ps.hand.end()) return;
        ps.hand.erase(it);
        auto& c = ctx.state.getObject(card);
        c.zone = ZoneType::MainDeck;
        c.location = std::nullopt;
        if (mode == 0) ps.main_deck.push_back(card);                  // top (drawn end)
        else           ps.main_deck.insert(ps.main_deck.begin(), card); // bottom
        ctx.events.logTrace(std::string("ALTAR OF MEMORIES: put a hand card on ") +
                            (mode == 0 ? "top" : "bottom") + " of deck");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 490;
        d.def_id = R"RB(sfd-169-221)RB";
        d.name = R"RB(Altar of Memories)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-169/221)RB";
        d.collector_number = 169;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When a friendly unit dies, you may exhaust me to draw 1, then put a card from your hand on the top or bottom of your Main Deck.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3ddc94a035d793d48987c6fe5d65fa11ea25c808-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_490(CardRegistry& r) {
    r.registerCard(490, std::make_unique<AltarOfMemories>());
}

} // namespace riftbound
