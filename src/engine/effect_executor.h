#pragma once
/// @file effect_executor.h
/// Atomic game operation helpers used by Card objects.
///
/// Card objects call these methods to modify game state (deal damage,
/// draw cards, bounce units, etc.). This is a utility library, not a
/// dispatch mechanism — Card objects are the dispatch layer.

#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"

#include <functional>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace riftbound {

class CardRegistry;

/// Which parts of a card's cost an effect-driven play still charges
/// (EffectExecutor::playPayingCost).
struct EffectPlayCost {
    bool ignore_energy = false;   // "ignoring its Energy cost"
    bool ignore_power = false;
    int energy_reduction = 0;     // "reducing its Energy cost by [N]"
};

// State-only queries shared by the executor, the action generators and
// Card::enumerateLegalTargets (which has no executor to ask).
/// [A] an opponent must pay to choose `target` (CR 808 Deflect); 0 if
/// `chooser` controls it or Deflect is ignored at its battlefield.
int deflectCostFor(const GameState& state, PlayerId chooser, GameObjectId target);
/// Power of any Domain `player` could pay right now.
int payableAnyPower(const GameState& state, PlayerId player);
/// Where `player` may put `card` (a unit or gear) as they play it (CR 355.2):
/// their base or a battlefield they control, widened by the unit's own
/// permissions ("play me to an open battlefield", ...) and standing grants
/// (CR 355.2.b), or replaced by the card's own list when it restricts its play
/// locations ([Deploy] gear, CR 830). Gear goes to base (CR 149.2). With
/// `include_ambush`, a unit with [Ambush] also offers battlefields where
/// `player` controls units (CR 822.1.c) — the play-from-hand generator offers
/// those with Reaction timing instead. Empty for spells / nothing legal.
std::vector<LocationId> legalPlayLocations(const GameState& state, const CardDB& db,
                                           const CardRegistry* registry, PlayerId player,
                                           GameObjectId card, bool include_ambush);

class EffectExecutor {
public:
    EffectExecutor(GameState& state, EventBus& events, const CardDB& card_db,
                   const CardRegistry* card_registry = nullptr);

    /// Register the CardRegistry after construction. Lets effects that
    /// re-play a card (Dazzling Aurora, Mirror Image, etc.) reach the
    /// played card's onPlay hook — without the registry, playIgnoringCost
    /// silently skips "As you play me" effects (Baron Nashor's Pit
    /// spawn, etc.). Tests that don't exercise the play path may leave
    /// this null.
    void setCardRegistry(const CardRegistry* registry) { card_registry_ = registry; }

    /// Access the CardDB for card-side code that needs to look up static
    /// card data (e.g. Virtuoso needs to read a chain item's energy_cost
    /// from the def). Returned reference is valid for the executor's life.
    const CardDB& cardDB() const { return card_db_; }

    /// Set the RNG for effects that need randomness (reveal, recycle).
    void setRng(std::mt19937_64* rng) { rng_ = rng; }

    /// Set agent query callback for effects that require player choice (discard).
    using AgentChoiceQuery = std::function<Intent(PlayerId, const std::vector<Intent>&)>;
    void setAgentQuery(AgentChoiceQuery query) { agent_query_ = std::move(query); }

    /// Ask `player` to choose one of `objects` right now, for an effect that
    /// is not resolving from the chain and so cannot suspend (an Equip
    /// attaching). Returns the first object when there is no agent to ask, and
    /// kInvalidId when `objects` is empty.
    GameObjectId chooseObjectNow(PlayerId player, const std::vector<GameObjectId>& objects);
    /// Ask `player` a yes/no question right now, for an effect that is not
    /// resolving from the chain and so cannot suspend (a "you may pay" death
    /// replacement). Options are [no, yes]; returns `fallback` with no agent.
    bool confirmNow(PlayerId player, const std::string& label, bool fallback = true);

