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

// "[Repeat] — [1] / [A] / [1][A]. Choose one you haven't already chosen —
//  Draw 1. / Deal 2 to a unit at a battlefield. / Deal 3 to a unit at a base. /
//  Give a unit at a battlefield -4 [M] this turn."
//
// Three separate Repeat costs, each paid at most once (the engine's Repeat
// parser handles a single repeatable cost, so this card runs its own): every
// one paid adds one more resolution of the effect, each with a mode not chosen
// before. Costs, modes and targets are all chosen as the spell is put on the
// chain (kFinalizeResolve, CR 355 / 820); the effects happen as it resolves,
// and a target that is no longer legal then is skipped.
//
// Resumable layout (stepChooseValue / stepChooseObject):
//   tranche t (0..2): resume_point 20+2t, resume_data[10+t] (1 = paid)
//   execution i:      mode at 30+4i -> data[13+2i]; target at 32+4i -> data[14+2i]
class CurtainCall : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }

    enum Mode { kDraw = 0, kDeal2Bf = 1, kDeal3Base = 2, kMinus4Bf = 3 };
    static bool needsBf(int m) { return m == kDeal2Bf || m == kMinus4Bf; }

    static std::vector<GameObjectId> modeTargets(CardContext& ctx, int m) {
        if (m == kDraw) return {};
        const PlayerId me = ctx.controller;
        const int payable = ctx.executor.payableAnyPower(me);
        auto out = chooseableUnits(ctx.state, me, [&](const GameObject& o) {
            if (needsBf(m) ? !o.isAtBattlefield() : !o.isAtBase()) return false;
            return ctx.executor.deflectCostFor(me, o.id) <= payable;
        });
        std::sort(out.begin(), out.end());
        return out;
    }
    static bool stillLegal(const CardContext& ctx, int m, GameObjectId t) {
        if (!ctx.state.objectExists(t)) return false;
        const auto& o = ctx.state.getObject(t);
        if (!o.isUnit() || !o.location.has_value()) return false;
        return needsBf(m) ? o.isAtBattlefield() : o.isAtBase();
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto* ri = ctx.state.chain.resuming.has_value() ? &*ctx.state.chain.resuming : nullptr;
        auto rp_now = [&]() { return ri ? ri->resume_point : 0; };
        const PlayerId me = ctx.controller;

        // ── Repeat costs: [1], [A], [1][A] ──
        int executions = 1;
        for (int t = 0; t < 3; ++t) {
            const int e = (t == 1) ? 0 : 1, p = (t == 0) ? 0 : 1;
            const bool affordable = payableEnergy(ctx.state, me) >= e &&
                                    (p == 0 || payableOnePower(ctx.state, me, std::nullopt));
            static const char* kLabels[3] = {"[1]", "[A]", "[1][A]"};
            std::vector<std::pair<int, std::string>> opts;
            if (affordable)
                opts = {{0, "Don't pay"}, {1, std::string("Pay ") + kLabels[t] + " to repeat"}};
            const int rp = 20 + 2 * t;
            const int before = rp_now();
            int paid = 0;
            if (!stepChooseValue(ctx, me, opts, std::string("Curtain Call: [Repeat] ") + kLabels[t],
                                 rp, 10 + t, 0, paid))
                return;
            if (paid == 1 && (!ri || before == rp)) {   // answered just now: pay it
                if (e > 0) payEnergy(ctx, me, e);
                if (p > 0) payOnePower(ctx, me, std::nullopt);
                ctx.events.logTrace(std::string("CURTAIN CALL: paid Repeat ") + kLabels[t]);
            }
            if (paid == 1) ++executions;
        }

        // ── Modes and targets, one per execution ──
        std::vector<int> modes;
        std::vector<GameObjectId> tgts;
        uint32_t used = 0;
        for (int i = 0; i < executions; ++i) {
            std::vector<std::pair<int, std::string>> opts;
            static const char* kModeLabels[4] = {"Draw 1", "Deal 2 to a unit at a battlefield",
                                                 "Deal 3 to a unit at a base",
                                                 "Give a unit at a battlefield -4 [M] this turn"};
            for (int m = 0; m < 4; ++m) {
                if (used & (1u << m)) continue;
                if (m != kDraw && modeTargets(ctx, m).empty()) continue;
                opts.push_back({m, kModeLabels[m]});
            }
            int mode = -1;
            if (!stepChooseValue(ctx, me, opts, "Curtain Call: choose one", 30 + 4 * i,
                                 13 + 2 * i, -1, mode))
                return;
            if (mode < 0) break;                     // nothing left to choose
            used |= (1u << mode);
            GameObjectId t = kInvalidId;
            if (mode != kDraw) {
                const int rp = 32 + 4 * i;
                const int before = rp_now();
                if (!stepChooseObject(ctx, me, modeTargets(ctx, mode),
                                      std::string("Curtain Call: ") + kModeLabels[mode],
                                      rp, 14 + 2 * i, t))
                    return;
                if (t != kInvalidId && (!ri || before == rp)) {
                    // Chosen just now: Deflect (CR 809) is paid as it is chosen.
                    const int deflect = ctx.executor.deflectCostFor(me, t);
                    if (deflect > 0) ctx.executor.payAnyPower(me, deflect);
                }
            }
            modes.push_back(mode);
            tgts.push_back(t);
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here

        for (size_t i = 0; i < modes.size(); ++i) {
            const int m = modes[i];
            const GameObjectId t = tgts[i];
            if (m == kDraw) {
                ctx.events.logTrace("CURTAIN CALL: Draw 1");
                ctx.executor.drawCards(me, 1);
                continue;
            }
            if (!stillLegal(ctx, m, t)) {
                ctx.events.logTrace("CURTAIN CALL: target no longer legal, mode skipped");
                continue;
            }
            switch (m) {
            case kDeal2Bf:
                ctx.events.logTrace("CURTAIN CALL: Deal 2 (BF)");
                ctx.executor.dealDamage(t, 2, ctx.source);
                break;
            case kDeal3Base:
                ctx.events.logTrace("CURTAIN CALL: Deal 3 (base)");
                ctx.executor.dealDamage(t, 3, ctx.source);
                break;
            case kMinus4Bf:
                ctx.events.logTrace("CURTAIN CALL: -4M (BF)");
                ctx.executor.giveTemporaryMight(t, -4, /*minimum=*/0);
                break;
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 743;
        d.def_id = R"RB(unl-182-219)RB";
        d.name = R"RB(Curtain Call)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-182/219)RB";
        d.collector_number = 182;
        d.artist = R"RB(华锐)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Mind};
        d.tags = {R"RB(Jhin)RB"};
        d.energy_cost = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] — [1] / [A] / [1][A] (You may pay each additional cost to repeat this spell's effect.)
Choose one you haven't already chosen —
Draw 1.Deal 2 to a unit at a battlefield.Deal 3 to a unit at a base.Give a unit at a battlefield -4 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/01496068d9a8d5567672d802bd008297bd1fe8cc-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_743(CardRegistry& r) {
    r.registerCard(743, std::make_unique<CurtainCall>());
}

} // namespace riftbound
