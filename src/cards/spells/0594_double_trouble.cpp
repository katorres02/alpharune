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

// "[Repeat] [2] ... Look at the top 3 cards of your Main Deck. You may reveal a
//  unit from among them and draw it. Recycle the rest." The [Repeat] cost loop
//  is engine-handled (resolves this onResolve once per repeat).

class DoubleTrouble : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // Look at the top 3 (top = back of vector). Their ids are stashed in
        // resume_data[6..8] on the first pass so re-entries after the choices
        // see the same cards.
        auto& ri = ctx.state.chain.resuming;
        std::vector<GameObjectId> top3;
        if (ri.has_value() && ri->resume_data.size() >= 10 && ri->resume_data[9] == 1) {
            for (int i = 6; i < 9; ++i)
                if (ri->resume_data[i] != static_cast<int32_t>(kInvalidId))
                    top3.push_back(static_cast<GameObjectId>(ri->resume_data[i]));
        } else {
            for (int i = (int)ps.main_deck.size() - 1;
                 i >= 0 && (int)top3.size() < 3; --i)
                top3.push_back(ps.main_deck[i]);
            if (ri.has_value()) {
                while (ri->resume_data.size() < 10) ri->resume_data.push_back(0);
                for (int i = 0; i < 3; ++i)
                    ri->resume_data[6 + i] = i < (int)top3.size()
                        ? static_cast<int32_t>(top3[i]) : static_cast<int32_t>(kInvalidId);
                ri->resume_data[9] = 1;
            }
        }
        if (top3.empty()) return;

        // "You may reveal a unit from among them and draw it."
        auto units = [&]() {
            std::vector<GameObjectId> out;
            for (auto cid : top3)
                if (ctx.state.objectExists(cid) && ctx.state.getObject(cid).isUnit() &&
                    std::find(ps.main_deck.begin(), ps.main_deck.end(), cid) != ps.main_deck.end())
                    out.push_back(cid);
            return out;
        };
        GameObjectId drawn_unit = kInvalidId;
        int conf = confirmOptional(ctx, "Double Trouble: reveal a unit and draw it?",
                                   [&]() { return !units().empty(); });
        if (conf == -1) return;   // waiting for the agent
        if (conf == 1) {
            drawn_unit = pickTarget(ctx, "Double Trouble: which unit?", units());
            if (ctx.executor.hasPendingChoice()) return;   // waiting for the agent
        }
        if (drawn_unit != kInvalidId && ctx.state.objectExists(drawn_unit)) {
            auto& deck = ps.main_deck;
            deck.erase(std::remove(deck.begin(), deck.end(), drawn_unit), deck.end());
            auto& obj = ctx.state.getObject(drawn_unit);
            ctx.events.emit(CardRevealedEvent{drawn_unit, obj.card_def_id, obj.owner,
                                              /*revealed_to_all=*/true, PlayerId::None,
                                              ZoneType::MainDeck});
            obj.zone = ZoneType::Hand;
            obj.location = std::nullopt;
            ps.hand.push_back(drawn_unit);
            ps.draws_this_turn += 1;
            ctx.events.emit(CardsDrawnEvent{ctx.controller, 1});
            ctx.events.logTrace("DOUBLE TROUBLE: revealed and drew " + obj.name);
        }

        // Recycle the rest (the remaining looked-at cards) to bottom of deck.
        std::vector<GameObjectId> rest;
        for (auto cid : top3) {
            if (cid == drawn_unit) continue;
            auto& deck = ps.main_deck;
            auto it = std::find(deck.begin(), deck.end(), cid);
            if (it != deck.end()) {
                deck.erase(it);
                rest.push_back(cid);
            }
        }
        if (!rest.empty()) {
            // GAP: the recycle order is not chosen by the player.
            ctx.executor.recycleCards(ctx.controller, rest);
            ctx.events.logTrace("DOUBLE TROUBLE: recycled " +
                                std::to_string(rest.size()) + " card(s)");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 594;
        d.def_id = R"RB(unl-032-219)RB";
        d.name = R"RB(Double Trouble)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-032/219)RB";
        d.collector_number = 32;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] [2] (You may pay the additional cost to repeat this spell's effect.)
Look at the top 3 cards of your Main Deck. You may reveal a unit from among them and draw it. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/64e977213f50471ad7b6e8664488fb9017693f71-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_594(CardRegistry& r) {
    r.registerCard(594, std::make_unique<DoubleTrouble>());
}

} // namespace riftbound
