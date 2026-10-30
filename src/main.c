#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#define SCREEN_W 640
#define SCREEN_H 480

// Movimento
#define RUN_SPEED   6.0f   // velocidade máxima (unidades por segundo)
#define ACCEL      30.0f   // aceleração ao começar a andar
#define DECEL      40.0f   // freio ao soltar o direcional
#define TURN_SPEED 12.0f   // rapidez para virar na direção do movimento
#define FIELD_SIZE 19.0f   // limite do campo

// Câmera
#define CAM_DISTANCE    7.0f
#define CAM_HEIGHT      3.5f
#define CAM_ORBIT_SPEED 2.5f   // radianos por segundo
#define CAM_FOLLOW      6.0f   // suavidade ao seguir o jogador
#define CAM_RECENTER    8.0f   // rapidez para voltar para trás do jogador

static Camera3D camera = { 0 };
static Vector3 playerPos = { 0 };
static Vector3 playerVel = { 0 };
static float playerAngle = 0.0f;
static float camYaw = 0.0f;
static Vector3 camFocus = { 0.0f, 1.0f, 0.0f };

// Deixa o ângulo entre -PI e PI, para girar sempre pelo caminho mais curto
static float WrapAngle(float a)
{
    while (a > PI) a -= 2.0f*PI;
    while (a < -PI) a += 2.0f*PI;
    return a;
}

// Direcional: teclado (setas/WASD), analógico esquerdo e D-pad
static Vector2 ReadMoveInput(void)
{
    Vector2 input = { 0 };
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) input.x += 1.0f;
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) input.x -= 1.0f;
    if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) input.y += 1.0f;
    if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) input.y -= 1.0f;
    if (IsGamepadAvailable(0))
    {
        input.x += GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        input.y += GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) input.x += 1.0f;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))  input.x -= 1.0f;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN))  input.y += 1.0f;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP))    input.y -= 1.0f;
    }
    if (Vector2Length(input) < 0.15f) return (Vector2){ 0 };   // zona morta do analógico
    if (Vector2Length(input) > 1.0f) input = Vector2Normalize(input);
    return input;
}

// Girar a câmera: Q/E ou analógico direito
static float ReadOrbitInput(void)
{
    float orbit = 0.0f;
    if (IsKeyDown(KEY_Q)) orbit -= 1.0f;
    if (IsKeyDown(KEY_E)) orbit += 1.0f;
    if (IsGamepadAvailable(0))
    {
        float rx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
        if (fabsf(rx) > 0.2f) orbit += rx;
    }
    return Clamp(orbit, -1.0f, 1.0f);
}

// Centralizar a câmera atrás do jogador: C ou L1
static bool ReadRecenterInput(void)
{
    return IsKeyDown(KEY_C) || (IsGamepadAvailable(0) && IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1));
}

static void UpdatePlayer(float dt)
{
    Vector2 move = ReadMoveInput();

    // O direcional é relativo à câmera: "cima" anda para onde a câmera olha
    Vector3 forward = { -sinf(camYaw), 0.0f, -cosf(camYaw) };
    Vector3 right = { cosf(camYaw), 0.0f, -sinf(camYaw) };
    Vector3 wishDir = Vector3Add(Vector3Scale(right, move.x), Vector3Scale(forward, -move.y));

    // Aceleração e freio curtos dão "peso" ao personagem
    Vector3 targetVel = Vector3Scale(wishDir, RUN_SPEED);
    float rate = (Vector2Length(move) > 0.0f) ? ACCEL : DECEL;
    playerVel = Vector3MoveTowards(playerVel, targetVel, rate*dt);
    playerPos = Vector3Add(playerPos, Vector3Scale(playerVel, dt));
    playerPos.x = Clamp(playerPos.x, -FIELD_SIZE, FIELD_SIZE);
    playerPos.z = Clamp(playerPos.z, -FIELD_SIZE, FIELD_SIZE);

    // Vira suavemente para a direção em que está andando
    if (Vector3Length(playerVel) > 0.1f)
    {
        float targetAngle = atan2f(playerVel.x, playerVel.z);
        playerAngle = WrapAngle(playerAngle + WrapAngle(targetAngle - playerAngle)*fminf(1.0f, TURN_SPEED*dt));
    }
}

