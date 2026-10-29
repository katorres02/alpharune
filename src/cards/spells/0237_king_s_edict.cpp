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

class KingSEdict : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Starting with the next player, each other player chooses a unit you don't
    //  control that hasn't been chosen for this spell. Kill those units."
    // In 1v1 the opponent chooses one unit the caster doesn't control (one of
    // their own, as nobody else has units) as the spell resolves; the choice
    // is published to the opponent (stepChooseObject, resume_point 20).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId opp = opponent(ctx.controller);
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (obj.controller == ctx.controller) continue;   // "a unit you don't control"
            // A unit chosen for this (enemy) spell: one that can't be chosen by
            // the caster's spells isn't a legal choice.
            if (obj.untargetable_by_enemy) continue;
            legal.push_back(id);
        }
        std::sort(legal.begin(), legal.end());
        GameObjectId victim = kInvalidId;
        if (!stepChooseObject(ctx, opp, legal, "King's Edict: choose a unit to kill",
                              /*rp=*/20, /*slot=*/10, victim))
            return;   // waiting for the opponent
        if (victim == kInvalidId || !ctx.state.objectExists(victim)) return;
        if (!ctx.state.getObject(victim).location.has_value()) return;
        ctx.events.logTrace("KING'S EDICT: " + ctx.state.getObject(victim).name +
                            " (opponent's choice) is killed");
        ctx.executor.killObject(victim);
    }
    // No play-time target: the killed unit is the OPPONENT's choice, not the
    // caster's, so we don't expose a caster-chosen target.
    bool hasLegalTargets(const GameState&, PlayerId) const override { return true; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 237;
        d.def_id = R"RB(ogn-237-298)RB";
        d.name = R"RB(King's Edict)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-237/298)RB";
        d.collector_number = 237;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Starting with the next player, each other player chooses a unit you don't control that hasn't been chosen for this spell. Kill those units.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ec1b8b98e1ca7a34939322883309c71605bb8bc5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_237(CardRegistry& r) {
    r.registerCard(237, std::make_unique<KingSEdict>());
}

} // namespace riftbound
