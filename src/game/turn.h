#pragma once

#include "../lib/linkage.h"

#include "entity.h"

typedef entity_t* entity_ptr_t;
SLICE_DEFINE(entity_ptr_t);

typedef struct {
    slice_entity_ptr_t order; // fixed turn sequence, authored by the scenario
    slice_entity_ptr_t capacity;
    int cursor;                // index into order of the entity currently acting
} turn_state_t;

PUBLIC slice_entity_ptr_t turn_order_init(linear_allocator_t *allocator);
PUBLIC void turn_order_deinit(linear_allocator_t *allocator, slice_entity_ptr_t order);
PUBLIC void turn_order_add(linear_allocator_t *allocator, slice_entity_ptr_t *order, entity_t *entity);

PUBLIC turn_state_t turn_init(slice_entity_ptr_t order);

PUBLIC entity_t* turn_active_entity(turn_state_t state);

// Moves the cursor to the next entry, wrapping back to the start, and resets
// the newly active entity's ap/mp.
PUBLIC turn_state_t turn_advance(turn_state_t state);

// Removes a batch of already-dead entities from the order in one pass --
// callers reconcile every casualty from a single action (e.g. an AoE blast)
// in one call, not per-casualty. None of `dead` may be the active entity
// (asserted in turn.c; see the comment there).
PUBLIC turn_state_t turn_remove_dead_entities(turn_state_t state, slice_entity_ptr_t dead);

#ifdef APP_UNITY_BUILD
#include "turn.c"
#endif
