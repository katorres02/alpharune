#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When I attack, you may reveal the top 2 cards of your Main Deck. You may
//  banish one, then play it. If it is a unit, you may play it here. Recycle the
//  rest." (2026-01-12 errata text.) The played card's cost is paid (CR
//  356.1.b.2); if it can't be paid it stays banished.
//
// Resumable. "you may reveal" is decided as the trigger is finalized
// (confirmOptional, resume_points 0-2). Then: kAwaitBanish (which card, or
// none), kAwaitWhere (where to play it: pickPlayLocation, "here" added for a
// unit). resume_data[6]/[7] = the revealed
// cards, [8] = the banished one ([0..5] belong to the choice helpers).

class RekSaiSwarmQueen : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }

    static constexpr int kAwaitBanish = 20;
    static constexpr int kAwaitWhere = 21;
    static constexpr int kDone = 22;

    static void finish(CardContext& ctx, ChainItem& ri) {
        // "Recycle the rest": the revealed cards still on top of the deck.
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> rest;
        for (int i : {6, 7}) {
            auto cid = static_cast<GameObjectId>(ri.resume_data[i]);
            if (cid == kInvalidId || cid == static_cast<GameObjectId>(ri.resume_data[8])) continue;
            auto it = std::find(ps.main_deck.begin(), ps.main_deck.end(), cid);
            if (it == ps.main_deck.end()) continue;
            ps.main_deck.erase(it);
            rest.push_back(cid);
        }
        if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
        ri.resume_point = kDone;
    }
    // Play the banished card (paying its cost) to `loc`; it stays banished
    // if the cost isn't paid.
    static void playBanished(CardContext& ctx, GameObjectId card,
                             std::optional<LocationId> loc) {
        auto& bz = ctx.state.player(ctx.controller).banishment;
        if (!ctx.state.objectExists(card) ||
            ctx.state.getObject(card).zone != ZoneType::Banishment) return;
        if (!ctx.executor.canPlayPayingCost(ctx.controller, card)) return;
        bz.erase(std::remove(bz.begin(), bz.end(), card), bz.end());
        if (!ctx.executor.playPayingCost(ctx.controller, card, loc) &&
            ctx.state.objectExists(card) &&
            ctx.state.getObject(card).zone == ZoneType::Banishment)
            bz.push_back(card);
    }
    static std::optional<BattlefieldId> here(const CardContext& ctx) {
        if (!ctx.state.objectExists(ctx.source)) return std::nullopt;
        return ctx.state.getObject(ctx.source).battlefieldId();
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        int go = confirmOptional(ctx, "Rek'Sai: reveal the top 2 cards of your Main Deck?",
                                 [&]() { return !ps.main_deck.empty(); });
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (go != 1) return;
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        while (ri.resume_data.size() < 9) ri.resume_data.push_back(kInvalidId);

        if (ri.resume_point == kDone) return;
        if (ri.resume_point < kAwaitBanish) {
            const int n = std::min(2, static_cast<int>(ps.main_deck.size()));
            if (n == 0) { ri.resume_point = kDone; return; }
            std::vector<Intent> choices;
            for (int i = 0; i < n; ++i) {
                GameObjectId cid = ps.main_deck[ps.main_deck.size() - 1 - i];
                ri.resume_data[6 + i] = static_cast<int32_t>(cid);
                const auto& obj = ctx.state.getObject(cid);
                ctx.events.emit(CardRevealedEvent{cid, obj.card_def_id, obj.owner,
                                                  /*revealed_to_all=*/true, PlayerId::None,
                                                  ZoneType::MainDeck});
                choices.push_back(objChoice(ctx.controller, cid));
            }
            // "You may banish one": declining is an option (empty chosen_objects).
            Intent decline;
            decline.type = IntentType::MakeChoice;
            decline.player = ctx.controller;
            choices.push_back(std::move(decline));
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                       "Rek'Sai: banish one of the top 2 and play it (or none)");
            ri.resume_point = kAwaitBanish;
            return;
        }

        if (ri.resume_point == kAwaitBanish) {
            GameObjectId picked = takenObject(ctx);
            const bool revealed = picked != kInvalidId &&
                (picked == static_cast<GameObjectId>(ri.resume_data[6]) ||
                 picked == static_cast<GameObjectId>(ri.resume_data[7])) &&
                std::find(ps.main_deck.begin(), ps.main_deck.end(), picked) != ps.main_deck.end();
            if (!revealed) { finish(ctx, ri); return; }
            ri.resume_data[8] = static_cast<int32_t>(picked);
            ctx.executor.banishObject(picked);
            ctx.events.logTrace("REK'SAI: banished " + ctx.state.getObject(picked).name +
                                " to play it");
            ri.resume_point = kAwaitWhere;
        }

        if (ri.resume_point == kAwaitWhere) {
            const auto picked = static_cast<GameObjectId>(ri.resume_data[8]);
            if (!ctx.state.objectExists(picked) ||
                !ctx.executor.canPlayPayingCost(ctx.controller, picked)) {
                finish(ctx, ri);   // can't be paid for: it stays banished
                return;
            }
            // Where it goes (CR 355.2): base, a battlefield I control, and —
            // "If it is a unit, you may play it here" — my battlefield.
            std::vector<LocationId> extra;
            if (ctx.state.getObject(picked).isUnit())
                if (auto bf = here(ctx)) extra.push_back(BattlefieldLocation{*bf});
            LocationId loc;
            const int r = pickPlayLocation(ctx, picked, loc, PlayerId::None,
                                           "Rek'Sai: play it where?", extra);
            if (r == -1) return;   // waiting for the agent
            if (r == 1) playBanished(ctx, picked, loc);
            finish(ctx, ri);
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 491;
        d.def_id = R"RB(sfd-170-221)RB";
        d.name = R"RB(Rek'Sai, Swarm Queen)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-170/221)RB";
        d.collector_number = 170;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Rek'Sai)RB", R"RB(The Void)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I attack, you may reveal the top 2 cards of your Main Deck. You may banish one, then play it. If it is a unit, you may play it here. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5470aaad3bdb7ff5d2e605b07d93cedb7254c54d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_491(CardRegistry& r) {
    r.registerCard(491, std::make_unique<RekSaiSwarmQueen>());
}

} // namespace riftbound
