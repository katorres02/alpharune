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

class PoppyDefenderOfTheMeek : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "You may spend 3 XP as an additional cost to play me. If you do, I cost
    //  [3] less." ([Ambush]/[Tank] engine-handled.)
    // An optional additional cost: the engine asks before paying, takes the XP
    // and stages the [3] discount (it doesn't offer "no" when only the
    // discounted price is payable).
    OptionalAdditionalCost optionalAdditionalCost() const override {
        OptionalAdditionalCost c;
        c.valid = true;
        c.spend_xp = 3;
        c.reduce_energy = 3;
        return c;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 740;
        d.def_id = R"RB(unl-178-219)RB";
        d.name = R"RB(Poppy, Defender of the Meek)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-178/219)RB";
        d.collector_number = 178;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Yordle)RB", R"RB(Demacia)RB", R"RB(Poppy)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Reaction);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB(You may spend 3 XP as an additional cost to play me. If you do, I cost [3] less.
[Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
[Tank] (I must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1a75c5322b2179e772af83b1fd16fed864c5bf24-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_740(CardRegistry& r) {
    r.registerCard(740, std::make_unique<PoppyDefenderOfTheMeek>());
}

} // namespace riftbound
