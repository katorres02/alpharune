#pragma once
/// @file keyword_abilities.h
/// Engine-owned ability objects for triggered abilities that come from a
/// KEYWORD rather than from printed card text (ChainItem::keyword_ability).
/// The engine queues them on the chain like any triggered ability (so players
/// can respond) and resolves them through these objects, which use the same
/// choice helpers as cards. They are not registered in the CardRegistry and
/// are not in the CardDB.
#include "cards/card.h"
#include "core/game_state.h"

namespace riftbound {

/// The ability object for `kind`, or nullptr for KeywordAbility::None.
Card* keywordAbilityCard(KeywordAbility kind);

/// Total [Disarm] Value of a unit (CR 831.2: printed + granted, summed).
int totalDisarmValue(const GameState& state, const Card* card, const GameObject& unit);

}  // namespace riftbound
