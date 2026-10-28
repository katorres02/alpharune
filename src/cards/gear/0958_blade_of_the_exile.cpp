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
#include <vector>

namespace riftbound {
namespace {

class BladeOfTheExile : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasEquipAbility() const override { return true; }
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Ganking); return k; }
    // [Equip] — [A] or exhaust a friendly unit. Heuristic choice (no agent prompt):
    // exhaust a ready friendly unit with a "when I become exhausted" trigger if any,
    // else pay [A], else exhaust any other ready friendly unit.
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        auto& state = ctx.state; auto player = ctx.controller;
        GameObjectId synergy = kInvalidId, other = kInvalidId;
        for (auto& [id, o] : state.objects) {
            if (!o.isUnit() || o.controller != player || o.is_exhausted || !o.location.has_value()) continue;
            if (id == unit) continue;
            if (o.card_def_id == 957 || o.card_def_id == 961) { if (synergy == kInvalidId) synergy = id; }
            else if (other == kInvalidId) other = id;
        }
        GameObjectId pay_unit = synergy;
        if (pay_unit == kInvalidId) {
            if (payableOnePower(state, player, std::nullopt)) {
                if (!payOnePower(ctx, player, std::nullopt)) return false;
            } else pay_unit = other;
            if (pay_unit == kInvalidId && !state.objectExists(ctx.source)) return false;
        }
        if (pay_unit != kInvalidId) {
            ctx.events.logTrace("  EQUIP_COST: exhausted " + state.getObject(pay_unit).name);
            ctx.executor.exhaustObject(pay_unit);
        }
        attachFree(ctx, ctx.source, unit);
        return true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 958;
        d.def_id = R"RB(rad-150-167)RB";
        d.name = R"RB(Blade of the Exile)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-150/167)RB";
        d.collector_number = 150;
        d.card_type = CardType::Gear;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm,Domain::Chaos};
        d.tags = {R"RB(Equipment)RB",R"RB(Riven)RB"};
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB([Equip] — [A] or exhaust a friendly unit.)RB";
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might_bonus = 3;
        d.keywords = []{KeywordSet k; k.set(Keyword::Equip); return k;}();
        d.effect_text = R"RB([Ganking] (I can move from battlefield to battlefield.))RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_958(CardRegistry& r) {
    r.registerCard(958, std::make_unique<BladeOfTheExile>());
}

}  // namespace riftbound
