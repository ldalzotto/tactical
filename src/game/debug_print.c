#include "./debug_print.h"
#include "../lib/fmt.h"
#include "../lib/linkage.h"
#include "../lib/memory.h"

PUBLIC void debug_print_position(position_t position) {
    fmt_write(STR("{\"x\":"));
    fmt_write_int(position.x);
    fmt_write(STR(",\"y\":"));
    fmt_write_int(position.y);
    fmt_write(STR("}"));
    fmt_end_line();
}

PUBLIC void debug_print_skill(skill_t skill) {
    fmt_write(STR("{\"range\":"));
    fmt_write_int(skill.range);
    fmt_write(STR(",\"damage\":"));
    fmt_write_int(skill.damage);
    fmt_write(STR(",\"ap_cost\":"));
    fmt_write_int(skill.ap_cost);
    fmt_write(STR(",\"aoe_radius\":"));
    fmt_write_int(skill.aoe_radius);
    fmt_write(STR("}"));
    fmt_end_line();
}

// Shared by debug_print_entity and debug_print_entity_list -- writes just
// the field list (no surrounding braces), so the list variant can prefix
// each entity with its index.
PRIVATE void debug_print_entity_fields(entity_t *entity) {
    fmt_write(STR("\"team\":\""));
    fmt_write(entity->team == ENTITY_TEAM_PLAYER ? STR("player") : STR("enemy"));
    fmt_write(STR("\",\"pos\":{\"x\":"));
    fmt_write_int(entity->position.x);
    fmt_write(STR(",\"y\":"));
    fmt_write_int(entity->position.y);
    fmt_write(STR("},\"hp\":"));
    fmt_write_int(entity->hp);
    fmt_write(STR(",\"max_hp\":"));
    fmt_write_int(entity->max_hp);
    fmt_write(STR(",\"ap\":"));
    fmt_write_int(entity->ap);
    fmt_write(STR(",\"max_ap\":"));
    fmt_write_int(entity->max_ap);
    fmt_write(STR(",\"mp\":"));
    fmt_write_int(entity->mp);
    fmt_write(STR(",\"max_mp\":"));
    fmt_write_int(entity->max_mp);
    fmt_write(STR(",\"alive\":"));
    fmt_write_bool(entity->alive);
    fmt_write(STR(",\"skill_count\":"));
    fmt_write_int(entity_skill_count(entity));
}

PUBLIC void debug_print_entity(entity_t *entity) {
    fmt_write(STR("{"));
    debug_print_entity_fields(entity);
    fmt_write(STR("}"));
    fmt_end_line();
}

PUBLIC void debug_print_entity_list(slice_entity_t list) {
    int index = 0;
    for (SLICE_FOREACH(list, entity_s)) {
        fmt_write(STR("{\"index\":"));
        fmt_write_int(index);
        fmt_write(STR(","));
        debug_print_entity_fields(&SLICE_DEREF(entity_s));
        fmt_write(STR("}"));
        fmt_end_line();
        index++;
    }
}

PUBLIC void debug_print_turn_state(turn_state_t turn) {
    fmt_write(STR("{\"cursor\":"));
    fmt_write_int(turn.cursor);
    fmt_write(STR(",\"order_count\":"));
    fmt_write_int((int32_t)SLICE_TYPESIZE(turn.order));
    fmt_write(STR("}"));
    fmt_end_line();
}

PRIVATE slice_t debug_print_game_mode_name(game_mode_t mode) {
    switch (mode) {
        case GAME_MODE_MOVEMENT: return STR("movement");
        case GAME_MODE_ATTACK: return STR("attack");
        default: return STR("none");
    }
}

PRIVATE slice_t debug_print_game_over_name(game_over_t game_over) {
    switch (game_over) {
        case GAME_OVER_WIN: return STR("win");
        case GAME_OVER_LOSE: return STR("lose");
        default: return STR("none");
    }
}

PUBLIC void debug_print_game_state(game_state_t *game) {
    fmt_write(STR("{\"mode\":\""));
    fmt_write(debug_print_game_mode_name(game->mode));
    fmt_write(STR("\",\"hover\":"));
    if (game->hover_valid) {
        fmt_write(STR("{\"x\":"));
        fmt_write_int(game->hover.x);
        fmt_write(STR(",\"y\":"));
        fmt_write_int(game->hover.y);
        fmt_write(STR("}"));
    } else {
        fmt_write(STR("null"));
    }
    fmt_write(STR(",\"selected_skill\":"));
    fmt_write_int(game->selected_skill);
    fmt_write(STR(",\"game_over\":\""));
    fmt_write(debug_print_game_over_name(game->game_over));
    fmt_write(STR("\",\"entity_count\":"));
    fmt_write_int((int32_t)SLICE_TYPESIZE(game->entities));
    fmt_write(STR(",\"turn_cursor\":"));
    fmt_write_int(game->turn.cursor);
    fmt_write(STR("}"));
    fmt_end_line();
}
