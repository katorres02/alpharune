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

class ScrutinizingScope : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "[1], [E]: Choose an opponent. They guess a card type. Reveal the top
    // card of your Main Deck. If it has that type, recycle it. Otherwise,
    // draw it." The opponent (only one in 1v1) answers as the ability
    // resolves (resume_point 20 = consume the guess).
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .energy = 1};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId opp = opponent(ctx.controller);
        int guess = 0;   // direct invocation (no chain): the opponent guesses unit
        if (ctx.state.chain.resuming.has_value()) {
            auto& ri = *ctx.state.chain.resuming;
            if (ri.resume_point < 20) {
                ctx.executor.requestChoice(opp,
                    {valChoice(opp, 0, "Guess: unit"), valChoice(opp, 1, "Guess: gear"),
                     valChoice(opp, 2, "Guess: spell")},
                    "Scrutinizing Scope: guess the card type of the top card");
                ri.resume_point = 20;
                return;
            }
            if (ri.resume_point != 20) return;
            ri.resume_point = 21;
            guess = takenValue(ctx, 0);
        }
        ctx.executor.lookBeforeReveal(ctx.controller, ctx.controller);   // Void Hatchling
        auto& deck = ctx.state.player(ctx.controller).main_deck;
        if (deck.empty()) return;
        const GameObjectId top = deck.back();
        const auto& t = ctx.state.getObject(top);
        ctx.events.emit(CardRevealedEvent{top, t.card_def_id, t.owner, /*revealed_to_all=*/true,
                                          PlayerId::None, ZoneType::MainDeck});
        const bool hit = (guess == 0 && t.isUnit()) || (guess == 1 && t.isGear()) ||
                         (guess == 2 && t.isSpell());
        ctx.events.logTrace(std::string("SCRUTINIZING SCOPE: revealed ") + t.name +
                            (hit ? " (guessed) -> recycle" : " (missed) -> draw"));
        if (hit) {
            deck.pop_back();
            ctx.executor.recycleCards(ctx.controller, {top});
        } else {
            ctx.executor.drawCards(ctx.controller, 1);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1044;
        d.def_id = R"RB(rad-064-167)RB";
        d.name = R"RB(Scrutinizing Scope)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-064/167)RB";
        d.collector_number = 64;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.rarity = Rarity::Rare;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.ability_text = R"RB([1], [E]: Choose an opponent. They guess a card type. Reveal the top card of your Main Deck. If it has that type, recycle it. Otherwise, draw it. (Unit, gear, and spell are types.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6a028fc372c14dda73864aba3d3e84063f6da98d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1044(CardRegistry& r) {
    r.registerCard(1044, std::make_unique<ScrutinizingScope>());
}

}  // namespace riftbound
