#include "game.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// Versão do build (o GitHub Actions preenche com o código do commit)
#ifndef GAME_VERSION
#define GAME_VERSION "dev"
#endif

static Player player;
static GameCamera gameCamera;

static void UpdateDrawFrame(void)
{
    float dt = GetFrameTime();
    if (dt > 0.1f) dt = 0.1f;   // evita "teleportes" quando a aba fica em segundo plano

    Player_Update(&player, gameCamera.yaw, dt);
    GameCamera_Update(&gameCamera, &player, dt);

    BeginDrawing();
    ClearBackground((Color){ 120, 170, 220, 255 });

    BeginMode3D(gameCamera.cam);
        World_Draw();
        Player_Draw(&player);
    EndMode3D();

    DrawText("Knight Game Project - Tarefa 3", 10, 10, 20, RAYWHITE);
    DrawText("Andar: setas/WASD ou analogico | Camera: Q/E ou analogico direito | Centralizar: C ou L1", 10, 34, 10, RAYWHITE);
    DrawFPS(10, SCREEN_H - 30);
    DrawText("versao " GAME_VERSION, SCREEN_W - 110, SCREEN_H - 20, 10, RAYWHITE);
    EndDrawing();
}

int main(void)
{
    InitWindow(SCREEN_W, SCREEN_H, "Knight Game Project");

    World_Init();
    Player_Init(&player);
    GameCamera_Init(&gameCamera);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) UpdateDrawFrame();
#endif

    CloseWindow();
    return 0;
}
