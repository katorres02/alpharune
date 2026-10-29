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

class BaitedHook : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "[1][C], [E]: Kill a friendly unit. Look at the top 5 cards of your Main
    //  Deck. You may banish a unit from among them that has Might up to 1 more
    //  than the killed unit and play it, ignoring its cost. Then recycle the rest."
    // [C] = one Order power (my Domain).
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .energy = 1, .power = 1,
                              .power_domain = Domain::Order};
    }
    // Target = the friendly unit to kill.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }

    // Resumable: resume_data[8] = 1 once the unit is killed, resume_data[9] =
    // its Might, so re-entry after the agent's choices doesn't kill again.
    static constexpr size_t kKilled = 8, kMight = 9;
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ps = ctx.state.player(ctx.controller);
        int killed_might = 0;
        if (ctx.state.chain.resuming.has_value()) {
            auto& ri = *ctx.state.chain.resuming;
            while (ri.resume_data.size() <= kMight) ri.resume_data.push_back(0);
            if (ri.resume_data[kKilled] != 1) {
                if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
                ri.resume_data[kMight] = ctx.state.getObject(targets[0]).current_might;
                ri.resume_data[kKilled] = 1;
                ctx.executor.killObject(targets[0]);
            }
            killed_might = ri.resume_data[kMight];
        } else {
            if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
            killed_might = ctx.state.getObject(targets[0]).current_might;
            ctx.executor.killObject(targets[0]);
        }

        // Look at the top 5 cards of the Main Deck (top = back).
        std::vector<GameObjectId> top5;
        for (auto it = ps.main_deck.rbegin();
             it != ps.main_deck.rend() && (int)top5.size() < 5; ++it) {
            top5.push_back(*it);
        }
        // Eligible: units with Might up to killed_might + 1.
        std::vector<GameObjectId> eligible;
        for (auto cid : top5) {
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (!obj.isUnit()) continue;
            if (obj.base_might > killed_might + 1) continue;
            eligible.push_back(cid);
        }
        // "You may banish a unit ... and play it."
        int conf = confirmOptional(ctx, "Baited Hook: banish and play a unit from the top 5?",
                                   [&] { return !eligible.empty(); });
        if (conf == -1) return;   // waiting for the agent
        GameObjectId picked = kInvalidId;
        if (conf == 1) {
            picked = pickTarget(ctx, "Baited Hook (play a unit from top 5)", eligible);
            if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) {
                return;  // suspended for agent decision
            }
        }
        // Base or a battlefield I control (CR 355.2), chosen before anything moves.
        LocationId where{BaseLocation{ctx.controller}};
        if (picked != kInvalidId && ctx.state.objectExists(picked)) {
            const int r = pickPlayLocation(ctx, picked, where, PlayerId::None,
                                           "Baited Hook: play it where?");
            if (r == -1) return;   // waiting for the agent
            if (r == 0) picked = kInvalidId;
        }
        if (picked != kInvalidId && ctx.state.objectExists(picked)) {
            ps.main_deck.erase(
                std::remove(ps.main_deck.begin(), ps.main_deck.end(), picked),
                ps.main_deck.end());
            ctx.executor.banishObject(picked);
            auto& bz = ctx.state.player(ctx.state.getObject(picked).owner).banishment;
            bz.erase(std::remove(bz.begin(), bz.end(), picked), bz.end());
            ctx.executor.playIgnoringCost(ctx.controller, picked, where);
        }
        // "Then recycle the rest."
        std::vector<GameObjectId> to_recycle;
        for (auto cid : top5) {
            auto pos = std::find(ps.main_deck.begin(), ps.main_deck.end(), cid);
            if (pos != ps.main_deck.end()) {
                to_recycle.push_back(cid);
                ps.main_deck.erase(pos);
            }
        }
        if (!to_recycle.empty()) ctx.executor.recycleCards(ctx.controller, to_recycle);
        ctx.events.logTrace("BAITED HOOK: kill friendly, dig top 5, recycle rest");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 242;
        d.def_id = R"RB(ogn-242-298)RB";
        d.name = R"RB(Baited Hook)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-242/298)RB";
        d.collector_number = 242;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 3;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB([1][C], [E]: Kill a friendly unit. Look at the top 5 cards of your Main Deck. You may banish a unit from among them that has Might up to 1 more than the killed unit and play it, ignoring its cost. Then recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7b4e6a1e32878a7d5fe4e40869c83e47ea08de7c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_242(CardRegistry& r) {
    r.registerCard(242, std::make_unique<BaitedHook>());
}

} // namespace riftbound
