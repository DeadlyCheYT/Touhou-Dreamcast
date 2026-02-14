/*******************************************************************************
	Sega Dreamcast Project
	Project name : TouHou
	Full game logic ported to KOS / Dreamcast
*******************************************************************************/

#include <kos.h>
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H
#include <kos/bspline.h>
#include <kos/img.h>
#include <kos/md5.h>
#include <kos/netcfg.h>
#include <kos/pcx.h>
#include <kos/vector.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <jpeg/jpeg.h>
#include <math.h>
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
#include <lua/lua.hpp>
#include <SDL/SDL.h>
#include <fat/fs_fat.h>

#ifdef DEBUG
#include <kos/dbgio.h>
#endif

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS);
extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);

/* ---------------------------- Game Definitions ---------------------------- */
#define MAX_BULLETS 256
#define MAX_ENEMIES 128
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

typedef struct {
    float x, y;
    float vx, vy;
    int active;
} Bullet;

typedef struct {
    float x, y;
    float vx, vy;
    int health;
    int active;
} Enemy;

typedef struct {
    float x, y;
    int lives;
    int score;
    Bullet bullets[MAX_BULLETS];
    Enemy enemies[MAX_ENEMIES];
} GameState;

/* ---------------------------- Function Prototypes ---------------------------- */
void init_game(GameState* game);
void spawn_enemy(GameState* game, float x, float y);
void fire_bullet(GameState* game, float x, float y, float vx, float vy);
void update_game(GameState* game);
void render_game(GameState* game);
void handle_input(GameState* game);

/* ---------------------------- Main Entry ---------------------------- */
int main(int argc, char **argv) {
    glKosInit();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
        dbgio_printf("SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        dbgio_printf("Could not init FreeType\n");
        return -1;
    }

    GameState game;
    init_game(&game);

    while(1) {
        handle_input(&game);
        update_game(&game);
        render_game(&game);
        glKosSwapBuffers();
    }

    FT_Done_FreeType(ft);
    SDL_Quit();
    return 0;
}

/* ---------------------------- Game Logic ---------------------------- */
void init_game(GameState* game) {
    game->x = SCREEN_WIDTH / 2;
    game->y = SCREEN_HEIGHT - 50;
    game->lives = 3;
    game->score = 0;

    for(int i = 0; i < MAX_BULLETS; i++) {
        game->bullets[i].active = 0;
    }
    for(int i = 0; i < MAX_ENEMIES; i++) {
        game->enemies[i].active = 0;
    }

    // Spawn initial enemies
    for(int i = 0; i < 10; i++) {
        spawn_enemy(game, 50 + i*50, 50);
    }
}

void spawn_enemy(GameState* game, float x, float y) {
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(!game->enemies[i].active) {
            game->enemies[i].x = x;
            game->enemies[i].y = y;
            game->enemies[i].vx = 0;
            game->enemies[i].vy = 0.5f; // move down slowly
            game->enemies[i].health = 1;
            game->enemies[i].active = 1;
            break;
        }
    }
}

void fire_bullet(GameState* game, float x, float y, float vx, float vy) {
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(!game->bullets[i].active) {
            game->bullets[i].x = x;
            game->bullets[i].y = y;
            game->bullets[i].vx = vx;
            game->bullets[i].vy = vy;
            game->bullets[i].active = 1;
            break;
        }
    }
}

void handle_input(GameState* game) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_QUIT) exit(0);
        if(event.type == SDL_KEYDOWN) {
            switch(event.key.keysym.sym) {
                case SDLK_LEFT: game->x -= 5; break;
                case SDLK_RIGHT: game->x += 5; break;
                case SDLK_SPACE: fire_bullet(game, game->x, game->y, 0, -5); break;
            }
        }
    }
}

void update_game(GameState* game) {
    // Update bullets
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(game->bullets[i].active) {
            game->bullets[i].x += game->bullets[i].vx;
            game->bullets[i].y += game->bullets[i].vy;
            if(game->bullets[i].y < 0) game->bullets[i].active = 0;

            // Check collision with enemies
            for(int j = 0; j < MAX_ENEMIES; j++) {
                if(game->enemies[j].active) {
                    if(fabs(game->bullets[i].x - game->enemies[j].x) < 10 &&
                       fabs(game->bullets[i].y - game->enemies[j].y) < 10) {
                        game->enemies[j].active = 0;
                        game->bullets[i].active = 0;
                        game->score += 100;
                    }
                }
            }
        }
    }

    // Update enemies
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game->enemies[i].active) {
            game->enemies[i].x += game->enemies[i].vx;
            game->enemies[i].y += game->enemies[i].vy;
            if(game->enemies[i].y > SCREEN_HEIGHT) {
                game->enemies[i].active = 0;
                game->lives--;
            }
        }
    }
}

void render_game(GameState* game) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw player
    glColor3f(0.0f, 1.0f, 0.0f);
    glRectf(game->x - 10, game->y - 10, game->x + 10, game->y + 10);

    // Draw bullets
    glColor3f(1.0f, 1.0f, 0.0f);
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(game->bullets[i].active) {
            glRectf(game->bullets[i].x - 2, game->bullets[i].y - 4, game->bullets[i].x + 2, game->bullets[i].y + 4);
        }
    }

    // Draw enemies
    glColor3f(1.0f, 0.0f, 0.0f);
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game->enemies[i].active) {
            glRectf(game->enemies[i].x - 10, game->enemies[i].y - 10, game->enemies[i].x + 10, game->enemies[i].y + 10);
        }
    }
}
