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

class DivineJudgment : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Each player chooses 2 units, 2 gear, 2 runes, and 2 cards in their hands.
    //  Recycle the rest."
    // Choices: for each player (controller first) and each category with more
    // than 2 candidates, that player picks the 2 to keep, one at a time.
    // Resume points 20/21/22; resume_data[15] = step (player * 4 + category),
    // resume_data[16..] = every kept object. Board objects leave the board
    // properly: gear comes off its unit, tokens cease to exist (CR 183.1), and
    // each departure is announced (LeftBoardEvent).
    static constexpr size_t kStep = 15;
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId players[2] = {ctx.controller, opponent(ctx.controller)};
        std::vector<GameObjectId> keeps;
        if (!ctx.state.chain.resuming.has_value()) {
            // Direct invocation (test path): keep the first 2 of each category.
            for (auto p : players)
                for (int c = 0; c < 4; ++c) {
                    auto cands = candidates(ctx.state, p, c);
                    for (size_t i = 0; i < cands.size() && i < 2; ++i) keeps.push_back(cands[i]);
                }
        } else {
            auto& ri = *ctx.state.chain.resuming;
            if (ri.resume_point < 20) {
                if (ri.resume_data.size() < kStep + 1) ri.resume_data.resize(kStep + 1, 0);
                ri.resume_data.resize(kStep + 1);
                ri.resume_data[kStep] = 0;
                ri.resume_point = 20;
            }
            for (size_t i = kStep + 1; i < ri.resume_data.size(); ++i)
                keeps.push_back(static_cast<GameObjectId>(ri.resume_data[i]));
            auto isKept = [&](GameObjectId id) {
                return std::find(keeps.begin(), keeps.end(), id) != keeps.end();
            };
            if (ri.resume_point == 21) {
                const int step = ri.resume_data[kStep];
                GameObjectId got = takenObject(ctx);
                auto cands = candidates(ctx.state, players[step / 4], step % 4);
                if (got != kInvalidId && !isKept(got) &&
                    std::find(cands.begin(), cands.end(), got) != cands.end()) {
                    ri.resume_data.push_back(static_cast<int32_t>(got));
                    keeps.push_back(got);
                }
                ri.resume_point = 20;
            }
            while (ri.resume_data[kStep] < 8) {
                const int step = ri.resume_data[kStep];
                const PlayerId p = players[step / 4];
                auto cands = candidates(ctx.state, p, step % 4);
                int kept_here = 0;
                for (auto id : cands) if (isKept(id)) ++kept_here;
                if (cands.size() <= 2 || kept_here >= 2) { ri.resume_data[kStep] = step + 1; continue; }
                std::vector<Intent> opts;
                for (auto id : cands) if (!isKept(id)) opts.push_back(objChoice(p, id));
                static const char* kNames[4] = {"unit", "gear", "rune", "card in hand"};
                ctx.executor.requestChoice(p, std::move(opts),
                    std::string("Divine Judgment: choose a ") + kNames[step % 4] + " to keep");
                ri.resume_point = 21;
                return;
            }
            ri.resume_point = 22;
        }

        for (auto p : players) {
            std::vector<GameObjectId> board, hand;
            for (int c = 0; c < 4; ++c) {
                auto cands = candidates(ctx.state, p, c);
                if (cands.size() <= 2) continue;
                for (auto id : cands)
                    if (std::find(keeps.begin(), keeps.end(), id) == keeps.end())
                        (c == 3 ? hand : board).push_back(id);
            }
            recycleFromBoard(ctx, p, board);
            auto& ps = ctx.state.player(p);
            for (auto cid : hand)
                ps.hand.erase(std::remove(ps.hand.begin(), ps.hand.end(), cid), ps.hand.end());
            if (!hand.empty()) ctx.executor.recycleCards(p, hand);
        }
        ctx.events.logTrace("DIVINE JUDGMENT: each player keeps 2/category, recycle rest");
    }

private:
    // Category 0 = units, 1 = gear, 2 = runes (on the board), 3 = hand.
    static std::vector<GameObjectId> candidates(const GameState& state, PlayerId p, int category) {
        std::vector<GameObjectId> out;
        if (category == 3) return state.player(p).hand;
        for (auto& [id, obj] : state.objects) {
            if (obj.controller != p || !obj.location.has_value()) continue;
            if ((category == 0 && obj.isUnit()) || (category == 1 && obj.isGear()) ||
                (category == 2 && obj.isRune()))
                out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    static void recycleFromBoard(CardContext& ctx, PlayerId p, const std::vector<GameObjectId>& ids) {
        std::vector<GameObjectId> to_deck;
        std::vector<LeftBoardEvent> left;
        for (auto id : ids) {
            if (!ctx.state.objectExists(id)) continue;
            auto& obj = ctx.state.getObject(id);
            if (!obj.location.has_value()) continue;
            // Gear comes off its unit; a recycled unit's gear stays on the board.
            if (obj.isGear() && obj.attached_to.has_value()) ctx.executor.unattachGear(id);
            if (obj.isUnit()) {
                auto attached = obj.attachments;
                for (auto g : attached) ctx.executor.unattachGear(g);
            }
            auto& o = ctx.state.getObject(id);
            const LocationId was_at = *o.location;
            const PlayerId controller = o.controller;
            if (o.isToken()) {
                // CR 183.1: a token put into a non-board zone ceases to exist.
                o.zone = ZoneType::Banishment;
                o.location = std::nullopt;
                o.damage_marked = 0;
                o.combat_designation = CombatDesignation::None;
                ctx.events.emit(LeftBoardEvent{id, controller, o.card_type, was_at,
                                               ZoneType::Banishment, false});
                continue;
            }
            o.damage_marked = 0;
            o.combat_designation = CombatDesignation::None;
            o.is_exhausted = false;
            to_deck.push_back(id);
            left.push_back(LeftBoardEvent{id, controller, o.card_type, was_at,
                o.isRune() ? ZoneType::RuneDeck : ZoneType::MainDeck, false});
        }
        if (!to_deck.empty()) ctx.executor.recycleCards(p, to_deck);
        for (const auto& e : left) ctx.events.emit(e);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 244;
        d.def_id = R"RB(ogn-244-298)RB";
        d.name = R"RB(Divine Judgment)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-244/298)RB";
        d.collector_number = 244;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Each player chooses 2 units, 2 gear, 2 runes, and 2 cards in their hands. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/995778eea2e24fdc62ade38c2baa25b9f1e6ab79-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_244(CardRegistry& r) {
    r.registerCard(244, std::make_unique<DivineJudgment>());
}

} // namespace riftbound
