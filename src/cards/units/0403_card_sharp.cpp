#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class CardSharp : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, you and each opponent may play a Gold gear token
    // exhausted. For each opponent who did, you play a Gold gear token
    // exhausted." Your "may" is decided at finalization; the opponent decides
    // as the ability resolves (resume_point 20 = consume the opponent's answer).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int mine = confirmOptional(ctx, "Card Sharp: play a Gold gear token exhausted?",
                                   [] { return true; });
        if (mine == -1) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        const PlayerId opp = opponent(ctx.controller);
        bool opp_did = true;   // direct invocation (no chain): opponent accepts
        if (ctx.state.chain.resuming.has_value()) {
            auto& ri = *ctx.state.chain.resuming;
            if (ri.resume_point < 20) {
                ctx.executor.requestChoice(opp,
                    {valChoice(opp, 0, "No - don't play a Gold token"),
                     valChoice(opp, 1, "Yes - play a Gold token (Card Sharp's controller gets one too)")},
                    "Card Sharp: opponent may play a Gold gear token exhausted");
                ri.resume_point = 20;
                return;
            }
            if (ri.resume_point != 20) return;
            ri.resume_point = 21;
            opp_did = takenValue(ctx, 0) == 1;
        }
        if (mine == 1) createGoldExhausted(ctx);
        if (opp_did) {
            auto tok = ctx.executor.createToken(opp, CardType::Gear, "Gold", 0, {}, {},
                                                LocationId{BaseLocation{opp}}, /*enter_ready=*/false);
            if (ctx.state.objectExists(tok)) {
                auto& gold = ctx.state.getObject(tok);
                gold.is_exhausted = true;
                gold.card_def_id = kGoldGearCardDefId;   // CR 187.5 Gold token ability
            }
            // "For each opponent who did, you play a Gold gear token exhausted."
            createGoldExhausted(ctx);
        }
        ctx.events.logTrace(std::string("CARD SHARP: you ") + (mine == 1 ? "played" : "declined") +
                            ", opponent " + (opp_did ? "played" : "declined"));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 403;
        d.def_id = R"RB(sfd-081-221)RB";
        d.name = R"RB(Card Sharp)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-081/221)RB";
        d.collector_number = 81;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, you and each opponent may play a Gold gear token exhausted. For each opponent who did, you play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8bfd5dc22473d439cd78d5a3cb23ed03de66953d-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_403(CardRegistry& r) {
    r.registerCard(403, std::make_unique<CardSharp>());
}

} // namespace riftbound
