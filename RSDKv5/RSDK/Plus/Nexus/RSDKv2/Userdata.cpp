#include "RetroEngine.hpp"

int NO_GLOBALVARIABLES;
int GlobalVariables[GLOBALVAR_COUNT];
char GlobalVariableNames[GLOBALVAR_COUNT][0x20];

// (Plus) No settings.ini: InitUserdata() and WriteSettings() are gone (the host sets the data file and screen width).
