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

// "[P], [E]: Return a friendly unit to its owner's hand. You may play a unit
// with less Might than that unit had, ignoring its cost." ([P] = Chaos power.)
// The unit to return is chosen as the ability is activated; the unit to play
// (from hand, by printed Might) as it resolves.
class Transpositionist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {ActivatedAbility{
            .cost = ActivationCost{.exhaust = true, .power = 1, .power_domain = Domain::Chaos},
            .targets = {}, .is_action = false, .is_reaction = false,
            .needs_activation_time_target = true}};
    }
    static constexpr int kPlayStage = 20;   // after pickTarget's 6/7/8

    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId me = ctx.controller;
        auto friendly = chooseableUnits(ctx.state, me,
                                        [me](const GameObject& o) { return o.controller == me; });
        GameObjectId t = pickTarget(ctx, "Transpositionist: return a friendly unit", friendly);
        if (finalizingChoices(ctx)) return;
        auto& ri_opt = ctx.state.chain.resuming;
        const bool resumable = ri_opt.has_value();
        if (!resumable || ri_opt->resume_point < kPlayStage) {
            if (t == kInvalidId) return;   // suspended or nothing to return
            if (!ctx.state.objectExists(t) || !ctx.state.getObject(t).location.has_value()) return;
            const int had = ctx.state.getObject(t).current_might;
            ctx.events.logTrace("TRANSPOSITIONIST: return " + ctx.state.getObject(t).name +
                                " (" + std::to_string(had) + " [M]) to hand");
            ctx.executor.bounceToHand(t);
            std::vector<GameObjectId> playable;
            for (auto cid : ctx.state.player(me).hand) {
                if (!ctx.state.objectExists(cid)) continue;
                const auto& c = ctx.state.getObject(cid);
                if (c.card_type != CardType::Unit || c.card_def_id == kInvalidId) continue;
                if (ctx.executor.cardDB().get(c.card_def_id).might < had) playable.push_back(cid);
            }
            if (playable.empty()) return;
            if (!resumable) { playFromHand(ctx, playable.front()); return; }   // no agent: base
            std::vector<Intent> opts;
            opts.push_back(valChoice(me, -1, "Don't play a unit"));
            for (auto cid : playable) opts.push_back(objChoice(me, cid));
            ctx.executor.requestChoice(me, std::move(opts),
                                       "Transpositionist: play a unit ignoring its cost?");
            ri_opt->resume_point = kPlayStage;
            return;
        }
        if (ri_opt->resume_point == kPlayStage) {
            ri_opt->resume_point = kPlayStage + 1;
            GameObjectId pick = takenObject(ctx);
            if (pick == kInvalidId) {
                ctx.events.logTrace("TRANSPOSITIONIST: declined");
                ri_opt->resume_point = kPlayStage + 2;
                return;
            }
            while (ri_opt->resume_data.size() < 7) ri_opt->resume_data.push_back(0);
            ri_opt->resume_data[6] = static_cast<int32_t>(pick);
        }
        if (ri_opt->resume_point == kPlayStage + 1) {
            const auto pick = static_cast<GameObjectId>(ri_opt->resume_data[6]);
            // Base or a battlefield I control (CR 355.2).
            LocationId where;
            const int r = pickPlayLocation(ctx, pick, where, PlayerId::None,
                                           "Transpositionist: play it where?");
            if (r == -1) return;   // waiting for the agent
            ri_opt->resume_point = kPlayStage + 2;
            if (r == 1) playFromHand(ctx, pick, where);
        }
    }
private:
    static void playFromHand(CardContext& ctx, GameObjectId card,
                             std::optional<LocationId> where = std::nullopt) {
        auto& hand = ctx.state.player(ctx.controller).hand;
        auto it = std::find(hand.begin(), hand.end(), card);
        if (it == hand.end()) return;
        hand.erase(it);
        ctx.events.logTrace("TRANSPOSITIONIST: play " + ctx.state.getObject(card).name +
                            " ignoring its cost");
        ctx.executor.playIgnoringCost(ctx.controller, card, where);
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1091;
        d.def_id = R"RB(rad-115-167)RB";
        d.name = R"RB(Transpositionist)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-115/167)RB";
        d.collector_number = 115;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bilgewater)RB"};   // as printed
        d.rarity = Rarity::Epic;
        d.energy_cost = 6;
        d.power_cost = 0;
        d.might = 4;
        d.ability_text = R"RB([P], [E]: Return a friendly unit to its owner's hand. You may play a unit with less Might than that unit had, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/60cf9dbb9345d44f0a9a78b60d5fb71411b50d69-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1091(CardRegistry& r) {
    r.registerCard(1091, std::make_unique<Transpositionist>());
}

}  // namespace riftbound
