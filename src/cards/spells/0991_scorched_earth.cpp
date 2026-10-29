#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace riftbound {
namespace {

// ── Split damage (CR 355.14), as in 0751_alpha_strike.cpp ──────────────────
// The controller builds the division one point at a time (each prompt assigns
// the next point to a unit they pick); damage is marked once the whole
// division is known. Unlike Alpha Strike's copy, the scratch data lives at
// resume_data[kSeSplitBase..] so it doesn't clobber pickMode's resume_data[1]
// (the battlefield chosen first).
// ENGINE-GAP (as Alpha Strike): the units are chosen at resolution, not
// declared as targets when the spell is finalized (no variable-count target
// declaration in the play path).
constexpr int kSeSplitAssign = 100;
constexpr int kSeSplitAwait = 101;
constexpr int kSeSplitDone = 102;
constexpr int kSeSplitMaxPicks = 12;
constexpr size_t kSeSplitBase = 8;   // resume_data[8] total, [9] per pick, [10..] picks

bool scorchedEarthSplit(CardContext& ctx, const std::string& label, int total,
                        std::vector<GameObjectId> legal) {
    std::sort(legal.begin(), legal.end());
    std::vector<std::pair<GameObjectId, int>> alloc;
    auto add = [&alloc](GameObjectId id, int n) {
        for (auto& a : alloc) if (a.first == id) { a.second += n; return; }
        alloc.emplace_back(id, n);
    };
    auto deal = [&]() {
        for (const auto& [id, n] : alloc) {
            if (n <= 0 || !ctx.state.objectExists(id)) continue;
            if (ctx.state.getObject(id).controller != ctx.controller)
                ctx.state.player(ctx.controller).chose_enemy_unit_this_turn = true;
            ctx.events.logTrace("SPLIT: " + label + " -> " + std::to_string(n) +
                                " to " + ctx.state.getObject(id).name);
            ctx.executor.dealDamage(id, n, ctx.source);
        }
    };
    auto& ri_opt = ctx.state.chain.resuming;
    if (!ri_opt.has_value()) {
        if (total <= 0 || legal.empty()) return true;
        for (int i = 0; i < total; ++i) add(legal[i % legal.size()], 1);
        deal();
        return true;
    }
    auto& ri = *ri_opt;
    if (ri.resume_point == kSeSplitDone) return true;
    auto alreadyPicked = [&ri](GameObjectId id) {
        for (size_t i = kSeSplitBase + 2; i < ri.resume_data.size(); ++i)
            if (static_cast<GameObjectId>(ri.resume_data[i]) == id) return true;
        return false;
    };
    auto offered = [&]() {
        const int payable = ctx.executor.payableAnyPower(ctx.controller);
        std::vector<GameObjectId> out;
        for (auto t : legal)
            if (alreadyPicked(t) || ctx.executor.deflectCostFor(ctx.controller, t) <= payable)
                out.push_back(t);
        return out;
    };
    if (ri.resume_point == kSeSplitAwait) {
        auto picked = ctx.executor.takeChoice();
        auto can_pick = offered();
        GameObjectId t = can_pick.empty() ? kInvalidId : can_pick.front();
        if (picked.has_value() && !picked->chosen_objects.empty() &&
            std::find(can_pick.begin(), can_pick.end(), picked->chosen_objects.front()) != can_pick.end())
            t = picked->chosen_objects.front();
        if (t != kInvalidId && !alreadyPicked(t)) {
            const int cost = ctx.executor.deflectCostFor(ctx.controller, t);
            if (cost > 0) ctx.executor.payAnyPower(ctx.controller, cost);
            ri.chosen_objects.push_back(t);
        }
        ri.resume_data.push_back(static_cast<int32_t>(t));
        ri.resume_point = kSeSplitAssign;
    } else if (ri.resume_point != kSeSplitAssign) {
        if (total <= 0 || legal.empty()) { ri.resume_point = kSeSplitDone; return true; }
        const int per_pick = legal.size() == 1
            ? total : (total + kSeSplitMaxPicks - 1) / kSeSplitMaxPicks;
        while (ri.resume_data.size() < kSeSplitBase) ri.resume_data.push_back(0);
        ri.resume_data.resize(kSeSplitBase);
        ri.resume_data.push_back(total);
        ri.resume_data.push_back(per_pick);
        ri.resume_point = kSeSplitAssign;
        if (offered().empty()) { ri.resume_point = kSeSplitDone; return true; }
    }
    total = ri.resume_data[kSeSplitBase];
    const int per_pick = std::max(1, ri.resume_data[kSeSplitBase + 1]);
    int remaining = total;
    for (size_t i = kSeSplitBase + 2; i < ri.resume_data.size() && remaining > 0; ++i) {
        const int n = std::min(per_pick, remaining);
        add(static_cast<GameObjectId>(ri.resume_data[i]), n);
        remaining -= n;
    }
    const auto can_pick = remaining > 0 ? offered() : std::vector<GameObjectId>{};
    if (remaining > 0 && !can_pick.empty()) {
        std::vector<Intent> options;
        for (auto t : can_pick) options.push_back(objChoice(ctx.controller, t));
        ctx.executor.requestChoice(ctx.controller, std::move(options),
                                   "split damage: " + label + " (" + std::to_string(remaining) + " left)");
        ri.resume_point = kSeSplitAwait;
        return false;
    }
    ri.resume_point = kSeSplitDone;
    deal();
    return true;
}

std::vector<GameObjectId> scorchedEarthUnitsAt(const GameState& state, PlayerId me, BattlefieldId bf) {
    return chooseableUnits(state, me, [bf](const GameObject& o) {
        auto b = o.battlefieldId();
        return b && *b == bf;
    });
}

// "Deal 5 damage split among units at a single battlefield." The battlefield
// is picked first (resume via pickMode), then the division.
class ScorchedEarth : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (const auto& bf : state.battlefields)
            if (!scorchedEarthUnitsAt(state, controller, bf.id).empty()) return true;
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const int nbf = static_cast<int>(ctx.state.battlefields.size());
        if (nbf <= 0) return;
        uint32_t legal = 0;
        std::vector<std::string> labels;
        for (int i = 0; i < nbf; ++i) {
            labels.push_back("Battlefield #" + std::to_string(i));
            if (i < 32 && !scorchedEarthUnitsAt(ctx.state, ctx.controller,
                                                ctx.state.battlefields[i].id).empty())
                legal |= (1u << i);
        }
        const int pick = pickMode(ctx, "Scorched Earth: pick a battlefield", nbf, labels, legal);
        if (pick < 0) return;
        const BattlefieldId bf = ctx.state.battlefields[pick].id;
        scorchedEarthSplit(ctx, "Scorched Earth", 5, scorchedEarthUnitsAt(ctx.state, ctx.controller, bf));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 991;
        d.def_id = R"RB(rad-006-167)RB";
        d.name = R"RB(Scorched Earth)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-006/167)RB";
        d.collector_number = 6;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        // Gallery data lists no domain (pre-release); the card costs [4] + one
        // power and sits in the Fury block of the set, so it is Fury.
        d.domains = {Domain::Fury};
        d.tags = {};
        d.rarity = Rarity::Common;
        d.energy_cost = 4;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Deal 5 damage split among units at a single battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e646f9850571e20eca22e07af5d4dd68813fdd19-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_991(CardRegistry& r) {
    r.registerCard(991, std::make_unique<ScorchedEarth>());
}

}  // namespace riftbound
