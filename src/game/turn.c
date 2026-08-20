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

PUBLIC turn_state_t turn_remove_dead_entity(turn_state_t state, entity_t *dead) {
    assert_debug(SLICE_TYPESIZE(state.order) > 0);
    assert_debug(!dead->alive);
    // The active entity can't die today: action_try_attack rejects
    // same-team targets and is the only caller of entity_damage, so nothing
    // can damage whoever is currently acting. This is unreachable -- and
    // untestable through the game API -- until something (e.g. AoE damage)
    // changes that. When it does, delete this assert and uncomment the
    // survivors_before_active/passed_active bookkeeping and the cursor-park
    // block below, both marked with the same TODO.
    entity_t *active = turn_active_entity(state);
    assert_debug(dead != active);

    slice_entity_ptr_t write = state.order;
    int new_cursor = state.cursor;
    // TODO(active-entity-death): re-enable together with the block below.
    // int survivors_before_active = 0;
    // bool passed_active = false;
    for ( SLICE_FOREACH(state.order, read) ) {
        entity_t *entity = SLICE_DEREF(read);

        // TODO(active-entity-death): re-enable together with the block below.
        // if (entity == active) {
        //     passed_active = true;
        // } else if (!passed_active) {
        //     survivors_before_active++;
        // }

        if (entity != dead) {
            if (entity == active) {
                new_cursor = (int)typesize(state.order.begin, write.begin);
            }
            SLICE_DEREF(write) = entity;
            write = SLICE_ADVANCE(write, 1);
        }
    }
    state.order.end = write.begin;

    // Every entity left in the order must still be alive: callers are
    // expected to reconcile one death at a time.
    for ( SLICE_FOREACH(state.order, remaining) ) {
        assert_debug(SLICE_DEREF(remaining)->alive);
    }

    // TODO(active-entity-death): active entity died -- park the cursor one
    // slot before the survivor that followed it in the original order, so
    // the next turn_advance (which resets ap/mp) lands on that survivor
    // instead of the head. Re-enable together with the bookkeeping above
    // once dead can legitimately equal the active entity.
    // if (dead == active) {
    //     int survivor_count = (int)typesize(state.order.begin, write.begin);
    //     new_cursor = (survivor_count > 0)
    //         ? (survivors_before_active - 1 + survivor_count) % survivor_count
    //         : 0;
    // }

    state.cursor = new_cursor;

    return state;
}
