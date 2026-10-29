#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "cards/gear/deploy_base.h"
#include "cards/gear/equip_base.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class PrideOfNazumah : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "[1], [E]: Give a friendly unit "I have [Assault] equal to my [Shield]"
    // this turn." The grant is a turn-stamped mark on the unit; this legend's
    // aura gives each marked unit [Assault N] for its current Shield N.
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility a;
        a.cost.exhaust = true;
        a.cost.energy = 1;
        a.targets = TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
        a.needs_activation_time_target = true;
        return {a};
    }
    std::vector<GameObjectId> enumerateLegalTargets(const GameState& state,
                                                    PlayerId controller) const override {
        return chooseableUnits(state, controller, [controller](const GameObject& o) {
            return o.controller == controller;
        });
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId u = !targets.empty() ? targets[0]
            : pickTarget(ctx, "Pride of Nazumah: a friendly unit",
                         enumerateLegalTargets(ctx.state, ctx.controller));
        if (u == kInvalidId || !ctx.state.objectExists(u)) return;
        ctx.state.getObject(u).card_counters[kMark] = ctx.state.turn.turn_number + 1;
        ctx.events.logTrace("PRIDE OF NAZUMAH: " + ctx.state.getObject(u).name +
                            " has [Assault] equal to its [Shield] this turn");
    }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject&) {
            const int stamp = state.turn.turn_number + 1;
            for (auto& [id, u] : state.objects) {
                if (!u.isUnit() || !u.location.has_value()) continue;
                auto it = u.card_counters.find(kMark);
                if (it == u.card_counters.end() || it->second != stamp) continue;
                const int shield = shieldOf(u);
                if (shield > 0) auraKeyword(u, sid, Keyword::Assault, shield);
            }
        });
    }
private:
    static constexpr const char* kMark = "__nazumah_assault_turn";   // turn + 1
    // [Shield] instances: printed + "this turn" (shield_value) plus continuous grants.
    static int shieldOf(const GameObject& u) {
        int n = u.shield_value;
        for (const auto& ae : u.aura_effects)
            if (ae.keyword == Keyword::Shield) n += ae.keyword_value > 0 ? ae.keyword_value : 1;
        return n;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 977;
        d.def_id = R"RB(rad-147-167)RB";
        d.name = R"RB(Pride of Nazumah)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-147/167)RB";
        d.collector_number = 147;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm,Domain::Body};
        d.tags = {R"RB(K'Sante)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([1], [E]: Give a friendly unit "I have [Assault] equal to my [Shield]" this turn. (+1 [M] for each instance of Assault while it's an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/190a27bf844fa21105824efc19de4e71c3137df6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_977(CardRegistry& r) {
    r.registerCard(977, std::make_unique<PrideOfNazumah>());
}

}  // namespace riftbound
