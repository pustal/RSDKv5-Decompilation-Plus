
using namespace RSDK;

#define NORMALIZE(val, minVal, maxVal) ((float)(val) - (float)(minVal)) / ((float)(maxVal) - (float)(minVal))

bool32 getControllerButton(RSDK::SKU::InputDeviceSDL *device, uint8 buttonID)
{
    if (buttonID == (uint8)SDL_CONTROLLER_BUTTON_INVALID || !device)
        return false;

    if (SDL_GameControllerGetButton(device->controllerPtr, (SDL_GameControllerButton)buttonID))
        return true;

    return false;
}

void RSDK::SKU::InputDeviceSDL::UpdateInput()
{
    int32 buttonMap[] = {
        SDL_CONTROLLER_BUTTON_DPAD_UP,   SDL_CONTROLLER_BUTTON_DPAD_DOWN,  SDL_CONTROLLER_BUTTON_DPAD_LEFT,    SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_A,         SDL_CONTROLLER_BUTTON_B,          SDL_CONTROLLER_BUTTON_INVALID,      SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_Y,         SDL_CONTROLLER_BUTTON_INVALID,    SDL_CONTROLLER_BUTTON_START,        SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    };

    int32 keyMasks[] = { KEYMASK_UP, KEYMASK_DOWN, KEYMASK_LEFT,  KEYMASK_RIGHT,  KEYMASK_A,      KEYMASK_B,      KEYMASK_C,       KEYMASK_X,
                         KEYMASK_Y,  KEYMASK_Z,    KEYMASK_START, KEYMASK_SELECT, KEYMASK_STICKL, KEYMASK_STICKR, KEYMASK_BUMPERL, KEYMASK_BUMPERR };

    this->prevButtonMasks = this->buttonMasks;

    float prevHDeltaL       = hDelta_L;
    float prevVDeltaL       = vDelta_L;
    float prevHDeltaR       = hDelta_R;
    float prevVDeltaR       = vDelta_R;
    float prevTriggerDeltaL = triggerDeltaL;
    float prevTriggerDeltaR = triggerDeltaR;

    this->buttonMasks = 0;
    for (int32 i = 0; i < KEY_MAX + 4; ++i) {
        if (getControllerButton(this, buttonMap[i]))
            this->buttonMasks |= keyMasks[i];
    }

    int32 delta = SDL_GameControllerGetAxis(controllerPtr, SDL_CONTROLLER_AXIS_LEFTX);
    if (delta < 0)
        hDelta_L = -NORMALIZE(-delta, 1, 32768);
    else
        hDelta_L = NORMALIZE(delta, 0, 32767);

    delta = SDL_GameControllerGetAxis(controllerPtr, SDL_CONTROLLER_AXIS_LEFTY);
    if (delta < 0)
        vDelta_L = -NORMALIZE(-delta, 1, 32768);
    else
        vDelta_L = NORMALIZE(delta, 0, 32767);
    vDelta_L = -vDelta_L;

    delta = SDL_GameControllerGetAxis(controllerPtr, SDL_CONTROLLER_AXIS_RIGHTX);
    if (delta < 0)
        hDelta_R = -NORMALIZE(-delta, 1, 32768);
    else
        hDelta_R = NORMALIZE(delta, 0, 32767);

    delta = SDL_GameControllerGetAxis(controllerPtr, SDL_CONTROLLER_AXIS_RIGHTY);
    if (delta < 0)
        vDelta_R = -NORMALIZE(-delta, 1, 32768);
    else
        vDelta_R = NORMALIZE(delta, 0, 32767);
    vDelta_R = -vDelta_R;

    triggerDeltaL = NORMALIZE(SDL_GameControllerGetAxis(controllerPtr, SDL_CONTROLLER_AXIS_TRIGGERLEFT), 0, 32767);
    triggerDeltaR = NORMALIZE(SDL_GameControllerGetAxis(controllerPtr, SDL_CONTROLLER_AXIS_TRIGGERRIGHT), 0, 32767);

    bumperDeltaL = (this->buttonMasks & KEYMASK_BUMPERL) != 0 ? 1.0 : 0.0;
    bumperDeltaR = (this->buttonMasks & KEYMASK_BUMPERR) != 0 ? 1.0 : 0.0;

    int32 changedButtons = ~this->prevButtonMasks & (this->prevButtonMasks ^ this->buttonMasks);

    if (changedButtons || hDelta_L != prevHDeltaL || vDelta_L != prevVDeltaL || hDelta_R != prevHDeltaR || vDelta_R != prevVDeltaR
        || triggerDeltaL != prevTriggerDeltaL || triggerDeltaR != prevTriggerDeltaR) {
        this->inactiveTimer[0] = 0;
        this->anyPress         = true;
    }
    else {
        ++this->inactiveTimer[0];
        this->anyPress = false;
    }

    if ((changedButtons & KEYMASK_A) || (changedButtons & KEYMASK_START))
        this->inactiveTimer[1] = 0;
    else
        ++this->inactiveTimer[1];

    this->stateUp       = (this->buttonMasks & KEYMASK_UP) != 0;
    this->stateDown     = (this->buttonMasks & KEYMASK_DOWN) != 0;
    this->stateLeft     = (this->buttonMasks & KEYMASK_LEFT) != 0;
    this->stateRight    = (this->buttonMasks & KEYMASK_RIGHT) != 0;
    this->stateA        = (this->buttonMasks & (swapABXY ? KEYMASK_B : KEYMASK_A)) != 0;
    this->stateB        = (this->buttonMasks & (swapABXY ? KEYMASK_A : KEYMASK_B)) != 0;
    this->stateC        = (this->buttonMasks & KEYMASK_C) != 0;
    this->stateX        = (this->buttonMasks & (swapABXY ? KEYMASK_Y : KEYMASK_X)) != 0;
    this->stateY        = (this->buttonMasks & (swapABXY ? KEYMASK_X : KEYMASK_Y)) != 0;
    this->stateZ        = (this->buttonMasks & KEYMASK_Z) != 0;
    this->stateStart    = (this->buttonMasks & KEYMASK_START) != 0;
    this->stateSelect   = (this->buttonMasks & KEYMASK_SELECT) != 0;
    this->stateBumper_L = (this->buttonMasks & KEYMASK_BUMPERL) != 0;
    this->stateBumper_R = (this->buttonMasks & KEYMASK_BUMPERR) != 0;
    this->stateStick_L  = (this->buttonMasks & KEYMASK_STICKL) != 0;
    this->stateStick_R  = (this->buttonMasks & KEYMASK_STICKR) != 0;

    ProcessInput(CONT_ANY);
}

