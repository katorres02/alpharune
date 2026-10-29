#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// resume_point layout (no Card::pick* helper is used):
//   0  → reveal, pick which revealed card (if more than one), → 1
//   1  → consume the pick; banish it; if it is a unit, ask where to play it → 2
//   2  → consume the location; play it ignoring its cost → 3 (done)
// resume_data[0] = the chosen card.
constexpr int kDrakeAwaitCard = 1;
constexpr int kDrakeAwaitLoc = 2;
constexpr int kDrakeDone = 3;

// "When you play me, reveal the top card of each opponent's Main Deck. Choose
// one and banish it, then play it, ignoring its cost. Recycle the rest."
// (1v1: one opponent, so one card is revealed, banished and played; there is
// no rest to recycle.) A unit is played to its new controller's base or a
// battlefield they control (CR 355.2.a).
class DrakehoundPackmaster : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ri_opt = ctx.state.chain.resuming;
        const PlayerId me = ctx.controller;
        int point = ri_opt.has_value() ? ri_opt->resume_point : 0;
        GameObjectId chosen = kInvalidId;

        if (point == 0) {
            std::vector<GameObjectId> revealed;
            for (PlayerId p : {PlayerId::Player1, PlayerId::Player2}) {
                if (p == me) continue;
                ctx.executor.lookBeforeReveal(me, p);   // Void Hatchling
                auto& deck = ctx.state.player(p).main_deck;
                if (deck.empty()) continue;
                const GameObjectId top = deck.back();
                const auto& o = ctx.state.getObject(top);
                ctx.events.emit(CardRevealedEvent{top, o.card_def_id, o.owner,
                                                  /*revealed_to_all=*/true, PlayerId::None,
                                                  ZoneType::MainDeck});
                revealed.push_back(top);
            }
            if (revealed.empty()) return;
            chosen = revealed.front();
            if (revealed.size() > 1 && ri_opt.has_value()) {
                std::vector<Intent> opts;
                for (auto r : revealed) opts.push_back(objChoice(me, r));
                ctx.executor.requestChoice(me, std::move(opts), "Drakehound Packmaster: choose a card");
                ri_opt->resume_point = kDrakeAwaitCard;
                return;
            }
            point = kDrakeAwaitCard;
        } else if (point == kDrakeAwaitCard) {
            chosen = takenObject(ctx);
        }

        if (point == kDrakeAwaitCard) {
            if (chosen == kInvalidId || !ctx.state.objectExists(chosen)) return;
            ctx.executor.banishObject(chosen);
            auto& obj = ctx.state.getObject(chosen);
            if (obj.zone != ZoneType::Banishment) return;
            auto& ban = ctx.state.player(obj.owner).banishment;
            ban.erase(std::remove(ban.begin(), ban.end(), chosen), ban.end());
            if (ri_opt.has_value()) ri_opt->resume_data = {static_cast<int32_t>(chosen)};
            ctx.events.logTrace("DRAKEHOUND PACKMASTER: banished " + obj.name + ", playing it");
            const auto dests = destinations(ctx, obj);
            if (dests.size() > 1 && ri_opt.has_value()) {
                std::vector<Intent> opts;
                for (int d : dests) opts.push_back(valChoice(me, d, destLabel(d)));
                ctx.executor.requestChoice(me, std::move(opts), "Drakehound Packmaster: play it where");
                ri_opt->resume_point = kDrakeAwaitLoc;
                return;
            }
            if (ri_opt.has_value()) ri_opt->resume_point = kDrakeDone;
            playIt(ctx, chosen, dests.empty() ? 0 : dests.front());
            return;
        }
        if (point == kDrakeAwaitLoc) {
            const int dest = takenValue(ctx, 0);
            chosen = (ri_opt.has_value() && !ri_opt->resume_data.empty())
                ? static_cast<GameObjectId>(ri_opt->resume_data[0]) : kInvalidId;
            if (ri_opt.has_value()) ri_opt->resume_point = kDrakeDone;
            playIt(ctx, chosen, dest);
        }
    }
private:
    /// Where the card may be played, valChoice-coded (0 = base, 1 + i =
    /// battlefield i): a unit to base or a battlefield its new controller
    /// controls (CR 355.2.a); a [Deploy] gear only to a battlefield (CR 830);
    /// other gear to base. Spells: none.
    // Where the played permanent may go (CR 355.2; EffectExecutor::
    // playLocationsFor — base, battlefields I control, its own permissions,
    // [Deploy] gear only to battlefields). 0 = base, 1 + id = a battlefield.
    static std::vector<int> destinations(CardContext& ctx, const GameObject& obj) {
        std::vector<int> out;
        for (const auto& l : ctx.executor.playLocationsFor(ctx.controller, obj.id)) {
            if (const auto* b = std::get_if<BattlefieldLocation>(&l))
                out.push_back(1 + static_cast<int>(b->id));
            else
                out.push_back(0);
        }
        return out;
    }
    static void playIt(CardContext& ctx, GameObjectId card, int dest) {
        if (card == kInvalidId || !ctx.state.objectExists(card)) return;
        const auto& obj = ctx.state.getObject(card);
        std::optional<LocationId> loc;
        if (!obj.isSpell())
            loc = dest <= 0 ? LocationId{BaseLocation{ctx.controller}}
                            : LocationId{BattlefieldLocation{static_cast<BattlefieldId>(dest - 1)}};
        // A spell goes on the chain and its controller chooses its targets as
        // it is finalized (GameEngine::chooseSpellTargetsAtFinalize).
        ctx.executor.playIgnoringCost(ctx.controller, card, loc);
    }

    const CardDef def_ = [] {
        CardDef d;
        d.id = 1003;
        d.def_id = R"RB(rad-018-167)RB";
        d.name = R"RB(Drakehound Packmaster)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-018/167)RB";
        d.collector_number = 18;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Noxus)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 9;
        d.power_cost = 2;
        d.might = 9;
        d.ability_text = R"RB(When you play me, reveal the top card of each opponent's Main Deck. Choose one and banish it, then play it, ignoring its cost. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9ddc2759cf8b039bc1ee77e32326c5475cc001e8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1003(CardRegistry& r) {
    r.registerCard(1003, std::make_unique<DrakehoundPackmaster>());
}

}  // namespace riftbound
