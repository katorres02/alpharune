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

// "You may play a unit from hand to a battlefield you control, reducing its
//  cost by [3]." ([Hidden] / [Action] are engine-handled timing.)
// Played through EffectExecutor::playPayingCost with a [3] Energy reduction:
// the rest of its cost (Energy above [3] and its Power) is paid; a unit whose
// remaining cost can't be paid isn't offered, and one that can't be paid by
// then goes back to hand.

class HereToHelp : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // resume_point 1 = consume the unit pick (or "Don't"), 2 = consume the
    // battlefield pick; resume_data[0] = the chosen unit.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        const EffectExecutor::EffectPlayCost reduced{.energy_reduction = 3};
        auto controlled_bfs = [&]() {
            std::vector<int> out;
            for (const auto& bf : ctx.state.battlefields)
                if (bf.controller && *bf.controller == ctx.controller)
                    out.push_back(1 + static_cast<int>(bf.id));
            return out;
        };
        auto units = [&]() {
            std::vector<GameObjectId> out;
            for (auto cid : ps.hand) {
                if (!ctx.state.objectExists(cid) || !ctx.state.getObject(cid).isUnit()) continue;
                if (!ctx.executor.canPlayPayingCost(ctx.controller, cid, reduced)) continue;
                out.push_back(cid);
            }
            return out;
        };
        auto play = [&](GameObjectId unit, int dest) {
            auto it = std::find(ps.hand.begin(), ps.hand.end(), unit);
            if (it == ps.hand.end()) return;
            ps.hand.erase(it);
            LocationId loc{BattlefieldLocation{static_cast<BattlefieldId>(dest - 1)}};
            if (!ctx.executor.playPayingCost(ctx.controller, unit, loc, reduced)) {
                ps.hand.push_back(unit);
                return;
            }
            ctx.events.logTrace("HERE TO HELP: played a unit to a controlled battlefield (-[3])");
        };
        switch (ri.resume_point) {
        case 0: {
            // "to a battlefield you control": none controlled -> nothing to do.
            if (controlled_bfs().empty()) return;
            auto u = units();
            if (u.empty()) return;
            std::vector<Intent> opts;
            for (auto id : u) opts.push_back(objChoice(ctx.controller, id));
            opts.push_back(valChoice(ctx.controller, 0, "Don't play a unit"));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Here to Help: you may play a unit from hand");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            ri.resume_point = 3;
            GameObjectId unit = takenObject(ctx);
            auto u = units();
            if (std::find(u.begin(), u.end(), unit) == u.end()) return;   // declined
            auto bfs = controlled_bfs();
            if (bfs.empty()) return;
            if (bfs.size() == 1) { play(unit, bfs.front()); return; }
            ri.resume_data = {static_cast<int32_t>(unit)};
            std::vector<Intent> opts;
            for (int d : bfs) opts.push_back(valChoice(ctx.controller, d, destLabel(d)));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Here to Help: battlefield to play it to");
            ri.resume_point = 2;
            return;
        }
        case 2: {
            ri.resume_point = 3;
            auto bfs = controlled_bfs();
            if (bfs.empty() || ri.resume_data.empty()) return;
            int d = takenValue(ctx, bfs.front());
            if (std::find(bfs.begin(), bfs.end(), d) == bfs.end()) d = bfs.front();
            play(static_cast<GameObjectId>(ri.resume_data[0]), d);
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 433;
        d.def_id = R"RB(sfd-111-221)RB";
        d.name = R"RB(Here to Help)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-111/221)RB";
        d.collector_number = 111;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
You may play a unit from hand to a battlefield you control, reducing its cost by [3].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6daa8caf27f76f2b0bce189fb9df196129582afa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_433(CardRegistry& r) {
    r.registerCard(433, std::make_unique<HereToHelp>());
}

} // namespace riftbound
