#include "game.h"

// Direcional: teclado (setas/WASD), analógico esquerdo e D-pad
Vector2 Input_Move(void)
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
float Input_Orbit(void)
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
bool Input_Recenter(void)
{
    return IsKeyDown(KEY_C) || (IsGamepadAvailable(0) && IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1));
}
