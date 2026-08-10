#include "scenario.h"

game_state_t scenario_setup_default(linear_allocator_t* allocator, int grid_width, int grid_height, int fb_width, int fb_height, int hud_height) {
    grid_t grid = grid_init(allocator, grid_width, grid_height);
    grid_set_walkable(grid, 7, 4, false);
    grid_set_walkable(grid, 7, 5, false);

    slice_entity_t entities = entity_list_init(allocator);
    entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, 1, 2, 10, 1, 3);
    entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, 1, 5, 10, 1, 3);
    entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, 1, 8, 10, 1, 3);

    entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, 14, 2, 10, 1, 3);
    entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, 14, 5, 10, 1, 3);
    entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, 14, 8, 10, 1, 3);

    return game_init(grid, entities, fb_width, fb_height, hud_height);
}
