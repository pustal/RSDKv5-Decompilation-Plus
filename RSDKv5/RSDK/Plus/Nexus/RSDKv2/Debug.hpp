#ifndef RSDKV2_DEBUG_H
#define RSDKV2_DEBUG_H

void PrintLog(const char *msg, ...);

enum DevMenuMenus {
    DEVMENU_MAIN,
    DEVMENU_PLAYERSEL,
    DEVMENU_STAGELISTSEL,
    DEVMENU_STAGESEL,
    DEVMENU_MODMENU,
};

void InitSystemMenu();
void ProcessSystemMenu();

#endif //!RSDKV2_DEBUG_H
