#include "turn.h"

#include "../lib/assert.h"

PUBLIC slice_entity_ptr_t turn_order_init(linear_allocator_t *allocator) {
    slice_entity_ptr_t order;
    order = LINEAR_ALLOCATOR_PUSH(allocator, order, 0);
    return order;
}

PUBLIC void turn_order_deinit(linear_allocator_t *allocator, slice_entity_ptr_t order) {
    LINEAR_ALLOCATOR_POP(allocator, order);
}

PUBLIC void turn_order_add(linear_allocator_t *allocator, slice_entity_ptr_t *order, entity_t *entity) {
    // Same append-in-place discipline as entity_spawn: only allowed right
    // after the previous add, with nothing else pushed in between.
    assert_debug(allocator->cursor == order->end);

    slice_entity_ptr_t entry;
    entry = LINEAR_ALLOCATOR_PUSH(allocator, entry, 1);
    SLICE_DEREF(entry) = entity;

    order->end = entry.end;
}

PRIVATE void turn_reset_points(entity_t *entity) {
    entity->ap = entity->max_ap;
    entity->mp = entity->max_mp;
}

PUBLIC turn_state_t turn_init(slice_entity_ptr_t order) {
    turn_state_t state = { .order = order, .capacity = order, .cursor = 0 };
    turn_reset_points(turn_active_entity(state));
    return state;
}

PUBLIC entity_t* turn_active_entity(turn_state_t state) {
    return SLICE_AT(state.order, state.cursor);
}

PUBLIC turn_state_t turn_advance(turn_state_t state) {
    int count = (int)SLICE_TYPESIZE(state.order);
    assert_debug(count > 0);
    state.cursor = (state.cursor + 1) % count;
    turn_reset_points(turn_active_entity(state));
    return state;
}

PUBLIC turn_state_t turn_remove_dead_entities(turn_state_t state) {
    entity_t *active = turn_active_entity(state);

    slice_entity_ptr_t write = state.order;
    int new_cursor = 0;
    bool active_seen = false;
    for ( SLICE_FOREACH(state.order, read) ) {
        entity_t *entity = SLICE_DEREF(read);
        // If the entity is dead, we remove it.
        if ( entity->alive ) {
            if (entity == active) {
                new_cursor = (int)typesize(state.order.begin, write.begin);
                active_seen = true;
            }
            SLICE_DEREF(write) = entity;
            write = SLICE_ADVANCE(write, 1);
        }
    }
    state.order.end = write.begin;

    // The active entity just acted (the caller only removes a *defender*
    // killed by that action), so it is alive and still present in the
    // compacted order. Repositioning the cursor without a search also keeps
    // this path free of an empty-slice loop that can never run.
    assert_debug(active_seen);
    state.cursor = new_cursor;

    return state;
}
