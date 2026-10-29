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

// "When you play a card with Energy cost [7] or more, you may play me from
// your trash for [Y]." ([Y] = one Order power.) Triggers from the trash
// (Card::triggersInTrash); the played card is the trigger's subject.
class MeticulousRestorationist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayACard; }
    bool triggersInTrash() const override { return true; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId,
                          TriggerType, GameObjectId) const override {
        return state.objectExists(source) && state.getObject(source).zone == ZoneType::Trash;
    }
    // "a card with Energy cost [7] or more": the played card's printed cost,
    // checked as it is played (a cheaper card doesn't trigger me).
    bool triggerConditionOnDefs(const GameState& state, const CardDB& db, GameObjectId,
                                PlayerId, TriggerType, GameObjectId subject) const override {
        return printedEnergyCost(state, db, subject) >= 7;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;

        const bool paid = costAlreadyPaid(ctx);
        auto in_trash = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   ctx.state.getObject(ctx.source).zone == ZoneType::Trash;
        };
        int go = confirmOptional(ctx, "Meticulous Restorationist: pay [Y] to play me from trash?",
                                 [&]() {
                                     return in_trash() &&
                                            (paid || payableOnePower(ctx.state, ctx.controller,
                                                                     Domain::Order));
                                 });
        if (go <= 0) return;
        if (costDueNow(ctx) && !payOnePower(ctx, ctx.controller, Domain::Order)) return;
        if (finalizingChoices(ctx)) return;
        if (!in_trash()) return;
        LocationId where;   // base or a battlefield I control (CR 355.2)
        if (pickPlayLocation(ctx, ctx.source, where) <= 0) return;
        auto& trash = ctx.state.player(ctx.state.getObject(ctx.source).owner).trash;
        auto it = std::find(trash.begin(), trash.end(), ctx.source);
        if (it != trash.end()) trash.erase(it);
        ctx.executor.playIgnoringCost(ctx.controller, ctx.source, where);
        ctx.events.logTrace("METICULOUS RESTORATIONIST: paid [Y] -> played from trash");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1096;
        d.def_id = R"RB(rad-120-167)RB";
        d.name = R"RB(Meticulous Restorationist)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-120/167)RB";
        d.collector_number = 120;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Piltover)RB"};   // gallery said "Device"; printed card says Piltover
        d.rarity = Rarity::Common;
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 2;
        d.ability_text = R"RB(When you play a card with Energy cost [7] or more, you may play me from your trash for [Y].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ee50fbf5e902e7109a3d54b417edc440b5a37198-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1096(CardRegistry& r) {
    r.registerCard(1096, std::make_unique<MeticulousRestorationist>());
}

}  // namespace riftbound
