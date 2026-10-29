#include "cards/card.h"
#include "cards/card_helpers.h"
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

// "When you play me, look at the top 4 cards of your Main Deck. You may reveal
// from among them a unit or gear with Energy cost no more than [3] and with
// Power cost no more than [A] and draw it. Recycle the rest." The looked-at
// cards stay on top until the player answers (resume_point 20, slot 10).
class TrifarianCaptain : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> looked;   // top first
        for (int i = 0; i < 4 && i < static_cast<int>(ps.main_deck.size()); ++i)
            looked.push_back(ps.main_deck[ps.main_deck.size() - 1 - i]);
        if (looked.empty()) return;
        const CardDB& db = ctx.executor.cardDB();
        std::vector<std::pair<int, std::string>> opts = {{0, "Reveal nothing"}};
        for (int i = 0; i < static_cast<int>(looked.size()); ++i) {
            const auto& o = ctx.state.getObject(looked[i]);
            if (!o.isUnit() && !o.isGear()) continue;
            if (o.card_def_id == kInvalidId || !db.all().count(o.card_def_id)) continue;
            const auto& def = db.get(o.card_def_id);
            if (def.energy_cost > 3 || def.power_cost > 1) continue;
            opts.push_back({1 + i, "Reveal and draw " + o.name});
        }
        int pick = 0;
        if (opts.size() > 1 &&
            !stepChooseValue(ctx, ctx.controller, opts,
                             "Trifarian Captain: reveal a unit or gear and draw it?", 20, 10, 0, pick))
            return;   // waiting on the agent
        GameObjectId drawn = kInvalidId;
        if (pick >= 1 && pick <= static_cast<int>(looked.size())) {
            drawn = looked[pick - 1];
            auto& obj = ctx.state.getObject(drawn);
            ctx.events.emit(CardRevealedEvent{drawn, obj.card_def_id, obj.owner,
                                              /*revealed_to_all=*/true, PlayerId::None,
                                              ZoneType::MainDeck});
            ps.main_deck.erase(std::remove(ps.main_deck.begin(), ps.main_deck.end(), drawn),
                               ps.main_deck.end());
            obj.zone = ZoneType::Hand;
            ps.hand.push_back(drawn);
            ps.draws_this_turn += 1;   // "draw it": a draw (CardsDrawnEvent)
            ps.last_draw_count = 1;
            ctx.events.emit(CardsDrawnEvent{ctx.controller, 1});
            ctx.events.logTrace("TRIFARIAN CAPTAIN: revealed and drew " + obj.name);
        }
        std::vector<GameObjectId> rest;
        for (auto cid : looked) {
            if (cid == drawn) continue;
            ps.main_deck.erase(std::remove(ps.main_deck.begin(), ps.main_deck.end(), cid),
                               ps.main_deck.end());
            rest.push_back(cid);
        }
        if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1105;
        d.def_id = R"RB(rad-129-167)RB";
        d.name = R"RB(Trifarian Captain)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-129/167)RB";
        d.collector_number = 129;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Noxus)RB", R"RB(Trifarian)RB"};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 1;
        d.ability_text = R"RB(When you play me, look at the top 4 cards of your Main Deck. You may reveal from among them a unit or gear with Energy cost no more than [3] and with Power cost no more than [A] and draw it. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/914ced10400826a878ed5cf7294207a4e7b542b1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1105(CardRegistry& r) {
    r.registerCard(1105, std::make_unique<TrifarianCaptain>());
}

}  // namespace riftbound
