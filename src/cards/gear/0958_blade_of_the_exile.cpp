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
    // [Equip] — [A] or exhaust a friendly unit. The player picks how to pay
    // (EffectExecutor::chooseObjectNow — equip resolves without a chain item):
    // choosing this gear means "pay [A]", choosing a ready friendly unit (the
    // one being equipped included) means "exhaust it". Nothing payable, no equip.
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        auto& state = ctx.state; auto player = ctx.controller;
        if (!state.objectExists(unit) || !state.objectExists(ctx.source)) return false;
        std::vector<GameObjectId> options;
        if (payableOnePower(state, player, std::nullopt)) options.push_back(ctx.source);
        std::vector<GameObjectId> units;
        for (auto& [id, o] : state.objects)
            if (o.isUnit() && o.controller == player && !o.is_exhausted && o.location.has_value())
                units.push_back(id);
        std::sort(units.begin(), units.end());
        options.insert(options.end(), units.begin(), units.end());
        if (options.empty()) return false;   // can't pay either cost
        const GameObjectId how = ctx.executor.chooseObjectNow(player, options);
        if (how == ctx.source) {
            if (!payOnePower(ctx, player, std::nullopt)) return false;
            ctx.events.logTrace("  EQUIP_COST: paid [A]");
        } else {
            if (how == kInvalidId || !state.objectExists(how)) return false;
            ctx.events.logTrace("  EQUIP_COST: exhausted " + state.getObject(how).name);
            ctx.executor.exhaustObject(how);
        }
        if (!state.objectExists(unit) || !state.getObject(unit).location.has_value()) return false;
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
