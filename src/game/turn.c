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

PUBLIC turn_state_t turn_remove_dead_entity(turn_state_t state, entity_t *dead) {
    assert_debug(SLICE_TYPESIZE(state.order) > 0);
    assert_debug(!dead->alive);
    // The active entity can never die: action_try_attack and
    // action_try_attack_area both reject same-team damage, and the
    // currently-active entity is always on the attacker's own team (it's
    // the one doing the attacking), so it can never appear as `dead` here --
    // including from its own AoE blast.
    entity_t *active = turn_active_entity(state);
    assert_debug(dead != active);

    slice_entity_ptr_t write = state.order;
    int new_cursor = state.cursor;
    for ( SLICE_FOREACH(state.order, read) ) {
        entity_t *entity = SLICE_DEREF(read);

        if (entity != dead) {
            if (entity == active) {
                new_cursor = (int)typesize(state.order.begin, write.begin);
            }
            SLICE_DEREF(write) = entity;
            write = SLICE_ADVANCE(write, 1);
        }
    }
    state.order.end = write.begin;

    // Unlike a single-target kill, an AoE blast can down several entities
    // at once (see action_try_attack_area): the caller reconciles them by
    // calling this function once per casualty in the same batch, so a
    // casualty not yet processed may transiently still sit in `order`,
    // already dead, between one call and the next -- no per-call "everyone
    // remaining is alive" invariant holds here anymore. The full order is
    // still checked to be entirely alive once the batch finishes (see
    // assert_game_invariants, run after every input event).

    state.cursor = new_cursor;

    return state;
}
