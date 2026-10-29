#include "cards/card.h"
#include "cards/card_registry.h"
#include "cards/card_helpers.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class CaptainFarron : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Other friendly units here have [Assault]." Each instance of me grants
    // [Assault] (with its +1 [M] while attacking) to every OTHER friendly unit
    // at my battlefield. I don't have [Assault] myself.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject& self) {
            auto my_bf = self.battlefieldId();
            if (!my_bf) return;  // "here" is a battlefield
            for (auto& [uid, u] : state.objects) {
                if (uid == sid) continue;  // "other"
                if (!u.isUnit() || u.controller != controller) continue;
                if (u.battlefieldId() != my_bf) continue;
                auraKeyword(u, sid, Keyword::Assault);
            }
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 15;
        d.def_id = R"RB(ogn-015-298)RB";
        d.name = R"RB(Captain Farron)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-015/298)RB";
        d.collector_number = 15;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Trifarian)RB", R"RB(Noxus)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Other friendly units here have [Assault]. (+1 [M] while they're attackers.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4c65e2cae6748590f589ad8b26bc5a20c0d770c2-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_15(CardRegistry& r) {
    r.registerCard(15, std::make_unique<CaptainFarron>());
}

} // namespace riftbound
