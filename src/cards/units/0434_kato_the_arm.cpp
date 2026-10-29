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

// "When I move to a battlefield, give another friendly unit my keywords and
//  +[M] equal to my Might this turn." ([Deflect] is Kato's printed keyword.)

class KatoTheArm : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        std::vector<GameObjectId> others;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;  // "another"
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            others.push_back(id);
        }
        GameObjectId tgt = pickTarget(ctx, "Kato: give another friendly unit my keywords + Might", others);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // Grant my current keywords (printed, granted this turn, or from a
        // continuous effect), with their values: [Assault N] / [Shield N]
        // carry N (CR 807.2: values sum on the recipient). [Deflect] is
        // granted as the keyword only — giveTemporaryKeyword's Deflect value
        // has no end-of-turn revert, and the keyword alone costs [A].
        for (uint32_t k = 0; k < static_cast<uint32_t>(Keyword::Count); ++k) {
            auto kw = static_cast<Keyword>(k);
            if (!self.hasKeyword(kw)) continue;
            int value = 0;
            if (kw == Keyword::Assault || kw == Keyword::Shield) {
                value = kw == Keyword::Assault ? self.assault_value : self.shield_value;
                for (const auto& ae : self.aura_effects)
                    if (ae.keyword == kw) value += std::max(1, ae.keyword_value);
                value = std::max(1, value);
            }
            ctx.executor.giveTemporaryKeyword(tgt, kw, value);
        }
        // +[M] equal to my Might this turn.
        ctx.executor.giveTemporaryMight(tgt, self.current_might);
        ctx.events.logTrace("KATO THE ARM: granted my keywords + " +
                            std::to_string(self.current_might) + " [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 434;
        d.def_id = R"RB(sfd-112-221)RB";
        d.name = R"RB(Kato the Arm)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-112/221)RB";
        d.collector_number = 112;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
When I move to a battlefield, give another friendly unit my keywords and +[M] equal to my Might this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f4ea4f2f5169a3813d9eb7f0d3d2f41bdddebfc7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_434(CardRegistry& r) {
    r.registerCard(434, std::make_unique<KatoTheArm>());
}

} // namespace riftbound
