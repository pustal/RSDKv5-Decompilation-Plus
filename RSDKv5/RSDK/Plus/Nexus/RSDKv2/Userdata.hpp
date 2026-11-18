#ifndef RSDKV2_USERDATA_H
#define RSDKV2_USERDATA_H

#define GLOBALVAR_COUNT (0x100)

extern int NO_GLOBALVARIABLES;
extern int GlobalVariables[GLOBALVAR_COUNT];
extern char GlobalVariableNames[GLOBALVAR_COUNT][0x20];

inline int GetGlobalVariableByName(const char *name)
{
    for (int v = 0; v < NO_GLOBALVARIABLES; ++v) {
        if (StrComp(name, GlobalVariableNames[v]))
            return GlobalVariables[v];
    }
    return 0;
}

inline void SetGlobalVariableByName(const char *name, int value)
{
    for (int v = 0; v < NO_GLOBALVARIABLES; ++v) {
        if (StrComp(name, GlobalVariableNames[v])) {
            GlobalVariables[v] = value;
            break;
        }
    }
}


#endif //!RSDKV2_USERDATA_H
