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

class RiftHerald : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenIMoveToFB, TriggerType::WhenIDie};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenIMoveToFB) {
            onMove(ctx);
        } else if (ctx.firing_trigger == TriggerType::WhenIDie) {
            onDeath(ctx);
        }
    }

private:
    // "When I move to a battlefield, look at the top 3 cards of your Main Deck.
    // You may reveal a unit from among them and draw it. Recycle the rest."
    // The player picks which unit (or none); the looked-at cards stay on top of
    // the deck until the answer comes back.
    void onMove(CardContext& ctx) {
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> looked;   // top first
        for (int i = 0; i < 3 && i < static_cast<int>(ps.main_deck.size()); ++i)
            looked.push_back(ps.main_deck[ps.main_deck.size() - 1 - i]);
        if (looked.empty()) return;
        // int-coded answers stay >= 0 (the action vocab folds negatives into 0):
        // 0 = reveal nothing, 1 + i = the i-th card looked at.
        std::vector<std::pair<int, std::string>> opts = {{0, "Reveal nothing"}};
        for (int i = 0; i < static_cast<int>(looked.size()); ++i) {
            const auto& o = ctx.state.getObject(looked[i]);
            if (o.isUnit()) opts.push_back({1 + i, "Reveal and draw " + o.name});
        }
        int pick = 0;
        if (opts.size() > 1 &&
            !stepChooseValue(ctx, ctx.controller, opts,
                             "Rift Herald: reveal a unit and draw it?", 20, 10, 0, pick))
            return;   // waiting on the agent
        GameObjectId drawn = kInvalidId;
        if (pick >= 1 && pick <= static_cast<int>(looked.size())) {
            drawn = looked[pick - 1];
            auto& obj = ctx.state.getObject(drawn);
            ctx.events.emit(CardRevealedEvent{
                drawn, obj.card_def_id, obj.owner,
                /*revealed_to_all=*/true, /*revealed_to=*/ctx.controller,
                ZoneType::MainDeck});
            ps.main_deck.erase(std::remove(ps.main_deck.begin(), ps.main_deck.end(), drawn),
                               ps.main_deck.end());
            obj.zone = ZoneType::Hand;
            ps.hand.push_back(drawn);
            ctx.events.logTrace("RIFT HERALD: drew unit " + obj.name);
        }
        // Recycle the rest.
        std::vector<GameObjectId> rest;
        for (auto cid : looked) {
            if (cid == drawn) continue;
            ps.main_deck.erase(std::remove(ps.main_deck.begin(), ps.main_deck.end(), cid),
                               ps.main_deck.end());
            rest.push_back(cid);
        }
        if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
    }

    // [Deathknell]: "Play a unit from your hand to your base, ignoring its
    // Energy cost. (You must still pay its Power cost.)" Only units whose Power
    // cost can be paid are offered; the Power is paid through the engine's
    // cost path (EffectExecutor::playPayingCost).
    void onDeath(CardContext& ctx) {
        auto& ps = ctx.state.player(ctx.controller);
        EffectPlayCost cost;
        cost.ignore_energy = true;
        std::vector<GameObjectId> hand_units;
        for (auto cid : ps.hand) {
            if (!ctx.state.objectExists(cid)) continue;
            if (!ctx.state.getObject(cid).isUnit()) continue;
            if (ctx.executor.canPlayPayingCost(ctx.controller, cid, cost)) hand_units.push_back(cid);
        }
        GameObjectId picked = pickTarget(ctx, "Rift Herald (deathknell: unit from hand)",
                                          hand_units);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto it = std::find(ps.hand.begin(), ps.hand.end(), picked);
        if (it == ps.hand.end()) return;
        ps.hand.erase(it);
        if (!ctx.executor.playPayingCost(ctx.controller, picked,
                                         LocationId{BaseLocation{ctx.controller}}, cost)) {
            ps.hand.push_back(picked);   // not paid: it stays in hand
            return;
        }
        ctx.events.logTrace("RIFT HERALD: deathknell -> played a unit from hand to base "
                            "(Energy ignored, Power paid)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 741;
        d.def_id = R"RB(unl-179-219)RB";
        d.name = R"RB(Rift Herald)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-179/219)RB";
        d.collector_number = 179;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(The Void)RB"};
        d.energy_cost = 8;
        d.power_cost = 1;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB(When I move to a battlefield, look at the top 3 cards of your Main Deck. You may reveal a unit from among them and draw it. Recycle the rest.
[Deathknell][>] Play a unit from your hand to your base, ignoring its Energy cost. (When I die, get the effect. You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4da94f3af92222bebe7be49197ef8d840772d73d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_741(CardRegistry& r) {
    r.registerCard(741, std::make_unique<RiftHerald>());
}

} // namespace riftbound