void RSDK::SKU::InputDeviceSDL::ProcessInput(int32 controllerID)
{
    controller[controllerID].keyUp.press |= this->stateUp;
    controller[controllerID].keyDown.press |= this->stateDown;
    controller[controllerID].keyLeft.press |= this->stateLeft;
    controller[controllerID].keyRight.press |= this->stateRight;
    controller[controllerID].keyA.press |= this->stateA;
    controller[controllerID].keyB.press |= this->stateB;
    controller[controllerID].keyC.press |= this->stateC;
    controller[controllerID].keyX.press |= this->stateX;
    controller[controllerID].keyY.press |= this->stateY;
    controller[controllerID].keyZ.press |= this->stateZ;
    controller[controllerID].keyStart.press |= this->stateStart;
    controller[controllerID].keySelect.press |= this->stateSelect;

#if RETRO_REV02
    stickL[controllerID].keyStick.press |= this->stateStick_L;
    stickL[controllerID].hDelta = this->hDelta_L;
    stickL[controllerID].vDelta = this->vDelta_L;
    stickL[controllerID].keyUp.press |= this->vDelta_L > INPUT_DEADZONE;
    stickL[controllerID].keyDown.press |= this->vDelta_L < -INPUT_DEADZONE;
    stickL[controllerID].keyLeft.press |= this->hDelta_L < -INPUT_DEADZONE;
    stickL[controllerID].keyRight.press |= this->hDelta_L > INPUT_DEADZONE;

    stickR[controllerID].keyStick.press |= this->stateStick_R;
    stickR[controllerID].hDelta = this->vDelta_R;
    stickR[controllerID].vDelta = this->hDelta_R;
    stickR[controllerID].keyUp.press |= this->vDelta_R > INPUT_DEADZONE;
    stickR[controllerID].keyDown.press |= this->vDelta_R < -INPUT_DEADZONE;
    stickR[controllerID].keyLeft.press |= this->hDelta_R < -INPUT_DEADZONE;
    stickR[controllerID].keyRight.press |= this->hDelta_R > INPUT_DEADZONE;

    triggerL[controllerID].keyBumper.press |= this->stateBumper_L;
    triggerL[controllerID].keyTrigger.press |= this->triggerDeltaL > INPUT_DEADZONE;
    triggerL[controllerID].bumperDelta  = this->bumperDeltaL;
    triggerL[controllerID].triggerDelta = this->triggerDeltaL;

    triggerR[controllerID].keyBumper.press |= this->stateBumper_R;
    triggerR[controllerID].keyTrigger.press |= this->triggerDeltaR > INPUT_DEADZONE;
    triggerR[controllerID].bumperDelta  = this->bumperDeltaR;
    triggerR[controllerID].triggerDelta = this->triggerDeltaR;
#else
    controller[controllerID].keyStickL.press |= this->stateStick_L;
    stickL[controllerID].hDeltaL = this->hDelta_L;
    stickL[controllerID].vDeltaL = this->vDelta_L;
    stickL[controllerID].keyUp.press |= this->vDelta_L > INPUT_DEADZONE;
    stickL[controllerID].keyDown.press |= this->vDelta_L < -INPUT_DEADZONE;
    stickL[controllerID].keyLeft.press |= this->hDelta_L < -INPUT_DEADZONE;
    stickL[controllerID].keyRight.press |= this->hDelta_L > INPUT_DEADZONE;

    controller[controllerID].keyStickR.press |= this->stateStick_R;
    stickL[controllerID].hDeltaR = this->vDelta_R;
    stickL[controllerID].vDeltaR = this->hDelta_R;

    controller[controllerID].keyBumperL.press |= this->stateBumper_L;
    controller[controllerID].keyTriggerL.press |= this->triggerDeltaL > INPUT_DEADZONE;
    stickL[controllerID].triggerDeltaL = this->triggerDeltaL;

    controller[controllerID].keyBumperR.press |= this->stateBumper_R;
    controller[controllerID].keyTriggerR.press |= this->triggerDeltaR > INPUT_DEADZONE;
    stickL[controllerID].triggerDeltaR = this->triggerDeltaR;
#endif
}

