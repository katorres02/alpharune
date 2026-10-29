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

class Chronobreak : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "As an additional cost to play this, pay any amount of Power." The amount
    // and its payment are part of playing it (finalization, CR 355).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto mine = friendlyUnits(ctx.state, ctx.controller);
        const int max_x = std::min(static_cast<int>(mine.size()),
                                   ctx.executor.payableAnyPower(ctx.controller));
        const int x = pickXAmount(ctx, "Chronobreak: pay any amount of Power", 0, max_x);
        if (x < 0) return;   // waiting for the agent
        // (pickXAmount has stored X at resume_data[0]; the rest of the layout
        // can be reserved now. kPaid keeps a re-entry from paying twice.)
        auto& ro = ctx.state.chain.resuming;
        if (ro.has_value() && ro->resume_data.size() < kEnd) ro->resume_data.resize(kEnd, 0);
        const bool paid = ro.has_value() && ro->resume_data[kPaid] != 0;
        if (x > 0 && !paid && costDueNow(ctx)) {
            ctx.executor.payAnyPower(ctx.controller, x);
            if (ro.has_value()) ro->resume_data[kPaid] = 1;
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here
        if (x <= 0) return;

        // "Banish that many units you control" — chosen as it resolves.
        const int n_pick = std::min(x, kMaxUnits);
        auto picked = pickSeveral(ctx, ctx.controller, "Chronobreak: banish a unit you control",
                                  mine, false, {}, std::min(n_pick, static_cast<int>(mine.size())),
                                  n_pick, kUnitsSlot);
        if (!picked) return;
        std::vector<int32_t> local;   // direct (no chain) invocation only
        auto& rd = ctx.state.chain.resuming.has_value() ? ctx.state.chain.resuming->resume_data
                                                        : local;
        if (rd.size() < kEnd) rd.resize(kEnd, 0);

        // The units in the order they were chosen; the banish happens once.
        std::vector<GameObjectId> order;
        for (int v : *picked) order.push_back(static_cast<GameObjectId>(v));

        if (rd[kPhase] == 0) {
            rd[kPhase] = 1;
            for (size_t i = 0; i < order.size(); ++i) {
                if (!ctx.state.objectExists(order[i])) { rd[kDone + i] = 1; continue; }
                auto& u = ctx.state.getObject(order[i]);
                if (!u.isUnit() || !u.location.has_value() || u.controller != ctx.controller) {
                    rd[kDone + i] = 1;   // no longer a unit you control: not banished
                    continue;
                }
                auto bf = u.battlefieldId();
                rd[kLoc + i] = bf ? static_cast<int>(*bf) : -1;
                const bool token = u.isToken();
                ctx.executor.banishObject(order[i]);
                if (token) rd[kDone + i] = 1;   // a token ceases to exist (CR 183.1)
            }
        }

        // "... then for each, its owner plays that unit back to the same
        // location or to their base, ignoring its cost. (You choose the
        // order.)" Each player plays their own units, in turn order starting
        // with the turn player (CR 303.2.a), in the order THEY choose (Radiance
        // rules FAQ): for each, its owner picks which of their units comes
        // next, then where it goes. Step k's answers live at kOrderSlot + 3k
        // and kChoiceSlot + 3k; rd[kStep] counts the units played back.
        const PlayerId first = ctx.state.turn.turn_player;
        for (PlayerId owner : {first, opponent(first)}) {
            while (true) {
                std::vector<int> mine;   // this owner's banished units still to play
                for (size_t i = 0; i < order.size(); ++i) {
                    if (rd[kDone + i]) continue;
                    const GameObjectId id = order[i];
                    if (!ctx.state.objectExists(id) ||
                        ctx.state.getObject(id).zone != ZoneType::Banishment) {
                        rd[kDone + i] = 1;
                        continue;
                    }
                    if (ctx.state.getObject(id).owner == owner) mine.push_back(static_cast<int>(id));
                }
                if (mine.empty()) break;
                const int step = rd[kStep];
                GameObjectId id = static_cast<GameObjectId>(mine.front());
                if (mine.size() > 1) {
                    auto next = pickSeveral(ctx, owner, "Chronobreak: which of your units is played back next",
                                            mine, false, {}, 1, 1, kOrderSlot + 3 * step);
                    if (!next) return;   // waiting for the owner
                    if (!next->empty()) id = static_cast<GameObjectId>((*next)[0]);
                }
                size_t i = 0;
                while (i < order.size() && order[i] != id) ++i;
                if (i == order.size()) { rd[kStep] = step + 1; continue; }
                LocationId dest{BaseLocation{owner}};
                const int bf = rd[kLoc + i];
                if (bf >= 0) {
                    auto where = pickSeveral(ctx, owner,
                                             "Chronobreak: play " + ctx.state.getObject(id).name + " back to",
                                             {0, 1}, true, {"The same battlefield", "Base"}, 1, 1,
                                             kChoiceSlot + 3 * step);
                    if (!where) return;   // waiting for the owner
                    if (!where->empty() && (*where)[0] == 0)
                        dest = LocationId{BattlefieldLocation{static_cast<BattlefieldId>(bf)}};
                }
                rd[kDone + i] = 1;
                rd[kStep] = step + 1;
                auto& bz = ctx.state.player(owner).banishment;
                bz.erase(std::remove(bz.begin(), bz.end(), id), bz.end());
                ctx.executor.playIgnoringCost(owner, id, dest);
                ctx.events.logTrace("CHRONOBREAK: " + ctx.state.getObject(id).name + " played back");
            }
        }
    }

private:
    // resume_data layout (CR 355 choices use 0..5): unit picks at kUnitsSlot,
    // then the replay bookkeeping.
    static constexpr int kMaxUnits = 32;
    static constexpr size_t kUnitsSlot = 8;                       // 8 .. 8+2+32
    static constexpr size_t kPaid = 43;                           // 1 = X paid
    static constexpr size_t kPhase = 44;                          // 1 = banished
    static constexpr size_t kLoc = 45;                            // per unit: bf id, -1 base
    static constexpr size_t kDone = kLoc + kMaxUnits;             // per unit: played back / skipped
    static constexpr size_t kChoiceSlot = kDone + kMaxUnits;      // per step: 3 ints
    static constexpr size_t kOrderSlot = kChoiceSlot + 3 * kMaxUnits;   // per step: 3 ints
    static constexpr size_t kStep = kOrderSlot + 3 * kMaxUnits;   // units played back so far
    static constexpr size_t kEnd = kStep + 1;

    static std::vector<int> friendlyUnits(const GameState& state, PlayerId player) {
        std::vector<int> out;
        for (const auto& [id, o] : state.objects)
            if (o.isUnit() && o.controller == player && o.location.has_value())
                out.push_back(static_cast<int>(id));
        std::sort(out.begin(), out.end());
        return out;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 970;
        d.def_id = R"RB(rad-140-167)RB";
        d.name = R"RB(Chronobreak)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-140/167)RB";
        d.collector_number = 140;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury,Domain::Mind};
        d.tags = {R"RB(Ekko)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.ability_text = R"RB(As an additional cost to play this, pay any amount of Power.
Banish that many units you control, then for each, its owner plays that unit back to the same location or to their base, ignoring its cost. (You choose the order.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/af2123a99f3e3e7e6c9b101c5fc2c9b0ff56e096-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_970(CardRegistry& r) {
    r.registerCard(970, std::make_unique<Chronobreak>());
}

}  // namespace riftbound
