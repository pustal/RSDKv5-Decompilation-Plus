
namespace SKU
{

struct InputDeviceSDL : InputDevice {
    void UpdateInput();
    void ProcessInput(int32 controllerID);
    void CloseDevice();

    int32 buttonMasks;
    int32 prevButtonMasks;
    uint8 stateUp;
    uint8 stateDown;
    uint8 stateLeft;
    uint8 stateRight;
    uint8 stateA;
    uint8 stateB;
    uint8 stateC;
    uint8 stateX;
    uint8 stateY;
    uint8 stateZ;
    uint8 stateStart;
    uint8 stateSelect;
    uint8 stateBumper_L;
    uint8 stateBumper_R;
    uint8 stateStick_L;
    uint8 stateStick_R;
    float bumperDeltaL;
    float bumperDeltaR;
    float triggerDeltaL;
    float triggerDeltaR;
    float hDelta_L;
    float vDelta_L;
    float vDelta_R;
    float hDelta_R;

    SDL_GameController *controllerPtr;
    bool32 swapABXY;
};

InputDeviceSDL *InitSDL2InputDevice(uint32 id, SDL_GameController *game_controller);

#if !RETRO_USE_ORIGINAL_CODE
// (Plus) The engine's controller family (DEVICE_XBOX, DEVICE_PS4, DEVICE_SWITCH_PRO) for the
// controller, as SDL reports it; false when SDL can't tell.
bool32 GetSDL2ControllerType(SDL_GameController *controller, uint8 *controllerType);

// (Plus) Opens the controller SDL reported at this joystick index (SDL_CONTROLLERDEVICEADDED) as
// an input device, unless the engine's XInput support already handles it.
void AddSDL2Controller(int32 joystickIndex);
// (Plus) Removes the controller with this SDL instance ID (SDL_CONTROLLERDEVICEREMOVED).
void RemoveSDL2Controller(int32 instanceID);

#if !RETRO_RENDERDEVICE_SDL2
// (Plus) With another renderer (DirectX on Windows), there is no SDL event loop: called every
// frame (ProcessInputDevices()) to update the controllers and handle connections.
void ProcessSDL2InputEvents();
#endif
#endif

void InitSDL2InputAPI();
void ReleaseSDL2InputAPI();

} // namespace SKU