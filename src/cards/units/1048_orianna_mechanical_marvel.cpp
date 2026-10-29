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
    // play a gear." An optional from_aura cost modifier offered while I'm at a
    // battlefield and unused this turn: the player chooses whether to take it
    // as each gear is paid for (GameEngine::decideOptionalCostModifiers);
    // taking it stamps the turn into my "__ori_used_turn" counter.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject& self) {
            if (!self.isAtBattlefield()) return;
            auto used = self.card_counters.find(kUsed);
            if (used != self.card_counters.end() && used->second == state.turn.turn_number) return;
            PlayerState::CostModifier m;
            m.source = sid;
            m.energy_reduction = 1;
            m.power_reduction = 1;
            m.gear_only = true;
            m.affects_friendly_only = true;
            m.this_turn_only = false;
            m.from_aura = true;
            m.optional = true;
            m.used_marker = kUsed;
            state.player(controller).cost_modifiers.push_back(m);
        });
    }
private:
    static constexpr const char* kUsed = "__ori_used_turn";
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
