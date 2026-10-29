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

// "Choose one — Move a friendly gear with [Deploy] to a battlefield.
//  Look at the top 5 cards of your Main Deck. You may reveal a gear from among
//  them and draw it. Recycle the rest."
// The mode and the gear to move are chosen as the spell is finalized; the
// destination and the card to draw are picked as it resolves (resume points
// 20-22; the looked-at cards ride in resume_data[10..]).
class CommandAttack : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const uint32_t legal = 0x2u | (deployGear(ctx).empty() ? 0u : 0x1u);
        const int mode = pickMode(ctx, "Command: Attack", 2,
                                  {"Move a friendly [Deploy] gear to a battlefield",
                                   "Look at the top 5, draw a gear"}, legal);
        if (mode == -1) return;
        if (mode < 0) { finalizingChoices(ctx); return; }
        GameObjectId gear = kInvalidId;
        if (mode == 0) {
            gear = pickTarget(ctx, "Command: Attack: move a [Deploy] gear", deployGear(ctx));
            if (ctx.executor.hasPendingChoice()) return;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (mode == 0) moveGear(ctx, gear);
        else lookAtTopFive(ctx);
    }
private:
    // Friendly gear on the board with [Deploy].
    static std::vector<GameObjectId> deployGear(const CardContext& ctx) {
        std::vector<GameObjectId> out;
        if (!ctx.registry) return out;
        for (const auto& [id, o] : ctx.state.objects) {
            if (!o.isGear() || o.controller != ctx.controller || !o.location.has_value()) continue;
            if (o.attached_to.has_value()) continue;
            const Card* c = ctx.registry->get(o.card_def_id);
            if (c && c->hasDeploy()) out.push_back(id);
        }
        return out;
    }
    static std::vector<BattlefieldId> destinations(const CardContext& ctx, GameObjectId gear) {
        std::vector<BattlefieldId> out;
        if (!ctx.state.objectExists(gear)) return out;
        auto here = ctx.state.getObject(gear).battlefieldId();
        for (const auto& bf : ctx.state.battlefields)
            if (!here || *here != bf.id) out.push_back(bf.id);
        return out;
    }
    void moveGear(CardContext& ctx, GameObjectId gear) {
        if (gear == kInvalidId || !ctx.state.objectExists(gear)) return;
        auto dests = destinations(ctx, gear);
        if (dests.empty()) return;
        if (!ctx.state.chain.resuming.has_value()) {   // direct invocation
            ctx.executor.moveToBattlefield(gear, dests.front());
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point < 20) {
            std::vector<Intent> opts;
            for (auto bf : dests)
                opts.push_back(valChoice(ctx.controller, static_cast<int>(bf),
                                         "Battlefield #" + std::to_string(bf)));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Command: Attack: move the gear to a battlefield");
            ri.resume_point = 21;
            return;
        }
        if (ri.resume_point != 21) return;
        ri.resume_point = 22;
        const int picked = takenValue(ctx, static_cast<int>(dests.front()));
        BattlefieldId dest = dests.front();
        for (auto bf : dests) if (static_cast<int>(bf) == picked) dest = bf;
        ctx.executor.moveToBattlefield(gear, dest);
        ctx.events.logTrace("COMMAND: ATTACK: moved " + ctx.state.getObject(gear).name +
                            " to battlefield #" + std::to_string(dest));
    }
    void lookAtTopFive(CardContext& ctx) {
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> looked;
        const bool chained = ctx.state.chain.resuming.has_value();
        if (chained && ctx.state.chain.resuming->resume_point >= 20) {
            const auto& rd = ctx.state.chain.resuming->resume_data;
            for (size_t i = 10; i < rd.size(); ++i) looked.push_back(static_cast<GameObjectId>(rd[i]));
        } else {
            for (auto it = ps.main_deck.rbegin(); it != ps.main_deck.rend() && looked.size() < 5; ++it)
                looked.push_back(*it);
            for (auto cid : looked) {
                const auto& c = ctx.state.getObject(cid);
                ctx.events.emit(CardRevealedEvent{cid, c.card_def_id, c.owner,
                                                  /*revealed_to_all=*/false, ctx.controller,
                                                  ZoneType::MainDeck});
            }
        }
        std::vector<GameObjectId> gear;
        for (auto cid : looked)
            if (ctx.state.objectExists(cid) && ctx.state.getObject(cid).isGear()) gear.push_back(cid);

        GameObjectId picked = kInvalidId;
        if (!chained) {
            if (!gear.empty()) picked = gear.front();
        } else {
            auto& ri = *ctx.state.chain.resuming;
            if (ri.resume_point < 20) {
                ri.resume_data.resize(10, 0);
                for (auto cid : looked) ri.resume_data.push_back(static_cast<int32_t>(cid));
                if (!gear.empty()) {
                    std::vector<Intent> opts;
                    for (auto g : gear) opts.push_back(objChoice(ctx.controller, g));
                    opts.push_back(valChoice(ctx.controller, 0, "Don't reveal a gear"));
                    ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                               "Command: Attack: reveal a gear and draw it");
                    ri.resume_point = 21;
                    return;
                }
                ri.resume_point = 21;
            } else if (ri.resume_point == 21) {
                if (!gear.empty()) picked = takenObject(ctx);
            } else {
                return;   // already done
            }
            ri.resume_point = 22;
        }
        if (std::find(gear.begin(), gear.end(), picked) == gear.end()) picked = kInvalidId;

        std::vector<GameObjectId> rest;
        for (auto cid : looked) {
            auto it = std::find(ps.main_deck.begin(), ps.main_deck.end(), cid);
            if (it == ps.main_deck.end()) continue;   // no longer in the deck
            ps.main_deck.erase(it);
            if (cid != picked) rest.push_back(cid);
        }
        if (picked != kInvalidId) {
            const auto& c = ctx.state.getObject(picked);
            ctx.events.emit(CardRevealedEvent{picked, c.card_def_id, c.owner,
                                              /*revealed_to_all=*/true, PlayerId::None,
                                              ZoneType::MainDeck});
            ps.main_deck.push_back(picked);   // back on top, then drawn normally
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("COMMAND: ATTACK: revealed and drew " + c.name);
        }
        if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1016;
        d.def_id = R"RB(rad-033-167)RB";
        d.name = R"RB(Command: Attack)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-033/167)RB";
        d.collector_number = 33;
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.rarity = Rarity::Uncommon;
        d.energy_cost = 1;
        d.power_cost = 1;
        d.ability_text = R"RB(Choose one —
Move a friendly gear with [Deploy] to a battlefield.
Look at the top 5 cards of your Main Deck. You may reveal a gear from among them and draw it. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b025756992e22af480902e13b01e25acbc747221-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1016(CardRegistry& r) {
    r.registerCard(1016, std::make_unique<CommandAttack>());
}

}  // namespace riftbound