    /// "You may make new choices for it" (CR 750-755): the item's controller
    /// re-chooses its targets. Costs already paid are unaffected (755.1).
    using RechooseFn = std::function<void(ChainItemId)>;
    void setRechoose(RechooseFn fn) { rechoose_ = std::move(fn); }
    void makeNewChoices(ChainItemId item) { if (rechoose_) rechoose_(item); }

    // ── Atomic game operations ──
    void dealDamage(GameObjectId target, int amount, GameObjectId source);
    void killObject(GameObjectId target);
    /// Exhaust several objects at the same time (an effect or cost that
    /// exhausts a group): each ready one becomes exhausted ("exhausted" state
    /// event), then the friendly units among them are reported as exhausted
    /// together by `by` (reportExhaustedTogether).
    void exhaustTogether(PlayerId by, const std::vector<GameObjectId>& objects);
    /// "When you exhaust N or more friendly units at the same time"
    /// (Seraphine, Inspiring): `units` were just exhausted together by `by`;
    /// the ones that are `by`'s units set PlayerState::last_exhaust_group_size
    /// and emit "exhausted_together". Used by standard moves and exhaustTogether.
    void reportExhaustedTogether(PlayerId by, const std::vector<GameObjectId>& units);
    /// Fill GameObject::last_known from the object's current state (called by
    /// the kill paths just before it dies).
    void snapshotLastKnown(GameObjectId id);
    /// Snapshot a group of units that die at once (cleanup's lethal-damage
    /// pass), so each one's last-known information sees the others still there.
    void snapshotDyingTogether(const std::vector<GameObjectId>& ids);
    /// The death replacements effects left on `unit` (Card::
    /// applyGrantedDeathReplacement). True if one applied: it does not die.
    bool applyGrantedDeathReplacements(GameObjectId unit);
    /// The card registry, when set (null in bare tests).
    const CardRegistry* cardRegistry() const { return card_registry_; }
    void drawCards(PlayerId player, int count);
    void bounceToHand(GameObjectId target);
    void giveTemporaryMight(GameObjectId target, int amount, int minimum = 0);
    void giveTemporaryKeyword(GameObjectId target, Keyword kw, int value);
    /// [Buff] (CR 427.1.b): place a buff counter on `target` unless it already
    /// has one (and its card doesn't allow any number of buffs). Returns
    /// whether a buff was placed.
    bool buffUnit(GameObjectId target);
    /// Spend a buff (CR 427.1.b): remove one buff counter from `unit` (its
    /// +1 [M] goes with it) and emit ObjectStateChangedEvent "buff_spent"
    /// (-> WhenYouSpendABuff for the unit's controller). Returns false, doing
    /// nothing, if the unit has no buff. Every "spend a buff" goes through here.
    bool spendBuff(GameObjectId unit);
    void readyObject(GameObjectId target);
    // ── Moves and recalls (CR 447-460; rules in core/move_rules.h) ──
    // An effect move is a Move: the unit's and battlefield's movement
    // restrictions apply ("can't move this turn", "can't move to base",
    // "can't be moved by enemy spells and abilities", "can't move from here to
    // base"), and so do applied costs on moving (Gravity Field, Mageseeker
    // Investigator), paid by `mover` — the resolving item's controller when
    // left None. A move that is forbidden, or whose applied cost is not paid,
    // doesn't happen; these return whether the unit moved.
    bool moveToBase(GameObjectId target, PlayerId mover = PlayerId::None);
    /// Recall (CR 457-460): send `target` to its controller's base. Not a
    /// Move: no movement restriction applies, no move triggers fire, and its
    /// damage and statuses are unaffected. Attached gear goes with it.
    void recallToBase(GameObjectId target);
    /// Move several units to one destination at the same time (one move,
    /// CR 144.3 / 204.4): the applied cost is charged once for the group
    /// (Mageseeker's [A] per unit beyond the first). Units that can't move
    /// are left out. Returns the units that moved.
    std::vector<GameObjectId> moveUnitsTogether(const std::vector<GameObjectId>& units,
                                                const LocationId& dest,
                                                PlayerId mover = PlayerId::None);
    /// Unattach a single gear from the unit it's equipped to. Removes it from
    /// the unit's attachment list, refunds the unit's might bonus, and clears
    /// the gear's attachment state. Per CR 719.5 the detached gear stays at
    /// its current location (on board, unattached). No-op if not attached.
    void unattachGear(GameObjectId gear);
    /// Move a unit to the given battlefield. Updates `location` + `zone`,
    /// emits UnitMovedEvent. Caller is responsible for any contested-status
    /// re-evaluation downstream (cleanup recomputes BF contested flags).
    /// `from_play` is false: this is a card-effect-driven move, not a play.
    bool moveToBattlefield(GameObjectId target, BattlefieldId bf, PlayerId mover = PlayerId::None);
    /// "If they both move to a battlefield you don't control, you're the
    /// attacker" (Void Assault): `player` applies Contested at `bf` (CR 190.3),
    /// so the combat staged there has them as the attacker (runCombat takes the
    /// attacker from the contesting player). No-op if `player` controls `bf` or
    /// a showdown / combat is already under way there.
    void contestAsAttacker(BattlefieldId bf, PlayerId player);
    void stunUnit(GameObjectId target);
    /// Stun + attribute to the controller of `stunner_source`. Use this from
    /// card effects so the resulting UnitStunnedEvent identifies the stunner
    /// for WhenYouStun triggers (e.g. Vex Mocking).
    void stunUnitBy(GameObjectId target, GameObjectId stunner_source);
    void discardCards(PlayerId player, int count);
    /// Make a specific player discard (for opponent targeting: "they discard 1").
    void opponentDiscards(PlayerId opponent, int count);
    /// Commit a single-card discard chosen by the agent earlier (e.g. via
    /// requestChoice/takeChoice during resumable resolution). Moves the card
    /// from hand → trash, sets `has_discarded_this_turn`, emits
    /// CardsDiscardedEvent. No-op if `card_id` isn't in player's hand.
    void applyDiscard(PlayerId player, GameObjectId card_id);
    void recycleCards(PlayerId player, const std::vector<GameObjectId>& cards);
    void banishObject(GameObjectId target);

