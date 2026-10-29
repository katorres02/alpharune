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

class IvernNurturer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold};
    }

    static bool isThemed(const GameObject& o) {
        for (const auto& t : o.tags)
            if (t == "Bird" || t == "Cat" || t == "Dog" || t == "Poro") return true;
        return false;
    }

    // The whole effect happens as the ability resolves: the "you may reveal"
    // and which unit to reveal are decided after looking at the cards, so
    // nothing is chosen at finalization.
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);

        // Look at the top 3 (top = back of vector). They stay in place until
        // every choice is made, so a suspend/resume sees the same cards.
        std::vector<GameObjectId> peeked;
        for (int i = 0; i < 3; ++i) {
            int idx = static_cast<int>(ps.main_deck.size()) - 1 - i;
            if (idx < 0) break;
            peeked.push_back(ps.main_deck[idx]);
        }
        if (peeked.empty()) return;
        std::vector<GameObjectId> units;
        for (auto id : peeked)
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).isUnit()) units.push_back(id);

        // "You may reveal a unit from among them and draw it."
        int conf = confirmOptional(ctx,
            "Ivern, Nurturer: reveal a unit from the top 3 and draw it?",
            [&]() { return !units.empty(); });
        if (conf == -1) return;  // waiting for agent

        GameObjectId drafted = kInvalidId;
        GameObjectId buff_target = kInvalidId;
        if (conf == 1) {
            // Which unit to reveal, then (only if it is a Bird, Cat, Dog or
            // Poro) which friendly unit to [Buff].
            auto [a, b] = pickTargetPair(ctx, "Ivern, Nurturer: reveal and draw a unit",
                units,
                [&](GameObjectId a) -> std::vector<GameObjectId> {
                    std::vector<GameObjectId> out;
                    if (!ctx.state.objectExists(a) || !isThemed(ctx.state.getObject(a))) return out;
                    for (auto& [oid, u] : ctx.state.objects) {
                        if (!u.isUnit() || u.controller != ctx.controller) continue;
                        if (!u.location.has_value()) continue;
                        out.push_back(oid);
                    }
                    return out;
                });
            auto& ri = ctx.state.chain.resuming;
            if (ri.has_value() && ri->resume_point < 13) return;   // waiting for agent
            drafted = a;
            buff_target = b;
        }

        // Take the 3 cards off the top.
        std::vector<GameObjectId> rest;
        for (int i = 0; i < 3 && !ps.main_deck.empty(); ++i) {
            GameObjectId id = ps.main_deck.back();
            ps.main_deck.pop_back();
            if (id != drafted) rest.push_back(id);
        }

        bool themed = false;
        if (drafted != kInvalidId && ctx.state.objectExists(drafted)) {
            auto& obj = ctx.state.getObject(drafted);
            ctx.events.emit(CardRevealedEvent{
                /*card=*/drafted,
                /*card_def_id=*/obj.card_def_id,
                /*owner=*/obj.owner,
                /*revealed_to_all=*/true,
                /*revealed_to=*/ctx.controller,
                /*source_zone=*/ZoneType::MainDeck,
            });
            // Draw it: put it back on top and draw, so it is a real draw.
            ps.main_deck.push_back(drafted);
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("IVERN NURTURER: revealed and drew " + obj.name);
            themed = isThemed(obj);
        }

        // Recycle the rest.
        if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);

        // "Then if you revealed a Bird, Cat, Dog, or Poro, [Buff] a friendly unit."
        if (themed && buff_target != kInvalidId && ctx.state.objectExists(buff_target)) {
            ctx.executor.buffUnit(buff_target);
            ctx.events.logTrace("IVERN NURTURER: themed reveal — buff " +
                                 ctx.state.getObject(buff_target).name);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 613;
        d.def_id = R"RB(unl-051-219)RB";
        d.name = R"RB(Ivern, Nurturer)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-051/219)RB";
        d.collector_number = 51;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ivern)RB", R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me or when I hold, look at the top 3 cards of your Main Deck. You may reveal a unit from among them and draw it. Recycle the rest. Then if you revealed a Bird, Cat, Dog, or Poro, do this: [Buff] a friendly unit. (Give it a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a72b08dd57422d3441df88eb61c000cdb4ac688a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_613(CardRegistry& r) {
    r.registerCard(613, std::make_unique<IvernNurturer>());
}

} // namespace riftbound
