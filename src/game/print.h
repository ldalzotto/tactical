#pragma once

#include "../lib/linkage.h"
#include "../lib/memory.h"

#include "entity.h"
#include "position.h"
#include "skill.h"
#include "turn.h"
#include "game.h"

// Debug-only pretty-printers: one compact JSON object per debug line. See
// fmt.h for what `dest` means -- NULL streams to the runtime debug bridge
// (the normal ad hoc call-site-while-debugging use), non-NULL pushes the
// JSON onto that allocator instead (e.g. for a test to capture and assert
// on the exact output). Not for anything the game depends on.
PUBLIC void print_position(linear_allocator_t *dest, position_t position);
PUBLIC void print_skill(linear_allocator_t *dest, skill_t skill);
PUBLIC void print_entity(linear_allocator_t *dest, entity_t entity);
// One line per entity, each prefixed with its index in `list`.
PUBLIC void print_entity_list(linear_allocator_t *dest, slice_entity_t list);
PUBLIC void print_turn_state(linear_allocator_t *dest, turn_state_t turn);
// Summary line only (mode, hover, turn/entity counts) -- not the grid,
// which wouldn't fit on one line and isn't skimmable as JSON anyway.
PUBLIC void print_game_state(linear_allocator_t *dest, game_state_t game);

#ifdef APP_UNITY_BUILD
#include "print.c"
#endif