    // ── Vendetta game actions ──
    /// Empower (CR 442): give `target` an Empowered status if it has room
    /// (binary unless its Card::maxEmpower() allows stacking). Returns true if
    /// a status was added; emits ObjectStateChangedEvent "empowered".
    bool empower(GameObjectId target);
    /// Disempower (CR 443): remove one Empowered status. Returns false (and
    /// does nothing) if the object wasn't Empowered.
    bool disempower(GameObjectId target);
    /// Burn X (CR 441): move the top `count` cards of `player`'s Main Deck to
    /// their trash, burning out if the deck runs dry. Returns the burned
    /// cards (in burn order); emits ObjectStateChangedEvent "burned" per card.
    std::vector<GameObjectId> burn(PlayerId player, int count);
    /// One Burn Out (CR 432.2): recycle `player`'s trash into their Main Deck
    /// (possibly nothing) and give the opponent 1 point. Returns true if the
    /// deck now has cards and the game is still going.
    bool burnOut(PlayerId player);
    /// Burn out repeatedly until the deck has a card or the game ends (CR 432.3).
    bool burnOutUntilDeckHasCards(PlayerId player);
    void healObject(GameObjectId target);
    void exhaustObject(GameObjectId target);
    void channelRunes(PlayerId player, int count, bool enter_exhausted = false);
    /// Channel without applying "if you would channel" replacements (Seraphic
    /// Wyvern's extra rune) — used for the replacement's own extra runes.
    void channelRunesNoReplacement(PlayerId player, int count, bool enter_exhausted = false);