void RSDK::SKU::InputDeviceSDL::CloseDevice()
{
    this->active     = false;
    this->isAssigned = false;
    SDL_GameControllerClose(this->controllerPtr);
    this->controllerPtr = NULL;
}

RSDK::SKU::InputDeviceSDL *RSDK::SKU::InitSDL2InputDevice(uint32 id, SDL_GameController *game_controller)
{
    if (inputDeviceCount >= INPUTDEVICE_COUNT)
        return NULL;

    if (inputDeviceList[inputDeviceCount] && inputDeviceList[inputDeviceCount]->active)
        return NULL;

    if (inputDeviceList[inputDeviceCount])
        delete inputDeviceList[inputDeviceCount];

    inputDeviceList[inputDeviceCount] = new InputDeviceSDL();

    InputDeviceSDL *device = (InputDeviceSDL *)inputDeviceList[inputDeviceCount];

    device->controllerPtr = game_controller;

    device->swapABXY     = false;
    uint8 controllerType = DEVICE_XBOX;

    const char *name = SDL_GameControllerName(device->controllerPtr);

#if !RETRO_USE_ORIGINAL_CODE
    // (Plus) SDL knows the family of most controllers; the names below are the fallback for the
    // ones it can't tell (the Wii U's and the Vita's built-in controllers, Saturn pads, ...).
    bool32 typeKnown = false;
    if (GetSDL2ControllerType(device->controllerPtr, &controllerType)) {
        typeKnown        = true;
        device->swapABXY = controllerType == DEVICE_SWITCH_PRO;
    }

    if (!typeKnown && name != NULL) {
#else
    if (name != NULL) {
#endif
        if (strstr(name, "Xbox"))
            controllerType = DEVICE_XBOX;
        else if (strstr(name, "Playstation") || strstr(name, "PS3") || strstr(name, "PS4") || strstr(name, "PS5") || strstr(name, "Vita"))
            controllerType = DEVICE_PS4;
        // (Plus) The Wii U's controllers have Nintendo's button layout too
        // (wiiu-sdl2 names them "Wii U Gamepad", "Wii U Pro Controller",
        // "Wii Remote" and "Wii Classic Controller").
        else if (strstr(name, "Switch") || strstr(name, "Wii U") || strstr(name, "Wii Remote") || strstr(name, "Wii Classic")) {
            controllerType   = DEVICE_SWITCH_PRO;
            device->swapABXY = true;
        }
        else if (strstr(name, "Saturn"))
            controllerType = DEVICE_SATURN;
    }

    device->active      = true;
    device->disabled    = false;
    device->gamepadType = (DEVICE_API_SDL2 << 16) | (DEVICE_TYPE_CONTROLLER << 8) | (controllerType << 0);
    device->id          = id;

    for (int32 i = 0; i < PLAYER_COUNT; ++i) {
        if (inputSlots[i] == id) {
            inputSlotDevices[i] = device;
            device->isAssigned  = true;
        }
    }

    inputDeviceCount++;
    return device;
}

#if !RETRO_USE_ORIGINAL_CODE
bool32 RSDK::SKU::GetSDL2ControllerType(SDL_GameController *controller, uint8 *controllerType)
{
#if SDL_VERSION_ATLEAST(2, 0, 12)
    switch (SDL_GameControllerGetType(controller)) {
        case SDL_CONTROLLER_TYPE_PS3:
        case SDL_CONTROLLER_TYPE_PS4:
#if SDL_VERSION_ATLEAST(2, 0, 14)
        case SDL_CONTROLLER_TYPE_PS5:
#endif
            *controllerType = DEVICE_PS4;
            return true;

        // Nintendo's layout (A on the right, B at the bottom): the Pro Controller, the Joy-Cons
        // (single or as a pair) and the Switch Online controllers.
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
#if SDL_VERSION_ATLEAST(2, 24, 0)
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
#endif
            *controllerType = DEVICE_SWITCH_PRO;
            return true;

        case SDL_CONTROLLER_TYPE_XBOX360:
        case SDL_CONTROLLER_TYPE_XBOXONE:
#if SDL_VERSION_ATLEAST(2, 0, 16)
        case SDL_CONTROLLER_TYPE_AMAZON_LUNA:
        case SDL_CONTROLLER_TYPE_GOOGLE_STADIA:
#endif
#if SDL_VERSION_ATLEAST(2, 24, 0)
        case SDL_CONTROLLER_TYPE_NVIDIA_SHIELD:
#endif
            *controllerType = DEVICE_XBOX;
            return true;

        default: break;
    }
#endif
    return false;
}

#if RETRO_INPUTDEVICE_XINPUT
// Whether the engine's XInput support already handles this controller (an XInput device:
// Xbox controllers and the many controllers that act as one), so SDL must leave it alone.
static bool32 IsXInputController(int32 joystickIndex)
{
    SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(joystickIndex);
    if (guid.data[14] == 'x') // SDL 2.24+: the driver's signature
        return true;
    if (memcmp(guid.data, "xinput", 6) == 0) // older SDL versions
        return true;

#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_GameControllerType type = SDL_GameControllerTypeForIndex(joystickIndex);
    return type == SDL_CONTROLLER_TYPE_XBOX360 || type == SDL_CONTROLLER_TYPE_XBOXONE;
#else
    return false;
#endif
}
#endif

void RSDK::SKU::AddSDL2Controller(int32 joystickIndex)
{
#if RETRO_INPUTDEVICE_XINPUT
    if (IsXInputController(joystickIndex))
        return;
#endif

    SDL_GameController *gameController = SDL_GameControllerOpen(joystickIndex);
    if (gameController == NULL)
        return;

    uint32 id;
    char idBuffer[0x20];
    sprintf_s(idBuffer, sizeof(idBuffer), "SDLDevice%d", SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gameController)));
    GenerateHashCRC(&id, idBuffer);

    if (InitSDL2InputDevice(id, gameController) == NULL) {
        SDL_GameControllerClose(gameController);
        return;
    }

    const char *name = SDL_GameControllerName(gameController);
    PrintLog(PRINT_NORMAL, "Controller connected: %s", name ? name : "(unnamed)");
}

