#include "RetroEngine.hpp"

InputData GKeyPress = InputData();
InputData GKeyDown  = InputData();

bool anyPress = false;

InputButton inputDevice[INPUT_MAX];

// (Plus) The buttons the host says are held this frame (SetHostInput()),
// instead of the decompilation's SDL keyboard and controller reading.
bool hostHeld[INPUT_MAX];

void SetHostInput(const bool held[INPUT_ANY])
{
    for (int i = 0; i < INPUT_ANY; ++i) hostHeld[i] = held[i];
}

void ReadInputDevice()
{
    bool any = false;
    for (int i = 0; i < INPUT_ANY; ++i) {
        if (hostHeld[i]) {
            inputDevice[i].setHeld();
            any = true;
        }
        else if (inputDevice[i].hold) {
            inputDevice[i].setReleased();
        }
    }
    if (any)
        inputDevice[INPUT_ANY].setHeld();
    else
        inputDevice[INPUT_ANY].setReleased();
}

void CheckKeyPress(InputData *input, byte flags) {
    if (flags & 0x1)
        input->up = inputDevice[INPUT_UP].press;
    if (flags & 0x2)
        input->down = inputDevice[INPUT_DOWN].press;
    if (flags & 0x4)
        input->left = inputDevice[INPUT_LEFT].press;
    if (flags & 0x8)
        input->right = inputDevice[INPUT_RIGHT].press;
    if (flags & 0x10)
        input->A = inputDevice[INPUT_BUTTONA].press;
    if (flags & 0x20)
        input->B = inputDevice[INPUT_BUTTONB].press;
    if (flags & 0x40)
        input->C = inputDevice[INPUT_BUTTONC].press;
    if (flags & 0x80)
        input->start = inputDevice[INPUT_START].press;
    if (flags & 0x80)
        anyPress = inputDevice[INPUT_ANY].press;
}

void CheckKeyDown(InputData *input, byte flags) {
    if (flags & 0x1)
        input->up = inputDevice[INPUT_UP].hold;
    if (flags & 0x2)
        input->down = inputDevice[INPUT_DOWN].hold;
    if (flags & 0x4)
        input->left = inputDevice[INPUT_LEFT].hold;
    if (flags & 0x8)
        input->right = inputDevice[INPUT_RIGHT].hold;
    if (flags & 0x10)
        input->A = inputDevice[INPUT_BUTTONA].hold;
    if (flags & 0x20)
        input->B = inputDevice[INPUT_BUTTONB].hold;
    if (flags & 0x40)
        input->C = inputDevice[INPUT_BUTTONC].hold;
    if (flags & 0x80)
        input->start = inputDevice[INPUT_START].hold;
}
