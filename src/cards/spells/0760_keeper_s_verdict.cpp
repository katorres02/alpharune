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

class KeeperSVerdict : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true,
                                   .must_be_at_battlefield = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller == controller) continue;
            // "Can't be chosen by enemy spells and abilities."
            if (obj.untargetable_by_enemy) continue;
            if (obj.isAtBattlefield()) out.push_back(id);
        }
        return out;
    }
    // The caster chooses the unit (as the spell is played); its OWNER chooses
    // top or bottom (stepChooseValue publishes that choice to them, after
    // pickTarget's resume points). A token ceases to exist instead (CR 183.1).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        GameObjectId picked = pickTarget(ctx, "Keeper's Verdict: place enemy unit on deck",
                                         enumerateLegalTargets(ctx.state, ctx.controller));
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting on the agent
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (!ctx.state.getObject(picked).location.has_value()) return;
        const PlayerId owner = ctx.state.getObject(picked).owner;
        const bool token = ctx.state.getObject(picked).isToken();

        int mode = 0;   // 0 = top, 1 = bottom
        if (!token &&
            !stepChooseValue(ctx, owner, {{0, "Top of your Main Deck"}, {1, "Bottom of your Main Deck"}},
                             "Keeper's Verdict: top or bottom of your Main Deck", 20, 10, 0, mode))
            return;   // the owner is choosing
        if (!ctx.state.objectExists(picked) || !ctx.state.getObject(picked).location.has_value())
            return;

        auto& u = ctx.state.getObject(picked);
        auto was_at = u.location.value_or(BaseLocation{owner});
        const PlayerId was_controller = u.controller;
        // Detach any attached gear first (CR 719.5).
        for (auto gid : std::vector<GameObjectId>(u.attachments)) {
            if (ctx.state.objectExists(gid)) ctx.executor.unattachGear(gid);
        }
        u.attachments.clear();
        u.attachment_might_bonus = 0;
        u.damage_marked = 0;
        u.combat_designation = CombatDesignation::None;
        u.location = std::nullopt;
        if (token) {
            u.zone = ZoneType::Banishment;   // ceases to exist; never enters the deck
            ctx.events.emit(LeftBoardEvent{picked, was_controller, CardType::Unit, was_at,
                                           ZoneType::Banishment, /*was_killed=*/false});
            ctx.events.logTrace("KEEPER'S VERDICT: token " + u.name + " ceases to exist");
            return;
        }
        u.zone = ZoneType::MainDeck;
        auto& deck = ctx.state.player(owner).main_deck;
        if (mode == 0) deck.push_back(picked);            // top = back
        else           deck.insert(deck.begin(), picked); // bottom = front
        ctx.events.emit(LeftBoardEvent{picked, was_controller, CardType::Unit, was_at,
                                       ZoneType::MainDeck, /*was_killed=*/false});
        ctx.events.logTrace("KEEPER'S VERDICT: owner placed " + u.name +
                            (mode == 0 ? " on TOP" : " on BOTTOM") + " of their deck");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 760;
        d.def_id = R"RB(unl-204-219)RB";
        d.name = R"RB(Keeper's Verdict)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-204/219)RB";
        d.collector_number = 204;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Poppy)RB"};
        d.energy_cost = 2;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose an enemy unit at a battlefield. Its owner places it on the top or bottom of their Main Deck.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/90dcafd8ca6cedd534416232ebd29c451f12107c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_760(CardRegistry& r) {
    r.registerCard(760, std::make_unique<KeeperSVerdict>());
}

} // namespace riftbound
