#pragma once

#include "entity.h"

typedef entity_t* entity_ptr_t;
SLICE_DEFINE(entity_ptr_t);

typedef struct {
    slice_entity_ptr_t order; // fixed turn sequence, authored by the scenario
    int cursor;                // index into order of the entity currently acting
} turn_state_t;

slice_entity_ptr_t turn_order_init(linear_allocator_t *allocator);
void turn_order_deinit(linear_allocator_t *allocator, slice_entity_ptr_t order);
void turn_order_add(linear_allocator_t *allocator, slice_entity_ptr_t *order, entity_t *entity);

turn_state_t turn_init(slice_entity_ptr_t order);

entity_t* turn_active_entity(turn_state_t state);

// Moves the cursor to the next entry, wrapping back to the start, and resets
// the newly active entity's ap/mp.
turn_state_t turn_advance(turn_state_t state);

turn_state_t turn_remove_dead_entities(turn_state_t state);
