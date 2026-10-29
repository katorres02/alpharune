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

class RumbleHotheaded : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // ── Part 1: "Your Mechs each have [Assault]." ── (I'm a Mech, so I get it
    // here too; it is not printed on me.)
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject&) {
            for (auto& [uid, u] : state.objects) {
                if (!u.isUnit() || u.controller != controller || !u.location.has_value()) continue;
                if (!hasTag(u, "Mech")) continue;
                auraKeyword(u, sid, Keyword::Assault, 1);
            }
        });
    }

    // ── Part 2: "When I conquer, you may recycle another friendly unit to play
    // a Mech from your trash. Reduce its Energy cost by the Might of the unit
    // you recycled." ──
    // At finalization (CR 383.3): the "you may", which unit to recycle (only
    // units whose Might makes some Mech in trash affordable are offered) and
    // the recycle itself (the cost). At resolution: which Mech to play.
    // resume_data[6] = recycled unit's Might, [7] = cost paid flag, [8] = the
    // Mech; resume_point 20 = consume the Mech pick, 21 = where to play it.
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;   // needs the resume slot
        auto& ri = *ctx.state.chain.resuming;
        while (ri.resume_data.size() < 9) ri.resume_data.push_back(0);

        auto playable_mechs = [&](int reduction) {
            std::vector<GameObjectId> out;
            const EffectExecutor::EffectPlayCost cost{.energy_reduction = reduction};
            for (auto id : ctx.state.player(ctx.controller).trash) {
                if (!ctx.state.objectExists(id)) continue;
                const auto& o = ctx.state.getObject(id);
                if (!o.isUnit() || !hasTag(o, "Mech")) continue;
                if (ctx.executor.canPlayPayingCost(ctx.controller, id, cost)) out.push_back(id);
            }
            return out;
        };
        auto recyclables = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source || !obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (!playable_mechs(obj.current_might).empty()) out.push_back(id);
            }
            return out;
        };

        int conf = confirmOptional(ctx,
            "Rumble: recycle another friendly unit to play a Mech from your trash?",
            [&]() { return ri.resume_data[7] == 1 || !recyclables().empty(); });
        if (conf == -1) return;   // waiting on agent
        if (conf == 0) { finalizingChoices(ctx); return; }

        if (ri.resume_data[7] == 0) {
            GameObjectId unit = pickTarget(ctx, "Rumble: recycle a friendly unit", recyclables());
            if (ctx.executor.hasPendingChoice()) return;
            if (costDueNow(ctx)) {
                if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
                ri.resume_data[6] = ctx.state.getObject(unit).current_might;
                ri.resume_data[7] = 1;
                ctx.executor.recycleCards(ctx.controller, {unit});   // the cost
            }
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (ri.resume_data[7] != 1) return;

        const int reduction = ri.resume_data[6];
        auto mechs = playable_mechs(reduction);
        GameObjectId mech = kInvalidId;
        if (ri.resume_point == 20) {
            mech = takenObject(ctx);
            if (std::find(mechs.begin(), mechs.end(), mech) == mechs.end())
                mech = mechs.empty() ? kInvalidId : mechs.front();
            ri.resume_point = 21;
            ri.resume_data[8] = static_cast<int32_t>(mech);
        } else if (ri.resume_point < 20) {
            if (mechs.empty()) { ri.resume_point = 22; return; }
            std::vector<Intent> opts;
            for (auto id : mechs) opts.push_back(objChoice(ctx.controller, id));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Rumble: play a Mech from your trash");
            ri.resume_point = 20;
            return;
        } else if (ri.resume_point == 21) {
            mech = static_cast<GameObjectId>(ri.resume_data[8]);
        }
        if (mech == kInvalidId || ri.resume_point != 21) return;
        // Base or a battlefield I control (CR 355.2).
        LocationId where;
        const int r = pickPlayLocation(ctx, mech, where, PlayerId::None, "Rumble: play the Mech where?");
        if (r == -1) return;   // waiting for the agent
        ri.resume_point = 22;
        if (r == 0) return;
        const EffectExecutor::EffectPlayCost reduced{.energy_reduction = reduction};
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.trash.begin(), ps.trash.end(), mech);
        if (it != ps.trash.end()) ps.trash.erase(it);
        if (!ctx.executor.playPayingCost(ctx.controller, mech, where, reduced)) {
            ps.trash.push_back(mech);   // couldn't pay after all: it stays in trash
            return;
        }
        ctx.events.logTrace("RUMBLE: conquer -> recycled a unit, played a Mech from trash");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 349;
        d.def_id = R"RB(sfd-026-221)RB";
        d.name = R"RB(Rumble, Hotheaded)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-026/221)RB";
        d.collector_number = 26;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Mech)RB", R"RB(Yordle)RB", R"RB(Rumble)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Your Mechs each have [Assault]. (+1 [M] while we're attackers.)
When I conquer, you may recycle another friendly unit to play a Mech from your trash. Reduce its Energy cost by the Might of the unit you recycled.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e211311ecb1475e87d3d94eba4a6774b4e900091-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_349(CardRegistry& r) {
    r.registerCard(349, std::make_unique<RumbleHotheaded>());
}

} // namespace riftbound
