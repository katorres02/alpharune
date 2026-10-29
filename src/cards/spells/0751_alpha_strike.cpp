#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include "core/intent.h"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace riftbound {
namespace {

class AlphaStrike : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a friendly unit. It deals damage equal to its Might split among
    //  enemy units at battlefields." The friendly unit and the split's targets
    // (up to its Might, CR 355.14.c) are chosen as the spell is played; the
    // division is decided as it resolves (splitDamageTargets /
    // splitDamageResolve, card_helpers.h).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    bool needsPlayTimeTargetPair() const override { return true; }   // one Play intent
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !friendlyUnits(state, controller).empty();
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId src = pickTarget(ctx, "Alpha Strike: choose a friendly unit",
                                            friendlyUnits(ctx.state, ctx.controller));
        if (ctx.executor.hasPendingChoice()) return;   // waiting for the agent
        const auto legal = enemyUnits(ctx);
        if (src != kInvalidId && ctx.state.objectExists(src)) {
            // "Up to the initial amount of damage": its Might as it's played.
            const int initial = ctx.state.getObject(src).current_might;
            if (!splitDamageTargets(ctx, "Alpha Strike", initial, /*min_targets=*/1, legal))
                return;   // waiting for the agent
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (src == kInvalidId || !ctx.state.objectExists(src)) return;   // mistarget

        // "It deals damage equal to its Might": read as the spell resolves.
        const int total = ctx.state.getObject(src).current_might;
        std::vector<GameObjectId> damaged;
        if (!splitDamageResolve(ctx, "Alpha Strike", total, src, legal, damaged)) return;

        // Kill lethally-damaged units (after all damage is marked); gain 1 XP
        // per unit this kills.
        int kills = 0;
        for (auto id : damaged) {
            if (!ctx.state.objectExists(id)) continue;
            if (ctx.state.getObject(id).hasLethalDamage()) {
                ctx.executor.killObject(id);
                ++kills;
            }
        }
        if (kills > 0) {
            ctx.state.player(ctx.controller).xp += kills;
            ctx.events.logTrace("ALPHA STRIKE: " + std::to_string(kills) +
                                " kill(s) -> gain " + std::to_string(kills) +
                                " XP (now " +
                                std::to_string(ctx.state.player(ctx.controller).xp) + ")");
        }
    }

private:
    static std::vector<GameObjectId> friendlyUnits(const GameState& state, PlayerId me) {
        std::vector<GameObjectId> out;
        for (const auto& [id, o] : state.objects)
            if (o.isUnit() && o.controller == me && o.location.has_value()) out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    // "enemy units at battlefields"; units that can't be chosen by enemy
    // spells and abilities are not legal.
    static std::vector<GameObjectId> enemyUnits(CardContext& ctx) {
        auto out = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
            return o.controller != ctx.controller && o.isAtBattlefield();
        });
        std::sort(out.begin(), out.end());
        return out;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 751;
        d.def_id = R"RB(unl-192-219)RB";
        d.name = R"RB(Alpha Strike)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-192/219)RB";
        d.collector_number = 192;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(Master Yi)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose a friendly unit. It deals damage equal to its Might split among enemy units at battlefields. Then for each unit this kills, do this: Gain 1 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/154940caa774fd8219625cf051ddcb230ffc7f02-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_751(CardRegistry& r) {
    r.registerCard(751, std::make_unique<AlphaStrike>());
}

} // namespace riftbound
