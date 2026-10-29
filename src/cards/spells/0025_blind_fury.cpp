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

class BlindFury : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Each opponent reveals the top card of their Main Deck. Choose one and
    //  banish it, then play it, ignoring its cost. Then recycle the rest."
    // 1v1: the single opponent reveals one card; it is banished and played
    // under my control ignoring its cost. A spell goes on the chain
    // (playIgnoringCost queues it); a unit lands where I choose (base or a
    // battlefield I control, CR 355.2.a). "The rest" is empty in 1v1.
    // Resumable: ri.targets stashes the banished card ({kInvalidId} = nothing
    // revealed) so re-entry after the location choice doesn't reveal again.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        if (ri.targets.empty()) {
            PlayerId opp = opponent(ctx.controller);
            auto& opp_deck = ctx.state.player(opp).main_deck;
            if (opp_deck.empty() || !ctx.state.objectExists(opp_deck.back())) {
                ctx.events.logTrace("BLIND FURY: opponent deck empty — no-op");
                ri.targets.push_back(kInvalidId);
                return;
            }
            GameObjectId top = opp_deck.back();
            auto& card = ctx.state.getObject(top);
            ctx.events.emit(CardRevealedEvent{
                top, card.card_def_id, card.owner,
                /*revealed_to_all=*/true, ctx.controller, ZoneType::MainDeck});
            ctx.events.logTrace("BLIND FURY: opponent reveals " + card.name);
            opp_deck.pop_back();
            ctx.executor.banishObject(top);
            ri.targets.push_back(top);
            if (!ctx.state.objectExists(top)) return;
            // It leaves banishment as it is played.
            auto& bz = ctx.state.player(ctx.state.getObject(top).owner).banishment;
            bz.erase(std::remove(bz.begin(), bz.end(), top), bz.end());
        }
        const GameObjectId top = ri.targets.front();
        if (top == kInvalidId || !ctx.state.objectExists(top)) return;
        const auto& card = ctx.state.getObject(top);

        // A unit or gear goes where I choose (CR 355.2); a spell has no location.
        LocationId where;
        const int r = pickPlayLocation(ctx, top, where, PlayerId::None,
                                       "Blind Fury: where to play " + card.name);
        if (r == -1) return;  // waiting for the agent
        if (r == 0) {         // nowhere it may be played: it stays banished
            ctx.state.player(card.owner).banishment.push_back(top);
            return;
        }
        std::optional<LocationId> loc;
        if (!card.isSpell()) loc = where;
        const std::string name = card.name;
        ctx.executor.playIgnoringCost(ctx.controller, top, loc);
        ctx.events.logTrace("BLIND FURY: banished + played " + name + " ignoring cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 25;
        d.def_id = R"RB(ogn-025-298)RB";
        d.name = R"RB(Blind Fury)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-025/298)RB";
        d.collector_number = 25;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Each opponent reveals the top card of their Main Deck. Choose one and banish it, then play it, ignoring its cost. Then recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d74ba4998df9348408f1400090a0b0737c675fbd-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_25(CardRegistry& r) {
    r.registerCard(25, std::make_unique<BlindFury>());
}

} // namespace riftbound
