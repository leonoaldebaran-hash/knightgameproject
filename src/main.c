#include "raylib.h"
#include "raymath.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#define SCREEN_W 640
#define SCREEN_H 480

static Camera3D camera = { 0 };
static Vector3 playerPos = { 0.0f, 0.0f, 0.0f };
static float playerAngle = 0.0f;

static void UpdateDrawFrame(void)
{
    float dt = GetFrameTime();

    // Entrada: teclado (setas/WASD) ou controle
    Vector2 input = { 0 };
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) input.x += 1.0f;
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) input.x -= 1.0f;
    if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) input.y += 1.0f;
    if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) input.y -= 1.0f;
    if (IsGamepadAvailable(0))
    {
        input.x += GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        input.y += GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);

        // Direcional (D-pad) do R36S
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) input.x += 1.0f;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))  input.x -= 1.0f;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN))  input.y += 1.0f;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP))    input.y -= 1.0f;
    }
    if (Vector2Length(input) > 1.0f) input = Vector2Normalize(input);

    // Movimento do jogador
    const float speed = 5.0f;
    playerPos.x += input.x*speed*dt;
    playerPos.z += input.y*speed*dt;
    if (Vector2Length(input) > 0.1f) playerAngle = atan2f(input.x, input.y);

    // Câmera em terceira pessoa seguindo o jogador
    camera.target = (Vector3){ playerPos.x, playerPos.y + 1.0f, playerPos.z };
    camera.position = (Vector3){ playerPos.x, playerPos.y + 4.0f, playerPos.z + 7.0f };

    BeginDrawing();
    ClearBackground((Color){ 120, 170, 220, 255 });

    BeginMode3D(camera);
        DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 40.0f, 40.0f }, (Color){ 90, 140, 70, 255 });

        // Pilares de pedra para dar noção de espaço
        for (int i = -2; i <= 2; i++)
        {
            DrawCube((Vector3){ i*6.0f, 1.5f, -8.0f }, 1.0f, 3.0f, 1.0f, GRAY);
            DrawCubeWires((Vector3){ i*6.0f, 1.5f, -8.0f }, 1.0f, 3.0f, 1.0f, DARKGRAY);
        }

        // Mateo provisório: corpo, cabeça loira e escudo na frente
        DrawCube((Vector3){ playerPos.x, 0.6f, playerPos.z }, 0.6f, 1.2f, 0.4f, (Color){ 40, 70, 160, 255 });
        DrawSphere((Vector3){ playerPos.x, 1.45f, playerPos.z }, 0.3f, (Color){ 240, 200, 90, 255 });
        Vector3 front = { playerPos.x + sinf(playerAngle)*0.4f, 0.7f, playerPos.z + cosf(playerAngle)*0.4f };
        DrawCube(front, 0.3f, 0.5f, 0.3f, LIGHTGRAY);
    EndMode3D();

    DrawText("Knight Game Project - Tarefa 1", 10, 10, 20, RAYWHITE);
    DrawFPS(10, SCREEN_H - 30);
    EndDrawing();
}

int main(void)
{
    InitWindow(SCREEN_W, SCREEN_H, "Knight Game Project");

    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) UpdateDrawFrame();
#endif

    CloseWindow();
    return 0;
}