    // Phase 6q+ — Add abilities (CR 429). Add floating energy / power to
    // a player's rune pool. Floating resources are spent before runes,
    // and the entire pool clears at end of turn (CR 317.2.d already
    // handled by expirationStep::emptyRunePools).
    //
    // Examples that use this:
    //   - Jhin, Murderous Artist [584]: "When I move, [Add] [1][A]."
    //   - Hextech Anomaly [405]: "Pay any amount of [A] to Add that
    //     much Energy."
    //   - Various "Add" rune-pool effects.
    //
    // Per CR 429.2, Add abilities resolve immediately (not via chain).
    // These helpers are called directly from Card::onTrigger / onActivate
    // bodies; no chain bookkeeping required.
    void addFloatingEnergy(PlayerId player, int amount);
    /// Energy (`power` false) or [A] (`power` true) that may only be spent on
    /// payments whose SpendPurpose overlaps `uses` ("Use only to play spells",
    /// "Spend this Power only to play units or gear") and, with
    /// `showdown_only`, only while a showdown is in progress
    /// (RunePool::restricted).
    void addRestrictedResource(PlayerId player, bool power, int amount, SpendPurpose uses,
                               bool showdown_only = false);
    /// Energy usable only to play spells (Lux, Crownguard).
    void addSpellOnlyEnergy(PlayerId player, int amount);
    /// Energy that can only pay for units and activated abilities of units.
    void addUnitOnlyEnergy(PlayerId player, int amount);
    void addFloatingPower(PlayerId player, Domain domain, int amount);
    /// Universal power ([A]). Counted separately from domain-specific power
    /// and spendable for any cost.
    void addFloatingUniversalPower(PlayerId player, int amount);

    // ── Paying inside an effect ──
    /// Power of any Domain ([A]) `player` could pay now: floating power of
    /// every kind plus runes in base (CR 164.2.b — recycle, ready or exhausted).
    int payableAnyPower(PlayerId player) const;
    /// Pay `amount` [A]: floating power first, then recycle runes (exhausted
    /// ones first). Pays nothing and returns false if it can't be covered.
    bool payAnyPower(PlayerId player, int amount);

    // ── Scoring by an effect (CR 471 "score N points" outside Conquer/Hold) ──
    /// Give `player` `amount` points from an effect: honours "can't gain/score
    /// points" locks and ends the game at once if this reaches the Victory
    /// Score with more points than every opponent. Returns points gained.
    int scorePoints(PlayerId player, int amount);
    /// Victory Score in effect right now (mode score plus modifiers).
    int effectiveVictoryScore() const;

    // ── Deflect (CR: "Opponents must pay [A] to choose me") ──
    /// [A] an opponent must pay to choose `target` with a spell or ability.
    /// 0 when `chooser` controls it, it has no Deflect, or Deflect is ignored
    /// at its battlefield.
    int deflectCostFor(PlayerId chooser, GameObjectId target) const;

    /// Ask the engine to recompute continuous effects now (auras, Might) —
    /// for an effect that changes what feeds them and then reads the result
    /// in the same resolution (Lacerate: disempower, then check Might).
    void setAuraRecalc(std::function<void()> fn) { aura_recalc_ = std::move(fn); }
    void recalculateContinuousEffects() { if (aura_recalc_) aura_recalc_(); }

    // Phase 6q+ — take control of a unit (CR 416 + "take control of").
    // Mutates GameObject::controller. If `until_end_of_turn` is true,
    // saves the original controller and reverts in expirationStep.
    // If false, control change is permanent (until something else
    // changes it). Used by Akshan, Mischievous + Conscription.
    void takeControl(GameObjectId target, PlayerId new_controller,
                      bool until_end_of_turn = false);

    // Take control of `target` only until `source` leaves the board (Akshan,
    // Mischievous: "You control it until I leave the board"). Records the
    // controlling source + the controller to restore; GameEngine's cleanup
    // reverts control once `source` is no longer in play. Snapshots the
    // restore-controller once (earliest owner wins if stacked).
    void takeControlUntilSourceLeaves(GameObjectId target,
                                      PlayerId new_controller,
                                      GameObjectId source);

