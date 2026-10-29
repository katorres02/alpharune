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

// "If you would play a unit token to a location while I'm at a battlefield,
// you may play a 3 [M] Mech unit token to that same location instead." A
// replacement effect (Card::unitTokenReplacement): EffectExecutor::createToken
// asks the controller as the token is about to be played. Not offered for a
// token that already is a 3 [M] Mech (nothing would change).
// GAP: effects that stamp a card id on the token they made after createToken
// (e.g. Recruit = 954) may still stamp it on the Mech that replaced it unless
// they check card_counters["__token_replaced"]; name / Might / tags are the Mech's.
class HeimerdingerAcademyInstructor : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TokenReplacement unitTokenReplacement(const GameState& state, GameObjectId self,
                                          PlayerId, const std::string& token_name,
                                          int token_might) const override {
        if (!state.objectExists(self)) return {};
        const auto& me = state.getObject(self);
        if (!me.location.has_value() || !std::holds_alternative<BattlefieldLocation>(*me.location))
            return {};
        if (token_name == "Mech" && token_might == 3) return {};
        TokenReplacement r;
        r.valid = true;
        r.name = "Mech";
        r.might = 3;
        r.tags = {"Mech"};
        r.label = "Heimerdinger, Academy Instructor: play a 3 [M] Mech instead of " + token_name + "?";
        return r;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1108;
        d.def_id = R"RB(rad-132-167)RB";
        d.name = R"RB(Heimerdinger, Academy Instructor)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-132/167)RB";
        d.collector_number = 132;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Heimerdinger)RB", R"RB(Piltover)RB", R"RB(Yordle)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.ability_text = R"RB(If you would play a unit token to a location while I'm at a battlefield, you may play a 3 [M] Mech unit token to that same location instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1795e905171c74b84b4b7f84a57fecbb4bbd6f75-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1108(CardRegistry& r) {
    r.registerCard(1108, std::make_unique<HeimerdingerAcademyInstructor>());
}

}  // namespace riftbound
