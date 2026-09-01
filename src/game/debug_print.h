#pragma once

#include "../lib/linkage.h"

#include "entity.h"
#include "position.h"
#include "skill.h"
#include "turn.h"
#include "game.h"

// Debug-only pretty-printers: one compact JSON object per debug line,
// streamed via lib/fmt.h straight to the runtime's debug_log bridge. For
// dropping ad hoc at a call site while debugging, not for anything the
// game depends on.
PUBLIC void debug_print_position(position_t position);
PUBLIC void debug_print_skill(skill_t skill);
PUBLIC void debug_print_entity(entity_t *entity);
// One line per entity, each prefixed with its index in `list`.
PUBLIC void debug_print_entity_list(slice_entity_t list);
PUBLIC void debug_print_turn_state(turn_state_t turn);
// Summary line only (mode, hover, turn/entity counts) -- not the grid,
// which wouldn't fit on one line and isn't skimmable as JSON anyway.
PUBLIC void debug_print_game_state(game_state_t *game);

#ifdef APP_UNITY_BUILD
#include "debug_print.c"
#endif
