#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace riftbound {
namespace {

class ClocktowerGuardian : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you play me or when I hold, [Predict 3]."
    // "[Deathknell][>] Banish the top card of your Main Deck. You may play it,
    //  ignoring its Energy cost." The "may" depends on the banished card, so it
    // is asked as the ability resolves. resume_data[5] = banished id + 1.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold, TriggerType::WhenIDie};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger != TriggerType::WhenIDie) {
            ctx.executor.predict(ctx.controller, 3);
            return;
        }
        auto banishTop = [&]() -> GameObjectId {
            auto& deck = ctx.state.player(ctx.controller).main_deck;
            if (deck.empty()) return kInvalidId;
            GameObjectId top = deck.back();
            ctx.executor.banishObject(top);
            return top;
        };
        GameObjectId card = kInvalidId;
        auto& ri_opt = ctx.state.chain.resuming;
        if (ri_opt.has_value()) {
            auto& ri = *ri_opt;
            while (ri.resume_data.size() < 6) ri.resume_data.push_back(0);
            if (ri.resume_data[5] == 0) {
                card = banishTop();
                ri.resume_data[5] = card == kInvalidId ? -1 : static_cast<int32_t>(card) + 1;
            } else if (ri.resume_data[5] > 0) {
                card = static_cast<GameObjectId>(ri.resume_data[5] - 1);
            }
        } else {
            card = banishTop();
        }
        if (card == kInvalidId) return;
        const EffectPlayCost free_energy{.ignore_energy = true};
        auto playable = [&]() {
            return ctx.state.objectExists(card) &&
                   ctx.state.getObject(card).zone == ZoneType::Banishment &&
                   ctx.executor.canPlayPayingCost(ctx.controller, card, free_energy);
        };
        int go = confirmOptional(ctx, "Clocktower Guardian: play the banished card?", playable);
        if (go != 1) return;
        // A unit or gear goes where I choose (CR 355.2).
        LocationId where;
        if (pickPlayLocation(ctx, card, where, PlayerId::None,
                             "Clocktower Guardian: play it where?") <= 0)
            return;
        auto& ban = ctx.state.player(ctx.state.getObject(card).owner).banishment;
        ban.erase(std::remove(ban.begin(), ban.end(), card), ban.end());
        if (!ctx.executor.playPayingCost(ctx.controller, card, where, free_energy)) {
            ban.push_back(card);   // not played: it stays banished
            return;
        }
        ctx.events.logTrace("CLOCKTOWER GUARDIAN: played the banished card ignoring its Energy cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1047;
        d.def_id = R"RB(rad-067-167)RB";
        d.name = R"RB(Clocktower Guardian)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-067/167)RB";
        d.collector_number = 67;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Shurima)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 8;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB(When you play me or when I hold, [Predict 3].
[Deathknell][>] Banish the top card of your Main Deck. You may play it, ignoring its Energy cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b846d604eee450a4ab8f457f2738b0596e7b361e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1047(CardRegistry& r) {
    r.registerCard(1047, std::make_unique<ClocktowerGuardian>());
}

}  // namespace riftbound
