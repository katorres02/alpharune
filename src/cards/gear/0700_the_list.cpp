#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class TheList : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    static std::string namedTag(const GameState& state, GameObjectId gear) {
        if (!state.objectExists(gear)) return "";
        auto& g = state.getObject(gear);
        auto it = g.string_state.find("named_tag");
        return it == g.string_state.end() ? "" : it->second;
    }

    // "As you play this, name a tag." A player choice, made as it is played
    // (AsYouPlayMe). Any tag can be named: the candidates are every tag on a
    // card in the card pool (CardDB), sorted. There are more of them than
    // MakeChoice has int slots, so the name is asked in steps — a range of the
    // list, then a tag within it (stepChooseFromList) — each option its own slot.
    TriggerType triggerType() const override { return TriggerType::AsYouPlayMe; }
    static std::vector<std::string> candidateTags(const CardDB& db) {
        std::set<std::string> tags;
        for (const auto& [id, d] : db.all())
            for (const auto& t : d.tags) tags.insert(t);
        return {tags.begin(), tags.end()};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // The list depends only on the card pool: identical on re-entry.
        const auto tags = candidateTags(ctx.executor.cardDB());
        int m = -1;
        if (!stepChooseFromList(ctx, ctx.controller, tags, "The List: name a tag",
                                /*rp=*/20, /*slot=*/10, m))
            return;   // waiting for agent
        if (m < 0 || m >= static_cast<int>(tags.size())) {
            ctx.events.logTrace("THE LIST: no tag to name");
            return;
        }
        ctx.state.getObject(ctx.source).string_state["named_tag"] = tags[m];
        ctx.events.logTrace("THE LIST: named tag \"" + tags[m] + "\"");
    }

    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility ab;
        ab.cost = ActivationCost{.exhaust = true};
        ab.targets = TargetRequirements{.count = 1, .must_be_unit = true};
        ab.needs_activation_time_target = true;  // pickTarget reads named tag
        return {ab};
    }

    // Broad gate (instance unknown here): any unit carrying any tag is a
    // plausible target. The precise named-tag filter happens in onActivate.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int /*ability_index*/) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            // "Can't be chosen by enemy spells and abilities."
            if (obj.controller != controller && obj.untargetable_by_enemy) continue;
            if (obj.isUnit() && obj.location.has_value() && !obj.tags.empty())
                out.push_back(id);
        }
        return out;
    }

    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& /*targets*/) override {
        std::string tag = namedTag(ctx.state, ctx.source);
        // Legal targets: units that carry the named tag.
        std::vector<GameObjectId> legal;
        if (!tag.empty()) {
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || !obj.location.has_value()) continue;
                for (const auto& t : obj.tags) {
                    if (t == tag) { legal.push_back(id); break; }
                }
            }
        }
        GameObjectId picked = pickTarget(ctx, "The List: -2 [M] (tag=" + tag + ")",
                                         legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.giveTemporaryMight(picked, -2);
        ctx.events.logTrace("THE LIST: -2 [M] this turn to a \"" + tag + "\" unit");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 700;
        d.def_id = R"RB(unl-138-219)RB";
        d.name = R"RB(The List)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-138/219)RB";
        d.collector_number = 138;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(As you play this, name a tag. (For example, Miss Fortune, Demacia, and Poro are tags.)
[E]: Give a unit with the named tag -2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f35b49faeb444071445662c9af13710c00a4569c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_700(CardRegistry& r) {
    r.registerCard(700, std::make_unique<TheList>());
}

} // namespace riftbound
