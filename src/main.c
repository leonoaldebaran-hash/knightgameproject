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

// Um coração: dois círculos e um triângulo
static void DrawHeartShape(float x, float y, float size, Color c)
{
    DrawCircle((int)(x - size*0.25f), (int)y, size*0.3f, c);
    DrawCircle((int)(x + size*0.25f), (int)y, size*0.3f, c);
    DrawTriangle((Vector2){ x - size*0.55f, y + size*0.1f }, (Vector2){ x, y + size*0.7f }, (Vector2){ x + size*0.55f, y + size*0.1f }, c);
}

// Vida em corações, com meio coração quando o valor é ímpar
static void DrawHearts(int hp, int maxHp, float x, float y)
{
    const float size = 22.0f, gap = 26.0f;
    for (int i = 0; i < maxHp/2; i++)
    {
        float cx = x + i*gap;
        int filled = hp - i*2;   // 2 = cheio, 1 = metade, 0 ou menos = vazio

        DrawHeartShape(cx, y, size, (Color){ 70, 40, 40, 255 });
        if (filled >= 2) DrawHeartShape(cx, y, size, RED);
        else if (filled == 1)
        {
            BeginScissorMode((int)(cx - size), (int)(y - size), (int)size, (int)(size*2.0f));
                DrawHeartShape(cx, y, size, RED);
            EndScissorMode();
        }
    }
}

static void UpdateDrawFrame(void)
{
    float dt = GetFrameTime();
    if (dt > 0.1f) dt = 0.1f;   // evita "teleportes" quando a aba fica em segundo plano

    Player_Update(&player, gameCamera.yaw, dt);
    Soldiers_Update(&player, dt);
    Dummies_Update(dt);
    GameCamera_Update(&gameCamera, &player, dt);
    Music_Update(Soldiers_Engaged(), dt);

    BeginDrawing();
    ClearBackground((Color){ 120, 170, 220, 255 });

    BeginMode3D(gameCamera.cam);
        World_Draw();
        Dummies_Draw();
        Soldiers_Draw();
        Player_Draw(&player);
    EndMode3D();

    DrawText("Knight Game Project - Tarefa 5 + som", 10, 10, 20, RAYWHITE);
    DrawText("Andar: setas/WASD ou analogico | Camera: Q/E ou analogico direito | Centralizar: C ou L1", 10, 34, 10, RAYWHITE);
    DrawText("Espada: J ou B (aperte 3x para o combo) | Escudo: segure K ou R1", 10, 48, 10, RAYWHITE);
    DrawText(TextFormat("Acertos: %d", Dummies_TotalHits()), SCREEN_W - 110, 10, 20, RAYWHITE);
    DrawHearts(player.hp, player.maxHp, 24.0f, 74.0f);

    if (player.fallenTimer > 0.0f)
    {
        const char *msg = "Voce caiu! Tente de novo.";
        DrawText(msg, (SCREEN_W - MeasureText(msg, 30))/2, SCREEN_H/2 - 60, 30, RED);
    }
    DrawFPS(10, SCREEN_H - 30);
    DrawText("versao " GAME_VERSION, SCREEN_W - 110, SCREEN_H - 20, 10, RAYWHITE);
    EndDrawing();
}

int main(void)
{
    InitWindow(SCREEN_W, SCREEN_H, "Knight Game Project");
    InitAudioDevice();
    Sfx_Init();
    Music_Init();

    World_Init();
    Dummies_Init();
    Soldiers_Init();
    Player_Init(&player);
    GameCamera_Init(&gameCamera);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) UpdateDrawFrame();
#endif

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
