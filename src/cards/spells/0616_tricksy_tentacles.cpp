#include "cards/card.h"
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

class TricksyTentacles : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Move any number of enemy units with the same controller and a total
    //  Might of 8 or less to a single location."
    // Resolution: choose the location (pickMode: their base, or a
    // battlefield), then pick units one at a time (each must fit the
    // remaining Might budget and not already be at that location) until the
    // player stops. In 1v1 every enemy unit has the same controller.
    // resume_points 20/21 + resume_data[6] (budget used) and [7..] (chosen ids)
    // are this card's own; pickMode uses 3/4/5 + resume_data[1].
    static constexpr int kBudget = 8;
    static constexpr int kPick = 20;
    static constexpr int kPicking = 21;

    static bool atDestination(const GameObject& o, int dest_mode) {
        if (dest_mode == 0) return o.isAtBase();
        auto bf = o.battlefieldId();
        return bf && static_cast<int>(*bf) == dest_mode - 1;
    }
    std::vector<GameObjectId> candidates(CardContext& ctx, int dest_mode,
                                         const std::vector<GameObjectId>& chosen,
                                         int used) const {
        std::vector<GameObjectId> out;
        const PlayerId opp = opponent(ctx.controller);
        const int payable = ctx.executor.payableAnyPower(ctx.controller);
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp || !obj.location.has_value()) continue;
            if (obj.untargetable_by_enemy) continue;
            if (atDestination(obj, dest_mode)) continue;
            if (std::find(chosen.begin(), chosen.end(), id) != chosen.end()) continue;
            if (obj.current_might > kBudget - used) continue;
            if (ctx.executor.deflectCostFor(ctx.controller, id) > payable) continue;
            out.push_back(id);
        }
        return out;
    }
    void publish(CardContext& ctx, const std::vector<GameObjectId>& cands) const {
        std::vector<Intent> options;
        for (auto id : cands) {
            Intent i;
            i.type = IntentType::MakeChoice;
            i.player = ctx.controller;
            i.chosen_objects = {id};
            options.push_back(std::move(i));
        }
        Intent done;   // empty chosen_objects = stop choosing
        done.type = IntentType::MakeChoice;
        done.player = ctx.controller;
        done.choice_label = "Done";
        options.push_back(std::move(done));
        ctx.executor.requestChoice(ctx.controller, std::move(options),
                                    "Tricksy Tentacles: choose an enemy unit to move, or stop");
    }
    void moveAll(CardContext& ctx, int dest_mode, const std::vector<GameObjectId>& chosen) const {
        for (auto id : chosen) {
            if (!ctx.state.objectExists(id)) continue;
            const auto& o = ctx.state.getObject(id);
            if (!o.location.has_value() || atDestination(o, dest_mode)) continue;
            if (dest_mode == 0) ctx.executor.moveToBase(id);
            else ctx.executor.moveToBattlefield(id, static_cast<BattlefieldId>(dest_mode - 1));
        }
        ctx.events.logTrace("TRICKSY TENTACLES: moved " + std::to_string(chosen.size()) +
                            " enemy unit(s) to one location");
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Destination: 0 = their base, i+1 = battlefield i.
        const int num_modes = 1 + static_cast<int>(ctx.state.battlefields.size());
        std::vector<std::string> labels{"Their base"};
        for (const auto& bf : ctx.state.battlefields)
            labels.push_back("Battlefield " + std::to_string(bf.id));
        uint32_t legal_modes = 1u;
        for (int i = 1; i < num_modes && i < 32; ++i) {
            const auto& bf = ctx.state.battlefields[i - 1];
            if (static_cast<int>(bf.id) == i - 1) legal_modes |= (1u << i);
        }
        int dest = pickMode(ctx, "Tricksy Tentacles: move to which location?",
                            num_modes, labels, legal_modes);
        if (dest == -1) return;     // waiting for agent
        if (dest < 0) return;

        auto& ri_opt = ctx.state.chain.resuming;
        if (!ri_opt.has_value()) {
            // Direct invocation (no chain): take units greedily, largest
            // Might that fits first.
            std::vector<GameObjectId> chosen;
            int used = 0;
            while (true) {
                auto c = candidates(ctx, dest, chosen, used);
                if (c.empty()) break;
                chosen.push_back(c.front());
                used += ctx.state.getObject(c.front()).current_might;
            }
            moveAll(ctx, dest, chosen);
            return;
        }
        auto& ri = *ri_opt;
        while (ri.resume_data.size() < 7) ri.resume_data.push_back(0);
        auto chosen_list = [&ri]() {
            return std::vector<GameObjectId>(ri.resume_data.begin() + 7, ri.resume_data.end());
        };
        if (ri.resume_point < kPick) {
            ri.resume_point = kPick;
            ri.resume_data[6] = 0;
        }
        if (ri.resume_point == kPicking) {
            auto pick = ctx.executor.takeChoice();
            auto chosen = chosen_list();
            if (pick.has_value() && !pick->chosen_objects.empty()) {
                GameObjectId id = pick->chosen_objects.front();
                auto c = candidates(ctx, dest, chosen, ri.resume_data[6]);
                if (std::find(c.begin(), c.end(), id) != c.end()) {
                    const int deflect = ctx.executor.deflectCostFor(ctx.controller, id);
                    if (deflect > 0) ctx.executor.payAnyPower(ctx.controller, deflect);
                    ri.resume_data[6] += ctx.state.getObject(id).current_might;
                    ri.resume_data.push_back(static_cast<int32_t>(id));
                    ri.chosen_objects.push_back(id);
                    ri.resume_point = kPick;   // offer another
                }
            } else {
                moveAll(ctx, dest, chosen);   // done
                ri.resume_point = kPicking + 1;
                return;
            }
        }
        if (ri.resume_point == kPick) {
            auto chosen = chosen_list();
            auto c = candidates(ctx, dest, chosen, ri.resume_data[6]);
            if (c.empty()) {
                moveAll(ctx, dest, chosen);
                ri.resume_point = kPicking + 1;
                return;
            }
            publish(ctx, c);
            ri.resume_point = kPicking;
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 616;
        d.def_id = R"RB(unl-054-219)RB";
        d.name = R"RB(Tricksy Tentacles)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-054/219)RB";
        d.collector_number = 54;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Move any number of enemy units with the same controller and a total Might of 8 or less to a single location.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7e1e77df44c076599fa780f7fd01a4550f67af45-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_616(CardRegistry& r) {
    r.registerCard(616, std::make_unique<TricksyTentacles>());
}

} // namespace riftbound
