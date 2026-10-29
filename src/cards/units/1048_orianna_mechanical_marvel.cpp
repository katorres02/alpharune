#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/gear/equip_base.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace riftbound {
namespace {

class OriannaMechanicalMarvel : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Once each turn, while I'm at a battlefield, you may pay [1][A] less to
    // play a gear." A from_aura cost modifier offered while I'm at a battlefield
    // and unused this turn; it counts as used once a gear is played while it is
    // offered (gears_played_this_turn rises past the value seen when offered).
    // APPROX: the discount is always taken on the first such gear (paying less
    // is never worse); the player can't save it for a later gear this turn.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject& self) {
            auto& c = self.card_counters;
            const int turn = state.turn.turn_number;
            const int gears = state.player(controller).gears_played_this_turn;
            auto t = c.find("__ori_turn");
            if (t == c.end() || t->second != turn) {
                c["__ori_turn"] = turn;
                c["__ori_used"] = 0;
                c["__ori_base"] = -1;
            }
            if (c["__ori_used"] != 0) return;
            if (c["__ori_base"] >= 0 && gears > c["__ori_base"]) { c["__ori_used"] = 1; return; }
            if (!self.isAtBattlefield()) { c["__ori_base"] = -1; return; }
            c["__ori_base"] = gears;
            PlayerState::CostModifier m;
            m.source = sid;
            m.energy_reduction = 1;
            m.power_reduction = 1;
            m.gear_only = true;
            m.affects_friendly_only = true;
            m.this_turn_only = false;
            m.from_aura = true;
            state.player(controller).cost_modifiers.push_back(m);
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1048;
        d.def_id = R"RB(rad-068-167)RB";
        d.name = R"RB(Orianna, Mechanical Marvel)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-068/167)RB";
        d.collector_number = 68;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Piltover)RB",R"RB(Orianna)RB"};
        d.rarity = Rarity::Epic;
        d.energy_cost = 5;
        d.power_cost = 0;
        d.might = 5;
        d.ability_text = R"RB(Once each turn, while I'm at a battlefield, you may pay [1][A] less to play a gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/00f5a081a085e9c3851daf79d9e39bd378f71fe3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1048(CardRegistry& r) {
    r.registerCard(1048, std::make_unique<OriannaMechanicalMarvel>());
}

}  // namespace riftbound
