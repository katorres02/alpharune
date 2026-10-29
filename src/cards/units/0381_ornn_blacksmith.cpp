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

class OrnnBlacksmith : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold};
    }
    // "Look at the top 4 cards of your Main Deck. You may reveal a gear from
    // among them and draw it. Then recycle the rest." The player picks a gear
    // or "None" (resume_point 20 = consume the pick).
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ps = ctx.state.player(ctx.controller);
        auto top4 = [&]() {
            std::vector<GameObjectId> out;
            for (int i = 0; i < 4 && i < static_cast<int>(ps.main_deck.size()); ++i)
                out.push_back(ps.main_deck[ps.main_deck.size() - 1 - i]);
            return out;
        };
        auto peeked = top4();
        if (peeked.empty()) return;
        std::vector<GameObjectId> gears;
        for (auto id : peeked)
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).isGear()) gears.push_back(id);

        GameObjectId drafted = kInvalidId;
        auto* ri = ctx.state.chain.resuming.has_value() ? &*ctx.state.chain.resuming : nullptr;
        if (ri && ri->resume_point == 20) {
            drafted = takenObject(ctx);
            if (std::find(gears.begin(), gears.end(), drafted) == gears.end()) drafted = kInvalidId;
            ri->resume_point = 21;
        } else if (ri && ri->resume_point < 20) {
            if (!gears.empty()) {
                std::vector<Intent> opts;
                for (auto id : gears) opts.push_back(objChoice(ctx.controller, id));
                opts.push_back(valChoice(ctx.controller, 0, "None"));
                ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                           "Ornn: reveal a gear and draw it");
                ri->resume_point = 20;
                return;
            }
            ri->resume_point = 21;
        } else if (!ri) {
            drafted = gears.empty() ? kInvalidId : gears.front();   // direct invocation
        } else {
            return;   // already done
        }

        if (drafted != kInvalidId) {
            auto& drafted_obj = ctx.state.getObject(drafted);
            ctx.events.emit(CardRevealedEvent{
                /*card=*/drafted,
                /*card_def_id=*/drafted_obj.card_def_id,
                /*owner=*/drafted_obj.owner,
                /*revealed_to_all=*/true,
                /*revealed_to=*/ctx.controller,
                /*source_zone=*/ZoneType::MainDeck,
            });
            ctx.events.logTrace("ORNN, BLACKSMITH: drafted gear " + drafted_obj.name);
            // "draw it": put it on top and draw, so it counts as a draw.
            auto it = std::find(ps.main_deck.begin(), ps.main_deck.end(), drafted);
            if (it != ps.main_deck.end()) ps.main_deck.erase(it);
            ps.main_deck.push_back(drafted);
            ctx.executor.drawCards(ctx.controller, 1);
        }
        std::vector<GameObjectId> rest;
        for (auto id : peeked) if (id != drafted) rest.push_back(id);
        for (auto id : rest) {
            auto it = std::find(ps.main_deck.begin(), ps.main_deck.end(), id);
            if (it != ps.main_deck.end()) ps.main_deck.erase(it);
        }
        if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 381;
        d.def_id = R"RB(sfd-058-221)RB";
        d.name = R"RB(Ornn, Blacksmith)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-058/221)RB";
        d.collector_number = 58;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ornn)RB", R"RB(Freljord)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play me or when I hold, look at the top 4 cards of your Main Deck. You may reveal a gear from among them and draw it. Then recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/77b48a28f48f26714f4bcf860379945a2d7186aa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_381(CardRegistry& r) {
    r.registerCard(381, std::make_unique<OrnnBlacksmith>());
}

} // namespace riftbound
