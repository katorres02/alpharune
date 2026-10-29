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

// "[Deflect 2] ... When I conquer after an attack, if you assigned 5 or more
//  excess damage to enemy units, you may deal that much to an enemy unit."

class SivirAmbitious : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }

    // Excess damage assigned in the attack at the battlefield I conquered.
    static int excessHere(const GameState& state, GameObjectId self, PlayerId controller,
                          GameObjectId subject) {
        const auto& ps = state.player(controller);
        if (!ps.last_combat_attacker) return 0;   // "after an attack"
        std::optional<BattlefieldId> bf;
        if (state.objectExists(self)) bf = state.getObject(self).battlefieldId();
        if (!bf) bf = battlefieldOfCard(state, subject);
        return bf ? excessCombatDamageAt(state, controller, *bf) : 0;
    }
    // "if you assigned 5 or more excess damage to enemy units"
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId subject) const override {
        return excessHere(state, source, controller, subject) >= 5;
    }
    // "you may deal that much to an enemy unit." The "may", the amount and
    // the enemy unit are fixed at finalization (resume_data[6] = amount).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int amount = 0;
        if (ctx.state.chain.resuming.has_value()) {
            auto& ri = *ctx.state.chain.resuming;
            while (ri.resume_data.size() < 7) ri.resume_data.push_back(0);
            if (ri.resume_data[6] == 0)
                ri.resume_data[6] = excessHere(ctx.state, ctx.source, ctx.controller,
                                               ri.triggering_subject);
            amount = ri.resume_data[6];
        } else {
            amount = excessHere(ctx.state, ctx.source, ctx.controller, kInvalidId);
        }
        if (amount < 5) { finalizingChoices(ctx); return; }
        const PlayerId me = ctx.controller;
        auto enemies = [&]() {
            return chooseableUnits(ctx.state, me,
                                   [&](const GameObject& u) { return u.controller != me; });
        };
        int conf = confirmOptional(ctx, "Sivir: deal " + std::to_string(amount) +
                                        " to an enemy unit?",
                                   [&]() { return !enemies().empty(); });
        if (conf == -1) return;
        if (conf == 0) { finalizingChoices(ctx); return; }
        GameObjectId t = pickTarget(ctx, "Sivir: deal excess damage to an enemy unit", enemies());
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        ctx.executor.dealDamage(t, amount, ctx.source);
        if (ctx.state.objectExists(t) && ctx.state.getObject(t).hasLethalDamage())
            ctx.executor.killObject(t);
        ctx.events.logTrace("SIVIR AMBITIOUS: dealt " + std::to_string(amount) +
                            " excess damage to an enemy unit");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 441;
        d.def_id = R"RB(sfd-120-221)RB";
        d.name = R"RB(Sivir, Ambitious)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-120/221)RB";
        d.collector_number = 120;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Sivir)RB", R"RB(Shurima)RB"};
        d.energy_cost = 6;
        d.power_cost = 3;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.deflect_value = 2;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect 2] (Opponents must pay [A][A] to choose me with a spell or Ability.)
When I conquer after an attack, if you assigned 5 or more excess damage to enemy units, you may deal that much to an enemy unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bd3f242c9ca59cfc2d7dc5b146877c66bb310bb7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_441(CardRegistry& r) {
    r.registerCard(441, std::make_unique<SivirAmbitious>());
}

} // namespace riftbound
