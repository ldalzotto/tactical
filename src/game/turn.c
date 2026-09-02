#include "turn.h"

#include "../lib/assert.h"
#include "game/entity.h"
#include "lib/linkage.h"
#include "lib/memory.h"

PUBLIC slice_entity_ptr_t turn_order_init(linear_allocator_t *allocator) {
    slice_entity_ptr_t order;
    order = LINEAR_ALLOCATOR_PUSH(allocator, order, 0);
    return order;
}

PUBLIC void turn_order_deinit(linear_allocator_t *allocator, slice_entity_ptr_t order) {
    LINEAR_ALLOCATOR_POP(allocator, order);
}

PUBLIC void turn_order_add(linear_allocator_t *allocator, slice_entity_ptr_t *order, entity_t *entity) {
    // Same append-in-place discipline as entity_spawn: nothing else may be
    // pushed between adds.
    slice_entity_ptr_t entry = LINEAR_ALLOCATOR_PUSH_GROW(allocator, order, 1);
    SLICE_DEREF(entry) = entity;
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

PUBLIC turn_state_t turn_remove_dead_entities(turn_state_t state, slice_entity_ptr_t dead) {
    assert_debug(SLICE_TYPESIZE(state.order) > 0);

    // The active entity can never die: action_try_attack(_area) both reject
    // same-team damage, and the active entity is always the attacker's own
    // team, so it can't appear in `dead` -- even from its own AoE blast.
    entity_t *active = turn_active_entity(state);
    for ( SLICE_FOREACH(dead, dead_s) ) {
        entity_t *d = SLICE_DEREF(dead_s);
        assert_debug(!d->alive);
        assert_debug(d != active);
    }

    slice_entity_ptr_t write = state.order;
    int new_cursor = state.cursor;
    for ( SLICE_FOREACH(state.order, read) ) {
        entity_t *entity = SLICE_DEREF(read);

        bool is_dead = false;
        for ( SLICE_FOREACH(dead, dead_s) ) {
            if (SLICE_DEREF(dead_s) == entity) {
                is_dead = true;
                break;
            }
        }

        if (!is_dead) {
            if (entity == active) {
                new_cursor = (int)typesize(state.order.begin, write.begin);
            }
            SLICE_DEREF(write) = entity;
            write = SLICE_ADVANCE(write, 1);
        }
    }
    state.order.end = write.begin;

    // The whole casualty batch is reconciled in this one call, so every
    // entity left in the order is guaranteed alive once this returns.
    for ( SLICE_FOREACH(state.order, remaining) ) {
        assert_debug(SLICE_DEREF(remaining)->alive);
    }

    state.cursor = new_cursor;

    return state;
}
