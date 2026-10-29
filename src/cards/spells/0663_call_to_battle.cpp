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

class CallToBattle : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Move a unit you control to a battlefield you control. Then, choose an
    //  opponent. They move a unit they control to the same battlefield."
    // The unit (a target) and its destination are chosen as the spell is
    // played (CR 355.4-355.5); the opponent picks their unit as it resolves,
    // through their own decision (stepChooseObject).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool needsPlayTimeTargetPair() const override { return true; }   // one Play intent
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !movableUnits(state, controller).empty();
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId unit = pickTarget(ctx, "Call to Battle: move a unit you control",
                                             movableUnits(ctx.state, ctx.controller));
        if (ctx.executor.hasPendingChoice()) return;   // waiting for the agent
        // CR 355.4: the destination, a battlefield you control other than
        // where the unit is (355.4.a).
        std::vector<std::pair<int, std::string>> dests;
        if (unit != kInvalidId && ctx.state.objectExists(unit))
            for (int bf : destinations(ctx.state, ctx.controller, ctx.state.getObject(unit)))
                dests.emplace_back(bf, bfName(ctx.state, bf));
        int dest = -1;
        if (!stepChooseValue(ctx, ctx.controller, dests, "Call to Battle: to which battlefield?",
                             /*rp=*/20, /*slot=*/10, /*fallback=*/-1, dest))
            return;   // waiting for the agent
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (unit == kInvalidId || dest < 0 || !ctx.state.objectExists(unit)) return;
        const auto to = static_cast<BattlefieldId>(dest);
        // The move happens once, before the opponent's choice: re-entries
        // while the opponent decides find it done (resume_data[12]).
        auto& ro = ctx.state.chain.resuming;
        if (ro.has_value() && ro->resume_data.size() < 13) ro->resume_data.resize(13, 0);
        if (!ro.has_value() || ro->resume_data[12] == 0) {
            auto& u = ctx.state.getObject(unit);
            auto here = u.battlefieldId();
            if (!u.location.has_value() || (here && *here == to)) return;   // can't move there
            if (ro.has_value()) ro->resume_data[12] = 1;
            ctx.executor.moveToBattlefield(unit, to);
            ctx.events.logTrace("CALL TO BATTLE: moved a friendly unit to battlefield #" +
                                std::to_string(dest));
        }

        // "Then, choose an opponent. They move a unit they control to the same
        //  battlefield." (1v1: the opponent.) Their choice, among their units
        //  not already there.
        const PlayerId opp = opponent(ctx.controller);
        std::vector<GameObjectId> theirs;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp || !obj.location.has_value()) continue;
            auto obf = obj.battlefieldId();
            if (obf && *obf == to) continue;   // already there
            theirs.push_back(id);
        }
        std::sort(theirs.begin(), theirs.end());
        GameObjectId opp_unit = kInvalidId;
        if (!stepChooseObject(ctx, opp, theirs, "Call to Battle: move one of your units to battlefield #" +
                                  std::to_string(dest), /*rp=*/24, /*slot=*/11, opp_unit))
            return;   // waiting for the opponent
        if (opp_unit == kInvalidId || !ctx.state.objectExists(opp_unit)) return;
        if (!ctx.state.getObject(opp_unit).location.has_value()) return;
        ctx.executor.moveToBattlefield(opp_unit, to);
        ctx.events.logTrace("CALL TO BATTLE: opponent moved " +
                            ctx.state.getObject(opp_unit).name + " to the same battlefield");
    }

private:
    static std::vector<int> destinations(const GameState& state, PlayerId me, const GameObject& u) {
        std::vector<int> out;
        auto here = u.battlefieldId();
        for (const auto& bf : state.battlefields) {
            if (!bf.controller || *bf.controller != me) continue;
            if (here && *here == bf.id) continue;
            out.push_back(static_cast<int>(bf.id));
        }
        return out;
    }
    static std::vector<GameObjectId> movableUnits(const GameState& state, PlayerId me) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != me || !obj.location.has_value()) continue;
            if (destinations(state, me, obj).empty()) continue;
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    static std::string bfName(const GameState& state, int bf) {
        for (const auto& b : state.battlefields)
            if (static_cast<int>(b.id) == bf && state.objectExists(b.card_object_id))
                return state.getObject(b.card_object_id).name;
        return "Battlefield #" + std::to_string(bf);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 663;
        d.def_id = R"RB(unl-101-219)RB";
        d.name = R"RB(Call to Battle)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-101/219)RB";
        d.collector_number = 101;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Move a unit you control to a battlefield you control. Then, choose an opponent. They move a unit they control to the same battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c7e0f93c72654f786f5df37895c6e95fa9d4d60-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_663(CardRegistry& r) {
    r.registerCard(663, std::make_unique<CallToBattle>());
}

} // namespace riftbound
