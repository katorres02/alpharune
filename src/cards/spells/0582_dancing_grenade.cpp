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

class DancingGrenade : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Deal 2 to a unit. Its controller may play this spell again for [A]. If
    //  they do, this deals 1 additional Bonus Damage for each time this spell
    //  has dealt damage this turn."
    // The unit's controller (either player) decides as this resolves. Yes
    // gives them an immediate TrashReplayGrant for [A]: as the spell lands in
    // its owner's trash the engine pays it and plays the spell again
    // (GameEngine::replayFromTrashNow), under their control, choosing a new
    // target as it is finalized. The damage count lives on the card object,
    // stamped with the turn ("this turn"); only a replay this way adds it.
    // resume_point 0 = deal damage (and ask), kAwaitReplay = take the answer;
    // resume_data[0] = the deciding player.
    static constexpr int kAwaitReplay = 30;
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ri = ctx.state.chain.resuming;
        if (ri.has_value() && ri->resume_point == kAwaitReplay) {
            ri->resume_point = kAwaitReplay + 1;
            if (ri->resume_data.empty()) return;
            const auto who = static_cast<PlayerId>(ri->resume_data[0]);
            if (takenValue(ctx, 0) != 1) return;
            grantReplay(ctx, who);
            return;
        }
        if (ri.has_value() && ri->resume_point != 0) return;
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& src = ctx.state.getObject(ctx.source);
        const int turn = ctx.state.turn.turn_number;
        if (src.card_counters["__dancing_grenade_turn"] != turn) {
            src.card_counters["__dancing_grenade_turn"] = turn;
            src.card_counters["__dancing_grenade_hits"] = 0;   // "this turn"
        }
        // "If they do": this play is the replay granted when it last resolved.
        const bool replayed = src.card_counters["__dancing_grenade_replay_turn"] == turn &&
                              src.card_counters["__played_not_from_hand"] == 1;
        src.card_counters["__dancing_grenade_replay_turn"] = 0;
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        const GameObjectId unit = targets[0];
        const PlayerId its_controller = ctx.state.getObject(unit).controller;
        const int bonus = replayed ? src.card_counters["__dancing_grenade_hits"] : 0;
        const int dmg = 2 + bonus;
        src.card_counters["__dancing_grenade_hits"] += 1;
        ctx.executor.dealDamage(unit, dmg, ctx.source);
        ctx.events.logTrace("DANCING GRENADE: deal " + std::to_string(dmg));
        if (ctx.state.objectExists(unit) && ctx.state.getObject(unit).hasLethalDamage())
            ctx.executor.killObject(unit);

        // "Its controller may play this spell again for [A]."
        if (!ri.has_value() || ctx.executor.payableAnyPower(its_controller) < 1) return;
        ri->resume_data = {static_cast<int32_t>(its_controller)};
        ri->resume_point = kAwaitReplay;
        ctx.executor.requestChoice(its_controller,
            {valChoice(its_controller, 0, "Don't play Dancing Grenade again"),
             valChoice(its_controller, 1, "Play Dancing Grenade again for [A]")},
            "Dancing Grenade: play this spell again for [A]?");
    }
    static void grantReplay(CardContext& ctx, PlayerId who) {
        if (!ctx.state.objectExists(ctx.source)) return;
        PlayerState::TrashReplayGrant g;
        g.card = ctx.source;
        g.power = 1;
        g.any_domain = true;   // [A]
        g.immediate = true;
        ctx.state.player(who).trash_replay_grants.push_back(g);
        ctx.state.getObject(ctx.source).card_counters["__dancing_grenade_replay_turn"] =
            ctx.state.turn.turn_number;
        ctx.events.logTrace("DANCING GRENADE: " + std::string(toString(who)) +
                            " plays it again for [A]");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 582;
        d.def_id = R"RB(unl-020-219)RB";
        d.name = R"RB(Dancing Grenade)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-020/219)RB";
        d.collector_number = 20;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Deal 2 to a unit. Its controller may play this spell again for [A]. If they do, this deals 1 additional Bonus Damage for each time this spell has dealt damage this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8afc30b1bc7cc1841dfe11cfa29adbcc02257845-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_582(CardRegistry& r) {
    r.registerCard(582, std::make_unique<DancingGrenade>());
}

} // namespace riftbound
