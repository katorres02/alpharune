#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class Encore : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose two — Deal 2 to a unit. / Play two 1 [M] Recruit unit tokens. /
    // Give up to two units each +1 [M] this turn. / Look at the top 2 cards of
    // your Main Deck, draw 1 and recycle the other."
    // Modes and the units they choose are decided as it is played (CR 355);
    // which card to draw is decided as it resolves (the cards are looked at then).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto modes = pickSeveral(ctx, ctx.controller, "Encore: choose two", {0, 1, 2, 3}, true,
                                 {"Deal 2 to a unit", "Play two Recruit tokens",
                                  "Up to two units +1 [M]", "Look at top 2, draw 1"},
                                 2, 2, kModesSlot);
        if (!modes) return;
        auto chosen = [&](int m) {
            return std::find(modes->begin(), modes->end(), m) != modes->end();
        };
        const PlayerId me = ctx.controller;
        auto any_unit = [](const GameObject&) { return true; };

        GameObjectId hit = kInvalidId;
        if (chosen(0)) {
            hit = pickTarget(ctx, "Encore: deal 2 to a unit", chooseableUnits(ctx.state, me, any_unit));
            if (ctx.executor.hasPendingChoice()) return;
        }
        std::vector<int> pumped;
        if (chosen(2)) {
            std::vector<int> units;
            for (auto id : chooseableUnits(ctx.state, me, any_unit)) units.push_back(static_cast<int>(id));
            std::sort(units.begin(), units.end());
            auto p = pickSeveral(ctx, me, "Encore: give a unit +1 [M]", units, false, {}, 0, 2, kPumpSlot);
            if (!p) return;
            pumped = *p;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)

        // Look at the top 2 and pick the card to draw before anything happens.
        GameObjectId keep = kInvalidId, other = kInvalidId;
        if (chosen(3)) {
            const auto& deck = ctx.state.player(me).main_deck;   // top = back
            std::vector<int> top;
            for (size_t i = 0; i < 2 && i < deck.size(); ++i)
                top.push_back(static_cast<int>(deck[deck.size() - 1 - i]));
            if (!top.empty()) {
                auto k = pickSeveral(ctx, me, "Encore: the card to draw", top, false, {}, 1, 1, kDrawSlot);
                if (!k) return;
                keep = k->empty() ? static_cast<GameObjectId>(top[0]) : static_cast<GameObjectId>((*k)[0]);
                for (int t : top)
                    if (static_cast<GameObjectId>(t) != keep) other = static_cast<GameObjectId>(t);
            }
        }

        if (chosen(0) && hit != kInvalidId && ctx.state.objectExists(hit) &&
            ctx.state.getObject(hit).location.has_value()) {
            ctx.executor.dealDamage(hit, 2, ctx.source);
            if (ctx.state.objectExists(hit) && ctx.state.getObject(hit).location.has_value() &&
                ctx.state.getObject(hit).hasLethalDamage())
                ctx.executor.killObject(hit);
        }
        if (chosen(1)) {
            for (int i = 0; i < 2; ++i) {
                auto tok = ctx.executor.createToken(me, CardType::Unit, "Recruit", 1, {"Recruit"},
                                                    KeywordSet{}, LocationId{BaseLocation{me}}, false);
                if (ctx.state.objectExists(tok)) ctx.state.getObject(tok).card_def_id = kRecruitDef;
            }
        }
        if (chosen(2))
            for (int v : pumped) {
                const auto id = static_cast<GameObjectId>(v);
                if (ctx.state.objectExists(id) && ctx.state.getObject(id).location.has_value())
                    ctx.executor.giveTemporaryMight(id, 1);
            }
        if (chosen(3) && keep != kInvalidId) {
            auto& deck = ctx.state.player(me).main_deck;
            auto it = std::find(deck.begin(), deck.end(), keep);
            if (it != deck.end()) {
                deck.erase(it);
                deck.push_back(keep);   // on top, then draw it
                ctx.executor.drawCards(me, 1);
            }
            auto ot = std::find(deck.begin(), deck.end(), other);
            if (other != kInvalidId && ot != deck.end()) {
                deck.erase(ot);   // recycleCards puts it on the bottom
                ctx.executor.recycleCards(me, {other});
            }
        }
    }
private:
    static constexpr CardDefId kRecruitDef = 954;
    static constexpr size_t kModesSlot = 8;    // 8..11
    static constexpr size_t kPumpSlot = 12;    // 12..15
    static constexpr size_t kDrawSlot = 16;    // 16..18
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 980;
        d.def_id = R"RB(rad-152-167)RB";
        d.name = R"RB(Encore)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-152/167)RB";
        d.collector_number = 152;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind,Domain::Order};
        d.tags = {R"RB(Seraphine)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 3;
        d.power_cost = 1;
        d.ability_text = R"RB(Choose two —
Deal 2 to a unit.
Play two 1 [M] Recruit unit tokens.
Give up to two units each +1 [M] this turn.
Look at the top 2 cards of your Main Deck, draw 1 and recycle the other.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0200aeca5d0a1cca4b8ab5b3f3910207118ee4e4-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_980(CardRegistry& r) {
    r.registerCard(980, std::make_unique<Encore>());
}

}  // namespace riftbound
