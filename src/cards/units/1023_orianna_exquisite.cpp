#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <memory>
#include <vector>

namespace riftbound {
namespace {

// "[Shield]. I don't deal combat damage unless there's a friendly gear here."
// A gear "here" is a friendly gear at my location, attached Equipment included.
class OriannaExquisite : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject& self) {
            if (!self.location.has_value()) return;
            for (const auto& [id, o] : state.objects) {
                if (!o.isGear() || o.controller != controller) continue;
                // Attached gear is where its unit is.
                const GameObject* at = &o;
                if (o.attached_to.has_value() && state.objectExists(*o.attached_to))
                    at = &state.getObject(*o.attached_to);
                if (at->location.has_value() && *at->location == *self.location)
                    return;   // a friendly gear here
            }
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.suppress_combat_damage = true;
            self.aura_effects.push_back(ae);
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1023;
        d.def_id = R"RB(rad-042-167)RB";
        d.name = R"RB(Orianna, Exquisite)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-042/167)RB";
        d.collector_number = 42;
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Piltover)RB", R"RB(Orianna)RB"};
        d.rarity = Rarity::Rare;
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 4;
        d.keywords.set(Keyword::Shield);
        d.shield_value = 1;
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.)
I don't deal combat damage unless there's a friendly gear here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3e608c51698b6fb0cc6a00335dbd1bca84f57c73-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1023(CardRegistry& r) {
    r.registerCard(1023, std::make_unique<OriannaExquisite>());
}

}  // namespace riftbound