static void UpdateCamera3rdPerson(float dt)
{
    camYaw -= ReadOrbitInput()*CAM_ORBIT_SPEED*dt;
    if (ReadRecenterInput()) camYaw += WrapAngle(playerAngle + PI - camYaw)*fminf(1.0f, CAM_RECENTER*dt);
    camYaw = WrapAngle(camYaw);

    // O foco segue o jogador com um pequeno atraso
    Vector3 focusTarget = { playerPos.x, 1.0f, playerPos.z };
    camFocus = Vector3Lerp(camFocus, focusTarget, fminf(1.0f, CAM_FOLLOW*dt));

    camera.target = camFocus;
    camera.position = (Vector3){ camFocus.x + sinf(camYaw)*CAM_DISTANCE,
                                 camFocus.y + CAM_HEIGHT,
                                 camFocus.z + cosf(camYaw)*CAM_DISTANCE };
}

// Mateo provisório, desenhado já virado para a direção em que anda
static void DrawPlayer(void)
{
    // Sombra redonda no chão, como no N64
    DrawCylinder((Vector3){ playerPos.x, 0.01f, playerPos.z }, 0.45f, 0.45f, 0.01f, 16, Fade(BLACK, 0.3f));

    rlPushMatrix();
        rlTranslatef(playerPos.x, 0.0f, playerPos.z);
        rlRotatef(playerAngle*RAD2DEG, 0.0f, 1.0f, 0.0f);
        DrawCube((Vector3){ 0.0f, 0.6f, 0.0f }, 0.6f, 1.2f, 0.4f, (Color){ 40, 70, 160, 255 });   // corpo
        DrawSphere((Vector3){ 0.0f, 1.45f, 0.0f }, 0.3f, (Color){ 240, 200, 90, 255 });         // cabeça loira
        DrawCube((Vector3){ 0.4f, 0.7f, 0.1f }, 0.1f, 0.6f, 0.5f, LIGHTGRAY);                     // escudo (braço esquerdo)
        DrawCube((Vector3){ -0.4f, 0.7f, 0.5f }, 0.06f, 0.06f, 0.9f, WHITE);                      // espada (braço direito)
    rlPopMatrix();
}

static void DrawWorld(void)
{
    DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 40.0f, 40.0f }, (Color){ 90, 140, 70, 255 });

    // Anel de pilares de pedra para dar noção de espaço ao girar a câmera
    for (int i = 0; i < 8; i++)
    {
        float a = i*(2.0f*PI/8.0f);
        Vector3 p = { sinf(a)*12.0f, 1.5f, cosf(a)*12.0f };
        DrawCube(p, 1.0f, 3.0f, 1.0f, GRAY);
        DrawCubeWires(p, 1.0f, 3.0f, 1.0f, DARKGRAY);
    }

    // Uma árvore simples no centro
    DrawCylinder((Vector3){ 4.0f, 0.0f, -4.0f }, 0.3f, 0.3f, 2.0f, 8, BROWN);
    DrawSphere((Vector3){ 4.0f, 2.6f, -4.0f }, 1.2f, DARKGREEN);
}

static void UpdateDrawFrame(void)
{
    float dt = GetFrameTime();

    UpdatePlayer(dt);
    UpdateCamera3rdPerson(dt);

    BeginDrawing();
    ClearBackground((Color){ 120, 170, 220, 255 });

    BeginMode3D(camera);
        DrawWorld();
        DrawPlayer();
    EndMode3D();

    DrawText("Knight Game Project - Tarefa 2", 10, 10, 20, RAYWHITE);
    DrawText("Andar: setas/WASD ou analogico | Camera: Q/E ou analogico direito | Centralizar: C ou L1", 10, 34, 10, RAYWHITE);
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
