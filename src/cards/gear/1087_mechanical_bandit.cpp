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

// "At the start of your Beginning Phase, each player reveals the top card of
// their Main Deck. If you revealed a card with the lowest Energy cost, draw
// it. Recycle the rest." Ties for lowest count as lowest.
class MechanicalBandit : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        struct Rev { PlayerId p; GameObjectId card; int cost; };
        std::vector<Rev> revealed;
        for (auto p : {ctx.controller, opponent(ctx.controller)}) {
            auto& deck = ctx.state.player(p).main_deck;
            if (deck.empty()) continue;
            GameObjectId top = deck.back();
            auto& o = ctx.state.getObject(top);
            int cost = o.card_def_id != kInvalidId ? ctx.executor.cardDB().get(o.card_def_id).energy_cost : 0;
            ctx.events.emit(CardRevealedEvent{top, o.card_def_id, o.owner,
                                              /*revealed_to_all=*/true, PlayerId::None,
                                              ZoneType::MainDeck});
            ctx.events.logTrace("MECHANICAL BANDIT: " + std::string(toString(p)) + " reveals " +
                                o.name + " (cost " + std::to_string(cost) + ")");
            revealed.push_back({p, top, cost});
        }
        if (revealed.empty()) return;
        int lowest = revealed.front().cost;
        for (const auto& r : revealed) lowest = std::min(lowest, r.cost);
        for (const auto& r : revealed) {
            if (r.p == ctx.controller && r.cost == lowest) {
                auto& deck = ctx.state.player(r.p).main_deck;
                if (!deck.empty() && deck.back() == r.card) {
                    ctx.executor.drawCards(ctx.controller, 1);
                    ctx.events.logTrace("MECHANICAL BANDIT: lowest cost -> draw it");
                }
                continue;
            }
            auto& deck = ctx.state.player(r.p).main_deck;
            auto it = std::find(deck.begin(), deck.end(), r.card);
            if (it == deck.end()) continue;
            deck.erase(it);
            ctx.executor.recycleCards(ctx.controller, {r.card});
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1087;
        d.def_id = R"RB(rad-111-167)RB";
        d.name = R"RB(Mechanical Bandit)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-111/167)RB";
        d.collector_number = 111;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        // Gallery said Fury + "Device" tag; the printed card is Chaos, no tag.
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.rarity = Rarity::Rare;
        d.energy_cost = 4;
        d.power_cost = 1;
        d.ability_text = R"RB(At the start of your Beginning Phase, each player reveals the top card of their Main Deck. If you revealed a card with the lowest Energy cost, draw it. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cb2a419b61b6f1e6d138c3c3b07b393a4037237b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1087(CardRegistry& r) {
    r.registerCard(1087, std::make_unique<MechanicalBandit>());
}

}  // namespace riftbound