    // ── Entering the board ready ──
    /// How a unit is entering the board (asked of unitEntersReady).
    enum class UnitEntry { PlayedFromChain, PlayedByEffect, Token };
    /// The one decision point for "enters ready" effects, asked by every way
    /// a unit enters the board (a play resolving from the chain, an effect
    /// playing it, a token being played): the unit's own "I enter ready"
    /// (Card::entersReadyOnPlay, not for tokens), "units you play this turn
    /// enter ready" (PlayerState::units_enter_ready_this_turn), "other units
    /// enter ready" (TurnState::all_units_enter_ready), the controller's other
    /// cards' continuous effects (Card::grantsEnterReadyTo), "your units enter
    /// ready" (PlayerState::units_enter_ready) and "your tokens enter ready"
    /// (PlayerState::tokens_enter_ready). `unit` is already at its destination.
    bool unitEntersReady(PlayerId controller, const GameObject& unit,
                         UnitEntry how) const;

    // ── Legends added by effects (CR 107.4.d) ──
    /// Add the legend card `legend_def` to `player`'s Legend Zone as a
    /// non-Champion legend (CR 107.4.d.1). It enters ready. When
    /// `while_on_board_of` is set, the legend lasts only while that object is
    /// on the board (Neeko, Blending In): removeUnboundLegends takes it away
    /// once it has left. Returns the new legend's id.
    GameObjectId addLegendToZone(PlayerId player, CardDefId legend_def,
                                 GameObjectId while_on_board_of = kInvalidId);
    /// Take a non-Champion legend out of its Legend Zone. A legend added by
    /// Neeko ceases to exist (Radiance patch notes): like a token, it is
    /// parked in Banishment without joining the banishment pile. The Champion
    /// Legend can't be removed (CR 107.4.d) — a no-op for it.
    void removeLegendFromZone(GameObjectId legend);
    /// Remove every added legend whose "while on the board" source has left
    /// the board. Called at the start of each aura recalculation.
    void removeUnboundLegends();

    // ── Token creation ──
    /// Create a token game object and place it on the board.
    GameObjectId createToken(PlayerId controller, CardType type,
                             const std::string& name, int might,
                             const std::vector<std::string>& tags,
                             KeywordSet keywords, LocationId location,
                             bool enter_ready = false);

    /// Add a battlefield-token slot to the board. Creates a fresh
    /// GameObject for the BF card (card_type=Battlefield, super_type=Token,
    /// no card_def_id) and a new BattlefieldState appended to
    /// state.battlefields. Returns the new BattlefieldId. Used by Baron
    /// Nashor for its Baron Pit spawn. `accepts_any_inbound` carries the
    /// "Units can move here from anywhere" rule onto the BattlefieldState.
    BattlefieldId addBattlefieldToken(const std::string& name,
                                       bool accepts_any_inbound);

    /// Replace an existing BF in place with a named token BF (CR 438).
    /// The original BF card moves to its owner's banishment and the
    /// BattlefieldState's `card_object_id` swaps to a fresh token card
    /// with `super_type=Token`. The original is recorded in
    /// `replaced_card`/`was_replaced` so swap-back works on score.
    /// Used by Green Father to install the Brush BF token.
    /// `token_def`: the card whose abilities the token battlefield has (the
    /// Brush card for a Brush token), so they work like any battlefield's.
    void replaceBattlefieldWithToken(BattlefieldId target_bf,
                                      const std::string& token_name,
                                      PlayerId controller,
                                      CardDefId token_def = kInvalidId);

    // ── Copy effects ──
    /// Make a token become a copy of another unit (base traits only, not buffs/auras).
    void copyUnit(GameObjectId token, GameObjectId source);

