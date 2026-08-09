#include "scenario.h"

void scenario_setup_default(game_state_t *game) {
    grid_set_walkable(game->grid, 7, 4, false);
    grid_set_walkable(game->grid, 7, 5, false);

    entity_spawn(&game->entities, ENTITY_TEAM_PLAYER, 1, 2, 10, 1, 3);
    entity_spawn(&game->entities, ENTITY_TEAM_PLAYER, 1, 5, 10, 1, 3);
    entity_spawn(&game->entities, ENTITY_TEAM_PLAYER, 1, 8, 10, 1, 3);

    entity_spawn(&game->entities, ENTITY_TEAM_ENEMY, 14, 2, 10, 1, 3);
    entity_spawn(&game->entities, ENTITY_TEAM_ENEMY, 14, 5, 10, 1, 3);
    entity_spawn(&game->entities, ENTITY_TEAM_ENEMY, 14, 8, 10, 1, 3);
}
