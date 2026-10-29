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

class KaiSaEvolutionary : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    // "When I conquer, you may play a spell from your trash with Energy cost
    // less than your points without paying its Energy cost. Then recycle it.
    // (You must still pay its Power cost.)" The "may" and the spell are chosen
    // at finalization; the spell is played through EffectExecutor::
    // playPayingCost with its Energy ignored, so its Power is paid (CR
    // 356.1.b.2). If that Power can't be paid it isn't played and stays in the
    // trash. A spell with targets chooses them as it is finalized.
    std::vector<GameObjectId> eligibleSpells(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        const auto& ps = ctx.state.player(ctx.controller);
        const auto& db = ctx.executor.cardDB();
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& obj = ctx.state.getObject(cid);
            if (obj.card_type != CardType::Spell || obj.card_def_id == kInvalidId) continue;
            // "Energy cost less than your points."
            if (db.get(obj.card_def_id).energy_cost >= ps.score) continue;
            if (!ctx.executor.canPlayPayingCost(ctx.controller, cid, kPowerOnly)) continue;
            out.push_back(cid);
        }
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        int conf = confirmOptional(ctx,
            "Kai'Sa: play a spell (Energy cost < points) from trash without Energy cost?",
            [&]() { return !eligibleSpells(ctx).empty(); });
        if (conf == -1) return;
        if (conf != 1) { finalizingChoices(ctx); return; }
        GameObjectId chosen = pickTarget(ctx, "Kai'Sa (spell from trash)", eligibleSpells(ctx));
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (chosen == kInvalidId || !ctx.state.objectExists(chosen)) return;
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.trash.begin(), ps.trash.end(), chosen);
        if (it == ps.trash.end()) return;
        ps.trash.erase(it);

        std::string sname = ctx.state.getObject(chosen).name;
        // "Then recycle it" replaces where it goes when it leaves the chain.
        ctx.state.getObject(chosen).card_counters["__recycle_on_leave_chain"] = 1;
        if (!ctx.executor.playPayingCost(ctx.controller, chosen, std::nullopt, kPowerOnly)) {
            // Its Power wasn't paid: not played, it stays in the trash.
            ctx.state.getObject(chosen).card_counters["__recycle_on_leave_chain"] = 0;
            ps.trash.push_back(chosen);
            return;
        }
        // With no chain to resolve on (an executor driven directly, outside a
        // game), the spell can't leave the chain later: recycle it now.
        if (ctx.state.objectExists(chosen) &&
            ctx.state.getObject(chosen).zone != ZoneType::Chain) {
            ctx.state.getObject(chosen).card_counters["__recycle_on_leave_chain"] = 0;
            ctx.executor.recycleCards(ctx.controller, {chosen});
        }
        ctx.events.logTrace("KAI'SA: conquer -> played " + sname +
                             " from trash (ignoring Energy), then recycled it");
    }
private:
    static constexpr EffectExecutor::EffectPlayCost kPowerOnly{.ignore_energy = true};
    const CardDef def_ = [] {
        CardDef d;
        d.id = 112;
        d.def_id = R"RB(ogn-112-298)RB";
        d.name = R"RB(Kai'Sa, Evolutionary)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-112/298)RB";
        d.collector_number = 112;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Kai'Sa)RB", R"RB(The Void)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Ganking] (I can move from battlefield to battlefield.)
When I conquer, you may play a spell from your trash with Energy cost less than your points without paying its Energy cost. Then recycle it. (You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fc4ee60eaedd5c56a9222fd07482b6a86b11baa4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_112(CardRegistry& r) {
    r.registerCard(112, std::make_unique<KaiSaEvolutionary>());
}

} // namespace riftbound