    // ── Composite operations ──
    /// "If you would reveal cards from a deck, look at the top card first. You
    /// may recycle it. Then reveal those cards." (Void Hatchling; the
    /// revealer's PlayerState::has_reveal_peek). Every reveal from a deck calls
    /// this first: `revealer` looks at the top card of `deck_owner`'s Main Deck
    /// (or Rune Deck) and chooses whether to recycle it.
    void lookBeforeReveal(PlayerId revealer, PlayerId deck_owner, bool rune_deck = false);
    std::pair<GameObjectId, std::vector<GameObjectId>>
        revealUntil(PlayerId player, CardType condition);
    /// Play `card` from its current zone onto the board without paying
    /// its costs. `location` is where the unit lands: if std::nullopt
    /// (default), the controller's base — back-compat for callers that
    /// don't need to surface a location choice. Per CR 355.2.a, when
    /// effects play a unit for free the controller still chooses base
    /// or any battlefield they control; callers should prompt the
    /// agent and pass the result here (see Dazzling Aurora).
    void playIgnoringCost(PlayerId player, GameObjectId card,
                           std::optional<LocationId> location = std::nullopt);

    /// Locations `player` may choose when an effect has them play `card`
    /// (legalPlayLocations with Ambush). Card::pickPlayLocation asks the agent.
    std::vector<LocationId> playLocationsFor(PlayerId player, GameObjectId card) const {
        return legalPlayLocations(state_, card_db_, card_registry_, player, card,
                                  /*include_ambush=*/true);
    }

    using EffectPlayCost = ::riftbound::EffectPlayCost;
    /// Whether `player` could pay for `card` under `cost` right now.
    bool canPlayPayingCost(PlayerId player, GameObjectId card,
                           EffectPlayCost cost = {}) const;
    /// Play `card` by an effect that does NOT ignore all of its costs
    /// (CR 356.1.b.2): the rest is paid through the normal cost path (cost
    /// modifiers, floating pool, rune choices). Payment is optional (CR 445.2):
    /// if it can't or won't be paid the card is not played and this returns
    /// false — the caller leaves the card where it was. `card` must already be
    /// out of the zone it is played from (as with playIgnoringCost).
    bool playPayingCost(PlayerId player, GameObjectId card,
                        std::optional<LocationId> location = std::nullopt,
                        EffectPlayCost cost = {});
    /// The engine's cost path, injected so effect-driven plays pay like any
    /// other play.
    using CanAffordFn = std::function<bool(PlayerId, GameObjectId)>;
    using PayCostFn = std::function<bool(PlayerId, GameObjectId)>;
    /// Puts a spell played by an effect onto the chain so it resolves like any
    /// other spell (injected by the engine: the executor has no chain access).
    using QueueSpellFn = std::function<void(PlayerId, GameObjectId)>;
    void setQueueSpell(QueueSpellFn fn) { queue_spell_ = std::move(fn); }

    /// Put `source`'s triggered ability for `trigger` on the chain as if the
    /// event had happened ("activate the conquer effects of units here").
    /// Goes through TriggerManager::fireTrigger, so the usual suppression and
    /// trigger conditions apply. No-op without an engine (bare executor).
    using QueueTriggerFn = std::function<void(GameObjectId, PlayerId, TriggerType, GameObjectId)>;
    void setQueueTrigger(QueueTriggerFn fn) { queue_trigger_ = std::move(fn); }
    bool canQueueTriggers() const { return static_cast<bool>(queue_trigger_); }
    void queueTriggeredAbility(GameObjectId source, PlayerId controller, TriggerType trigger,
                               GameObjectId subject = kInvalidId) {
        if (queue_trigger_) queue_trigger_(source, controller, trigger, subject);
    }
    void setPlayCostFns(CanAffordFn can, PayCostFn pay) {
        can_afford_ = std::move(can);
        pay_cost_ = std::move(pay);
    }
    /// The engine's applied / additional cost path (energy + any-domain
    /// power), injected so effect moves can charge applied move costs.
    /// `pay` = false only checks. Without it, applied costs are not charged.
    using AppliedCostFn = std::function<bool(PlayerId, int energy, int power, bool pay)>;
    void setAppliedCostFn(AppliedCostFn fn) { applied_cost_ = std::move(fn); }
    /// Token play locations for `player` (CR 185.2.a + 355.2.a): their base
    /// and the battlefields they control (not a "can't be played here" one);
    /// base only under "units can only be played to your base".
    std::vector<LocationId> tokenPlayLocations(PlayerId player) const;

