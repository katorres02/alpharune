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

class BeastBelow : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, return another friendly unit and an enemy unit to
    // their owners' hands." Both units are chosen at finalization: a pair
    // (friendly, enemy) when another friendly unit exists, else just the
    // enemy unit.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId me = ctx.controller;
        const GameObjectId self = ctx.source;
        auto friendlies = [&]() {
            return chooseableUnits(ctx.state, me, [&](const GameObject& u) {
                return u.controller == me && u.id != self;
            });
        };
        auto enemies = [&]() {
            return chooseableUnits(ctx.state, me,
                                   [&](const GameObject& u) { return u.controller != me; });
        };
        // Which helper was used at finalization (pair: resume_point >= 9).
        const int rp = ctx.state.chain.resuming ? ctx.state.chain.resuming->resume_point : 0;
        const bool use_pair = rp >= 9 || (rp < 6 && !friendlies().empty());
        GameObjectId f = kInvalidId, e = kInvalidId;
        if (use_pair) {
            auto en = enemies();
            auto pr = pickTargetPair(ctx, "Beast Below: return another friendly unit and an enemy unit",
                                     friendlies(), [en](GameObjectId) { return en; });
            f = pr.first;
            e = pr.second;
        } else {
            e = pickTarget(ctx, "Beast Below: return an enemy unit", enemies());
        }
        if (ctx.executor.hasPendingChoice()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        for (auto t : {f, e})
            if (t != kInvalidId && ctx.state.objectExists(t) &&
                ctx.state.getObject(t).location.has_value())
                ctx.executor.bounceToHand(t);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 453;
        d.def_id = R"RB(sfd-132-221)RB";
        d.name = R"RB(Beast Below)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-132/221)RB";
        d.collector_number = 132;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, return another friendly unit and an enemy unit to their owners' hands.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d27a56111c68c651099d7b73b22a54a825458857-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_453(CardRegistry& r) {
    r.registerCard(453, std::make_unique<BeastBelow>());
}

} // namespace riftbound
