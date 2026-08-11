#include "turn.h"

#include "../lib/assert.h"

slice_entity_ptr_t turn_order_init(linear_allocator_t *allocator) {
    slice_entity_ptr_t order;
    order = LINEAR_ALLOCATOR_PUSH(allocator, order, 0);
    return order;
}

void turn_order_deinit(linear_allocator_t *allocator, slice_entity_ptr_t order) {
    LINEAR_ALLOCATOR_POP(allocator, order);
}

void turn_order_add(linear_allocator_t *allocator, slice_entity_ptr_t *order, entity_t *entity) {
    // Same append-in-place discipline as entity_spawn: only allowed right
    // after the previous add, with nothing else pushed in between.
    assert_debug(allocator->cursor == order->end);

    slice_entity_ptr_t entry;
    entry = LINEAR_ALLOCATOR_PUSH(allocator, entry, 1);
    SLICE_DEREF(entry) = entity;

    order->end = entry.end;
}

static void turn_reset_points(entity_t *entity) {
    entity->ap = entity->max_ap;
    entity->mp = entity->max_mp;
}

turn_state_t turn_init(slice_entity_ptr_t order) {
    turn_state_t state = { .order = order, .cursor = 0 };
    turn_reset_points(turn_active_entity(state));
    return state;
}

entity_t* turn_active_entity(turn_state_t state) {
    return SLICE_AT(state.order, state.cursor);
}

turn_state_t turn_advance(turn_state_t state) {
    int count = (int)SLICE_TYPESIZE(state.order);
    assert_debug(count > 0);
    state.cursor = (state.cursor + 1) % count;
    turn_reset_points(turn_active_entity(state));
    return state;
}

turn_state_t turn_remove_dead_entities(turn_state_t state) {
    entity_t *active = turn_active_entity(state);

    slice_entity_ptr_t write = state.order;
    for ( SLICE_FOREACH(state.order, read) ) {
        // If the entity is dead, we remove it.
        if ( SLICE_DEREF(read)->alive ) {
            SLICE_DEREF(write) = SLICE_DEREF(read);
            write = SLICE_ADVANCE(write, 1);
        }
    }
    state.order.end = write.begin;

    // reposition the cursor
    state.cursor = 0;
    for ( SLICE_FOREACH(state.order, entity_p) ) {
        if ( SLICE_DEREF(entity_p) == active ) {
            state.cursor = typesize(state.order.begin, entity_p.begin);
            break;
        }
    }

    return state;
}