    /// Predict N: look at top N cards, agent chooses which to recycle (put back on bottom).
    void predict(PlayerId player, int count);

    /// Reveal top N and let agent choose: draw matching cards or recycle them.
    /// Returns list of cards the agent chose to draw/play.
    std::vector<GameObjectId> revealAndChoose(PlayerId player, int count);

    // ── Phase C-1 commit 6 — pending-choice mechanism (in progress) ──
    //
    // The traditional path (discardCards / predict / revealAndChoose above)
    // calls `agent_query_` synchronously and blocks the engine's worker
    // thread on a condvar inside StepDriver. That's what prevents Clone()
    // from being a memcpy (the worker thread can't be cloned).
    //
    // The new path: a card's onResolve calls requestChoice() to publish
    // pending legal-actions, then immediately returns. The chain manager
    // detects the pending choice, re-pushes the ChainItem with a saved
    // `resume_point`, and bubbles `StepKind::NeedDecision` up the stack.
    // When `applyChoice` is called: consume the pending choice, apply it,
    // re-call Card::onResolve which advances from its saved resume_point.
    //
    // This API is in-place but UNUSED today — cards still call the
    // blocking discardCards/predict/revealAndChoose. Refactoring them is
    // mechanical; CLAUDE.md "Phase C-1 commit 6 — resume pattern"
    // documents the per-card template.
    struct PendingChoice {
        bool active = false;
        PlayerId player = PlayerId::None;
        std::vector<Intent> legal;        // the choice set
        std::string label;                // free-text context for trace logs
                                          // (e.g. "discard 1 (Lunar Boon)").
                                          // Empty = no label printed; trace
                                          // falls back to a generic
                                          // "MakeChoice pick=[...]" line.
    };
    void requestChoice(PlayerId p, std::vector<Intent> legal,
                        std::string label = "") {
        pending_ = PendingChoice{true, p, std::move(legal), std::move(label)};
    }
    bool hasPendingChoice() const { return pending_.active; }
    PendingChoice consumePendingChoice() {
        PendingChoice tmp = std::move(pending_);
        pending_ = PendingChoice{};
        return tmp;
    }

    // The chain manager stashes the agent's answer here between
    // `consumePendingChoice` and the next Card::onResolve re-entry. The
    // resuming Card reads it in its case 1 branch via `takeChoice`, then
    // applies card-specific semantics (move discarded card to trash,
    // recycle predicted card to bottom, etc.). One-shot only — `takeChoice`
    // clears the slot.
    void recordChoice(Intent i) { last_applied_choice_ = std::move(i); }
    std::optional<Intent> takeChoice() {
        auto out = std::move(last_applied_choice_);
        last_applied_choice_.reset();
        return out;
    }
    bool hasRecordedChoice() const { return last_applied_choice_.has_value(); }

private:
    GameState& state_;
    EventBus& events_;
    const CardDB& card_db_;
    std::function<void()> aura_recalc_;
    CanAffordFn can_afford_;
    QueueSpellFn queue_spell_;
    QueueTriggerFn queue_trigger_;
    RechooseFn rechoose_;
    PayCostFn pay_cost_;
    AppliedCostFn applied_cost_;
    /// Shared body of the effect moves; `charge` = pay the applied cost here
    /// (false when moveUnitsTogether charged it for the group).
    bool effectMove(GameObjectId target, const LocationId& dest, PlayerId mover, bool charge);
    /// Ask `mover` to pay an applied move cost; false if unpaid.
    bool payMoveCost(PlayerId mover, const LocationId& dest, int unit_count);
    const CardRegistry* card_registry_ = nullptr;  // optional; needed for playIgnoringCost onPlay dispatch
    std::mt19937_64* rng_ = nullptr;
    AgentChoiceQuery agent_query_;
    PendingChoice pending_;
    std::optional<Intent> last_applied_choice_;
};

} // namespace riftbound
