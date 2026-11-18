#ifndef RSDKV2_INPUT_H
#define RSDKV2_INPUT_H

enum InputButtons {
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_BUTTONA,
    INPUT_BUTTONB,
    INPUT_BUTTONC,
    INPUT_START,
    INPUT_ANY,
    INPUT_MAX,
};

struct InputData {
    bool up;
    bool down;
    bool left;
    bool right;
    bool A;
    bool B;
    bool C;
    bool start;
};

struct InputButton {
    bool press, hold;
    int keyMappings, contMappings;

    inline void setHeld()
    {
        press = !hold;
        hold  = true;
    }
    inline void setReleased()
    {
        press = false;
        hold  = false;
    }

    inline bool down() { return (press || hold); }
};

extern InputData GKeyPress;
extern InputData GKeyDown;
extern bool anyPress;

extern InputButton inputDevice[INPUT_MAX];

/** (Plus) The buttons held this frame, indexed by InputButtons (up to INPUT_START), from the host. */
void SetHostInput(const bool held[INPUT_ANY]);
void ReadInputDevice();

void CheckKeyPress(InputData *input, byte Flags);
void CheckKeyDown(InputData *input, byte Flags);

#endif // !RSDKV2_INPUT_H
