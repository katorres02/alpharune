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

class ArachnoidHorror : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Hunt 2]
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 2;
    }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }

    // "Friendly units can be played to an occupied battlefield if an enemy
    //  unit is alone there." While on the board (engine play generator).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            state.player(controller).grant_friendly_units_lone_enemy_bf = true;
            return;
        }
    }

    // "I can be played to an occupied battlefield if an enemy unit is alone
    //  there." From hand: my play locations are the usual ones (base,
    //  battlefields I control, any the board-wide grants open) plus every
    //  enemy-controlled battlefield with exactly one enemy unit.
    bool restrictsPlayLocations() const override { return true; }
    std::vector<LocationId> getPlayLocations(const GameState& state, PlayerId player) const override {
        const auto& ps = state.player(player);
        std::vector<LocationId> out{BaseLocation{player}};
        if (ps.units_play_base_only) return out;
        for (const auto& bf : state.battlefields) {
            if (bf.blocks_unit_play) continue;
            const bool controlled = bf.controller.has_value() && *bf.controller == player;
            const bool open = !bf.controller.has_value();
            int enemies = 0;
            for (const auto& [oid, o] : state.objects)
                if (o.isUnit() && o.controller != player && o.battlefieldId() == bf.id) ++enemies;
            const bool enemy_held = bf.controller.has_value() && *bf.controller != player;
            if (controlled || (open && ps.grant_friendly_units_open_bf) ||
                (enemy_held && enemies == 1))
                out.push_back(BattlefieldLocation{bf.id});
        }
        return out;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 679;
        d.def_id = R"RB(unl-117-219)RB";
        d.name = R"RB(Arachnoid Horror)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-117/219)RB";
        d.collector_number = 117;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Spider)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Hunt);
        d.ability_text = R"RB([Hunt 2] (When I conquer or hold, gain 2 XP.)
I can be played to an occupied battlefield if an enemy unit is alone there.
Friendly units can be played to an occupied battlefield if an enemy unit is alone there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ae2ec829a6714ac75e8b203631af08c0c1e1565f-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_679(CardRegistry& r) {
    r.registerCard(679, std::make_unique<ArachnoidHorror>());
}

} // namespace riftbound
