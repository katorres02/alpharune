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

class Defy : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Counter a spell that costs no more than [4] and no more than [A]."
    // Any such spell on the chain may be chosen, not only the top one.
    static bool eligible(const GameState& state, const CardDB& db, const ChainItem& it) {
        if (!it.is_spell || it.card_def_id == kInvalidId) return false;
        if (state.player(it.controller).spells_cant_be_countered) return false;
        const auto& d = db.get(it.card_def_id);
        return d.energy_cost <= 4 && d.power_cost <= 1;
    }
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        // No CardDB here: any spell on the chain may be a legal target; the
        // cost gate is applied as Defy resolves.
        for (const auto& it : state.chain.items)
            if (it.is_spell) return true;
        return false;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> legal;   // spell objects, top of the chain first
        for (auto it = ctx.state.chain.items.rbegin(); it != ctx.state.chain.items.rend(); ++it)
            if (eligible(ctx.state, ctx.executor.cardDB(), *it) && !isUncounterable(ctx, *it))
                legal.push_back(it->source);
        GameObjectId pick = pickTarget(ctx, "Defy: counter which spell", legal);
        if (pick == kInvalidId) return;   // suspended, or nothing to counter
        auto& items = ctx.state.chain.items;
        for (auto it = items.begin(); it != items.end(); ++it) {
            if (it->source != pick || !it->is_spell) continue;
            revertCounteredPlay(ctx, *it);  // CR 425.1.b
            items.erase(it);
            if (ctx.state.objectExists(pick)) {
                ctx.events.logTrace("COUNTER: " + ctx.state.getObject(pick).name +
                                    " countered by Defy");
                disposeCounteredCard(ctx, pick);
            }
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 45;
        d.def_id = R"RB(ogn-045-298)RB";
        d.name = R"RB(Defy)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-045/298)RB";
        d.collector_number = 45;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Counter a spell that costs no more than [4] and no more than [A].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4989bfcc4bd7be77051f0c2c349a981ba9c273e0-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_45(CardRegistry& r) {
    r.registerCard(45, std::make_unique<Defy>());
}

} // namespace riftbound
