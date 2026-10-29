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

// "Reveal the top 2 cards of your Main Deck. You may banish one, then play it,
//  reducing its cost by [2]. Draw any you didn't banish." (2026-01-12 errata.)
//
// Which card to banish (or none) is the controller's choice. The banished card
// is played paying its cost minus [2] Energy; if that can't be paid it stays
// banished (banish-then-play, per the errata). Permanents land at base.
// resume_point kAwaitBanish while the choice is pending; resume_data[6]/[7] =
// the revealed cards ([0..5] belong to the choice helpers).

class VoidRush : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    static constexpr int kAwaitBanish = 20;
    static constexpr int kDone = 21;
    static constexpr int kAwaitWhere = 22;   // resume_data[8] = the banish pick

    static void banishAndPlay(CardContext& ctx, GameObjectId card,
                              LocationId where = BaseLocation{PlayerId::None}) {
        if (const auto* b = std::get_if<BaseLocation>(&where)) {
            if (b->player == PlayerId::None) where = BaseLocation{ctx.controller};
        }
        auto& ps = ctx.state.player(ctx.controller);
        ctx.executor.banishObject(card);
        ctx.events.logTrace("VOID RUSH: banished " + ctx.state.getObject(card).name +
                            " to play it");
        const EffectExecutor::EffectPlayCost reduced{.energy_reduction = 2};
        if (!ctx.state.objectExists(card) ||
            ctx.state.getObject(card).zone != ZoneType::Banishment) return;
        if (!ctx.executor.canPlayPayingCost(ctx.controller, card, reduced)) return;
        auto& bz = ps.banishment;
        bz.erase(std::remove(bz.begin(), bz.end(), card), bz.end());
        if (!ctx.executor.playPayingCost(ctx.controller, card, where, reduced) &&
            ctx.state.objectExists(card) &&
            ctx.state.getObject(card).zone == ZoneType::Banishment)
            bz.push_back(card);
    }
    // "Draw any you didn't banish" — the revealed cards still in the deck.
    static void drawRest(CardContext& ctx, const std::vector<GameObjectId>& revealed,
                         GameObjectId banished) {
        auto& ps = ctx.state.player(ctx.controller);
        for (auto cid : revealed) {
            if (cid == kInvalidId || cid == banished) continue;
            auto it = std::find(ps.main_deck.begin(), ps.main_deck.end(), cid);
            if (it == ps.main_deck.end()) continue;
            // Move it to the top so the draw takes exactly this card.
            ps.main_deck.erase(it);
            ps.main_deck.push_back(cid);
            ctx.executor.drawCards(ctx.controller, 1);
        }
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        auto& ri_opt = ctx.state.chain.resuming;
        if (ri_opt.has_value() && ri_opt->resume_point == kDone) return;

        if (ri_opt.has_value() && ri_opt->resume_point == kAwaitBanish) {
            auto& ri = *ri_opt;
            const std::vector<GameObjectId> revealed{
                static_cast<GameObjectId>(ri.resume_data[6]),
                static_cast<GameObjectId>(ri.resume_data[7])};
            GameObjectId picked = takenObject(ctx);
            const bool ok = picked != kInvalidId &&
                std::find(revealed.begin(), revealed.end(), picked) != revealed.end() &&
                std::find(ps.main_deck.begin(), ps.main_deck.end(), picked) != ps.main_deck.end();
            if (!ok) picked = kInvalidId;
            while (ri.resume_data.size() < 9) ri.resume_data.push_back(kInvalidId);
            ri.resume_data[8] = static_cast<int32_t>(picked);
            ri.resume_point = kAwaitWhere;
        }
        if (ri_opt.has_value() && ri_opt->resume_point == kAwaitWhere) {
            auto& ri = *ri_opt;
            const std::vector<GameObjectId> revealed{
                static_cast<GameObjectId>(ri.resume_data[6]),
                static_cast<GameObjectId>(ri.resume_data[7])};
            const auto picked = static_cast<GameObjectId>(ri.resume_data[8]);
            // A unit or gear goes where I choose (CR 355.2), before anything moves.
            LocationId where{BaseLocation{ctx.controller}};
            if (picked != kInvalidId &&
                pickPlayLocation(ctx, picked, where, PlayerId::None,
                                 "Void Rush: play it where?") == -1)
                return;   // waiting for the agent
            if (picked != kInvalidId) banishAndPlay(ctx, picked, where);
            drawRest(ctx, revealed, picked);
            ri.resume_point = kDone;
            return;
        }

        // Reveal the top 2 (top of deck = back of vector).
        ctx.executor.lookBeforeReveal(ctx.controller, ctx.controller);   // Void Hatchling
        std::vector<GameObjectId> top2;
        const int n = static_cast<int>(ps.main_deck.size());
        for (int i = 0; i < std::min(2, n); ++i) top2.push_back(ps.main_deck[n - 1 - i]);
        if (top2.empty()) return;
        for (auto cid : top2) {
            const auto& c = ctx.state.getObject(cid);
            ctx.events.emit(CardRevealedEvent{cid, c.card_def_id, c.owner,
                                              /*revealed_to_all=*/true, PlayerId::None,
                                              ZoneType::MainDeck});
        }
        ctx.events.logTrace("VOID RUSH: revealed top " + std::to_string(top2.size()) +
                            " card(s)");

        if (!ri_opt.has_value()) {
            // Direct invocation (no chain item to resume): no agent can be
            // asked; banish the first card that can be played.
            const EffectExecutor::EffectPlayCost reduced{.energy_reduction = 2};
            GameObjectId pick = kInvalidId;
            for (auto cid : top2)
                if (ctx.executor.canPlayPayingCost(ctx.controller, cid, reduced)) { pick = cid; break; }
            if (pick != kInvalidId) banishAndPlay(ctx, pick);
            drawRest(ctx, top2, pick);
            return;
        }
        auto& ri = *ri_opt;
        while (ri.resume_data.size() < 8) ri.resume_data.push_back(kInvalidId);
        ri.resume_data[6] = static_cast<int32_t>(top2[0]);
        ri.resume_data[7] = static_cast<int32_t>(top2.size() > 1 ? top2[1] : kInvalidId);
        std::vector<Intent> choices;
        for (auto cid : top2) choices.push_back(objChoice(ctx.controller, cid));
        Intent decline;   // "You may": banish none, draw both
        decline.type = IntentType::MakeChoice;
        decline.player = ctx.controller;
        choices.push_back(std::move(decline));
        ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                   "Void Rush: banish one and play it for [2] less (or none)");
        ri.resume_point = kAwaitBanish;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 505;
        d.def_id = R"RB(sfd-188-221)RB";
        d.name = R"RB(Void Rush)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-188/221)RB";
        d.collector_number = 188;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Order};
        d.tags = {R"RB(Rek'Sai)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Reveal the top 2 cards of your Main Deck. You may banish one, then play it, reducing its cost by [2]. Draw any you didn't banish.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ce606f096ae76485cd8dd3cc337629b510f0d1e7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_505(CardRegistry& r) {
    r.registerCard(505, std::make_unique<VoidRush>());
}

} // namespace riftbound
