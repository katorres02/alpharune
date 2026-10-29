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
#include "cards/gear/equip_base.h"

namespace riftbound {
namespace {

class ExperimentalHexplate : public SimpleEquipGear {
public:
    ExperimentalHexplate() : SimpleEquipGear(Domain::Mind) {}
    const CardDef& def() const override { return def_; }
    // effect_text "I am a Mech.": the unit this is attached to gains the Mech
    // tag while attached (removed again on unattach unless it prints it).
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        if (!SimpleEquipGear::onEquip(ctx, unit)) return false;
        if (!ctx.state.objectExists(ctx.source)) return true;
        auto& gear = ctx.state.getObject(ctx.source);
        if (gear.attached_to.has_value() && ctx.state.objectExists(*gear.attached_to)) {
            auto& u = ctx.state.getObject(*gear.attached_to);
            if (!hasTag(u, "Mech")) {
                u.tags.push_back("Mech");
                gear.card_counters["__granted_mech_to"] = static_cast<int>(u.id);
            }
        }
        return true;
    }
    void onUnattached(CardContext& ctx, GameObjectId unit) override {
        if (!ctx.state.objectExists(ctx.source) || !ctx.state.objectExists(unit)) return;
        auto& gear = ctx.state.getObject(ctx.source);
        auto it = gear.card_counters.find("__granted_mech_to");
        if (it == gear.card_counters.end() || it->second != static_cast<int>(unit)) return;
        gear.card_counters.erase(it);
        auto& tags = ctx.state.getObject(unit).tags;
        auto t = std::find(tags.begin(), tags.end(), std::string("Mech"));
        if (t != tags.end()) tags.erase(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 396;
        d.def_id = R"RB(sfd-073-221)RB";
        d.name = R"RB(Experimental Hexplate)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-073/221)RB";
        d.collector_number = 73;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 1;
        d.might_bonus = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [B] ([B]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(I am a Mech.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/940a5e9796d4ac10a238d9b2612545c04bd9a570-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_396(CardRegistry& r) {
    r.registerCard(396, std::make_unique<ExperimentalHexplate>());
}

} // namespace riftbound