void RSDK::SKU::RemoveSDL2Controller(int32 instanceID)
{
    uint32 id;
    char idBuffer[0x20];
    sprintf_s(idBuffer, sizeof(idBuffer), "SDLDevice%d", instanceID);
    GenerateHashCRC(&id, idBuffer);

    RemoveInputDevice(InputDeviceFromID(id));
}

#if !RETRO_RENDERDEVICE_SDL2
void RSDK::SKU::ProcessSDL2InputEvents()
{
    // Updates the controllers' state and queues their connection events.
    SDL_PumpEvents();

    // Only controller events reach this queue without an SDL window; anything else is dropped.
    SDL_Event events[16];
    int32 count;
    while ((count = SDL_PeepEvents(events, 16, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) > 0) {
        for (int32 e = 0; e < count; ++e) {
            switch (events[e].type) {
                case SDL_CONTROLLERDEVICEADDED: AddSDL2Controller(events[e].cdevice.which); break;
                case SDL_CONTROLLERDEVICEREMOVED: RemoveSDL2Controller(events[e].cdevice.which); break;
                default: break;
            }
        }
    }
}
#endif
#endif // !RETRO_USE_ORIGINAL_CODE

void RSDK::SKU::InitSDL2InputAPI()
{
#if !RETRO_USE_ORIGINAL_CODE
    // (Plus) Buttons by position, not by the letter printed on them: SDL's "A" is always the bottom
    // face button. Nintendo controllers then get their own layout through swapABXY, as above
    // (with labels, SDL would already have swapped them, and swapABXY would undo it).
#ifdef SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
#endif

#if !RETRO_RENDERDEVICE_SDL2
    // Without an SDL window, SDL finds controllers being plugged in on a thread of its own.
    SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
#endif

#if RETRO_INPUTDEVICE_RAWINPUT
    // The engine registers for the Raw Input of gamepads itself, and Windows keeps only one
    // registration per process: SDL's would take it away.
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
#endif

#if RETRO_INPUTDEVICE_XINPUT
    // Xbox controllers stay with the engine's XInput support (see IsXInputController()).
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "0");
#endif
#endif

    SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC);

    char path[0x100]; 
    sprintf_s(path, sizeof(path), "%sgamecontrollerdb.txt", SKU::userFileDir);
    SDL_GameControllerAddMappingsFromFile(path);
}

void RSDK::SKU::ReleaseSDL2InputAPI() { SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC); }
