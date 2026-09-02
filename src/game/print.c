#include "./print.h"
#include "../lib/fmt.h"
#include "../lib/linkage.h"
#include "../lib/memory.h"
#include "game/entity.h"
#include "game/game.h"
#include "game/position.h"
#include "game/turn.h"
#include <stdint.h>

PUBLIC void print_position(linear_allocator_t *dest, position_t position) {
    fmt_write(dest, STR("{\"x\":"));
    fmt_write_int(dest, position.x);
    fmt_write(dest, STR(",\"y\":"));
    fmt_write_int(dest, position.y);
    fmt_write(dest, STR("}"));
    fmt_end_line(dest);
}

PUBLIC void print_skill(linear_allocator_t *dest, skill_t skill) {
    fmt_write(dest, STR("{\"range\":"));
    fmt_write_int(dest, skill.range);
    fmt_write(dest, STR(",\"damage\":"));
    fmt_write_int(dest, skill.damage);
    fmt_write(dest, STR(",\"ap_cost\":"));
    fmt_write_int(dest, skill.ap_cost);
    fmt_write(dest, STR(",\"aoe_radius\":"));
    fmt_write_int(dest, skill.aoe_radius);
    fmt_write(dest, STR("}"));
    fmt_end_line(dest);
}

// Shared by print_entity and print_entity_list -- writes just
// the field list (no surrounding braces), so the list variant can prefix
// each entity with its index.
PRIVATE void print_entity_fields(linear_allocator_t *dest, entity_t entity) {
    fmt_write(dest, STR("\"team\":\""));
    fmt_write(dest, entity.team == ENTITY_TEAM_PLAYER ? STR("player") : STR("enemy"));
    fmt_write(dest, STR("\",\"pos\":{\"x\":"));
    fmt_write_int(dest, entity.position.x);
    fmt_write(dest, STR(",\"y\":"));
    fmt_write_int(dest, entity.position.y);
    fmt_write(dest, STR("},\"hp\":"));
    fmt_write_int(dest, entity.hp);
    fmt_write(dest, STR(",\"max_hp\":"));
    fmt_write_int(dest, entity.max_hp);
    fmt_write(dest, STR(",\"ap\":"));
    fmt_write_int(dest, entity.ap);
    fmt_write(dest, STR(",\"max_ap\":"));
    fmt_write_int(dest, entity.max_ap);
    fmt_write(dest, STR(",\"mp\":"));
    fmt_write_int(dest, entity.mp);
    fmt_write(dest, STR(",\"max_mp\":"));
    fmt_write_int(dest, entity.max_mp);
    fmt_write(dest, STR(",\"alive\":"));
    fmt_write_bool(dest, entity.alive);
    fmt_write(dest, STR(",\"skill_count\":"));
    fmt_write_int(dest, entity_skill_count(&entity));
}

PUBLIC void print_entity(linear_allocator_t *dest, entity_t entity) {
    fmt_write(dest, STR("{"));
    print_entity_fields(dest, entity);
    fmt_write(dest, STR("}"));
    fmt_end_line(dest);
}

PUBLIC void print_entity_list(linear_allocator_t *dest, slice_entity_t list) {
    int index = 0;
    for (SLICE_FOREACH(list, entity_s)) {
        fmt_write(dest, STR("{\"index\":"));
        fmt_write_int(dest, index);
        fmt_write(dest, STR(","));
        print_entity_fields(dest, SLICE_DEREF(entity_s));
        fmt_write(dest, STR("}"));
        fmt_end_line(dest);
        index++;
    }
}

PUBLIC void print_turn_state(linear_allocator_t *dest, turn_state_t turn) {
    fmt_write(dest, STR("{\"cursor\":"));
    fmt_write_int(dest, turn.cursor);
    fmt_write(dest, STR(",\"order_count\":"));
    fmt_write_int(dest, (int32_t)SLICE_TYPESIZE(turn.order));
    fmt_write(dest, STR("}"));
    fmt_end_line(dest);
}

PRIVATE slice_t print_game_mode_name(game_mode_t mode) {
    switch (mode) {
        case GAME_MODE_MOVEMENT: return STR("movement");
        case GAME_MODE_ATTACK: return STR("attack");
        default: return STR("none");
    }
}

PRIVATE slice_t print_game_over_name(game_over_t game_over) {
    switch (game_over) {
        case GAME_OVER_WIN: return STR("win");
        case GAME_OVER_LOSE: return STR("lose");
        default: return STR("none");
    }
}

PUBLIC void print_game_state(linear_allocator_t *dest, game_state_t game) {
    fmt_write(dest, STR("{\"mode\":\""));
    fmt_write(dest, print_game_mode_name(game.mode));
    fmt_write(dest, STR("\",\"hover\":"));
    if (game.hover_valid) {
        fmt_write(dest, STR("{\"x\":"));
        fmt_write_int(dest, game.hover.x);
        fmt_write(dest, STR(",\"y\":"));
        fmt_write_int(dest, game.hover.y);
        fmt_write(dest, STR("}"));
    } else {
        fmt_write(dest, STR("null"));
    }
    fmt_write(dest, STR(",\"selected_skill\":"));
    fmt_write_int(dest, game.selected_skill);
    fmt_write(dest, STR(",\"game_over\":\""));
    fmt_write(dest, print_game_over_name(game.game_over));
    fmt_write(dest, STR("\",\"entity_count\":"));
    fmt_write_int(dest, (int32_t)SLICE_TYPESIZE(game.entities));
    fmt_write(dest, STR(",\"turn_cursor\":"));
    fmt_write_int(dest, game.turn.cursor);
    fmt_write(dest, STR("}"));
    fmt_end_line(dest);
}
