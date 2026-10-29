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

class IcathianRain : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Deal 2 to a unit." x6 — six separate instructions, each choosing its
    // own unit (the same unit may be chosen more than once). All six units
    // are chosen as the spell is played (kFinalizeResolve, CR 355); each
    // instruction is skipped if its unit is gone by resolution.
    // Resumable: resume_point 20 + 2i publishes choice i, 21 + 2i consumes it;
    // the picks live in resume_data[10 + i]. 32 = all chosen.
    static constexpr int kHits = 6, kFirstRp = 20, kDoneRp = kFirstRp + 2 * kHits,
                         kData = 10;
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // One Play intent with no targets; the six choices are made through the
    // card's own prompts (same opt-in Falling Star uses for its two).
    bool needsPlayTimeTargetPair() const override { return true; }

    static std::vector<GameObjectId> units(const GameState& state) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.location.has_value()) out.push_back(id);
        return out;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        while (ri.resume_data.size() < static_cast<size_t>(kData + kHits))
            ri.resume_data.push_back(static_cast<int32_t>(kInvalidId));
        if (ri.resume_point < kFirstRp) ri.resume_point = kFirstRp;
        while (ri.resume_point < kDoneRp) {
            const int i = (ri.resume_point - kFirstRp) / 2;
            if ((ri.resume_point - kFirstRp) % 2 == 0) {
                auto legal = units(ctx.state);
                if (legal.empty()) { ri.resume_point += 2; continue; }
                std::vector<Intent> options;
                for (auto t : legal) options.push_back(objChoice(ctx.controller, t));
                ctx.executor.requestChoice(ctx.controller, std::move(options),
                    "Icathian Rain: deal 2 to a unit (" + std::to_string(i + 1) + "/6)");
                ri.resume_point += 1;
                return;   // waiting for the agent
            }
            ri.resume_data[kData + i] = static_cast<int32_t>(takenObject(ctx));
            ri.resume_point += 1;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)

        for (int i = 0; i < kHits; ++i) {
            const auto t = static_cast<GameObjectId>(ri.resume_data[kData + i]);
            if (t == kInvalidId || !ctx.state.objectExists(t)) continue;
            const auto& u = ctx.state.getObject(t);
            if (!u.isUnit() || !u.location.has_value()) continue;   // left the board
            ctx.executor.dealDamage(t, 2, ctx.source);
            if (ctx.state.objectExists(t) && ctx.state.getObject(t).hasLethalDamage())
                ctx.executor.killObject(t);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 247;
        d.def_id = R"RB(ogn-248-298)RB";
        d.name = R"RB(Icathian Rain)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-248/298)RB";
        d.collector_number = 248;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Mind};
        d.tags = {R"RB(Kai'Sa)RB"};
        d.energy_cost = 7;
        d.power_cost = 3;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Deal 2 to a unit.
Deal 2 to a unit.
Deal 2 to a unit.
Deal 2 to a unit.
Deal 2 to a unit.
Deal 2 to a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8df37f633d3da734f3f2d85808a7cb9eadec1b04-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_247(CardRegistry& r) {
    r.registerCard(247, std::make_unique<IcathianRain>());
}

} // namespace riftbound
