#include "RetroEngine.hpp"

bool engineDebugMode = false;

RetroEngine Engine = RetroEngine();

inline int getLowerRate(int intendRate, int targetRate) {
    int result   = 0;
    int valStore = 0;

    result = targetRate;
    if (intendRate) {
        do {
            valStore   = result % intendRate;
            result     = intendRate;
            intendRate = valStore;
        } while (valStore);
    }
    return result;
}

// (Plus) The decompilation's processEvents() (the window's events and the
// developer keys) is gone: the host has its own window and developer menu.

bool RetroEngine::Init() {
    CalculateTrigAngles();
    // (Plus) No settings.ini (InitUserdata()): the host sets dataFile and the screen's width.
    CheckBinFile(dataFile);

    GameMode    = ENGINE_EXITGAME;
    GameRunning = false;
    if (LoadGameConfig("Data/Game/GameConfig.bin")) {
        if (InitRenderDevice()) {
            if (InitSoundDevice()) {
                InitSystemMenu();
                ClearScriptData();
                initialised = true;
                GameRunning = true;
            }
        }
    }

    // Calculate Skip frame
    int lower        = getLowerRate(targetRefreshRate, refreshRate);
    renderFrameIndex = targetRefreshRate / lower;
    skipFrameIndex   = refreshRate / lower;
    return GameRunning;
}

// (Plus) One pass of the decompilation's Run() loop; the host paces the frames.
void RetroEngine::RunFrame() {
    if (!GameRunning)
        return;

    for (int s = 0; s < gameSpeed; ++s) {
        ReadInputDevice();

        if (!masterPaused || frameStep) {
            switch (GameMode) {
                case ENGINE_SYSMENU:
                    ProcessSystemMenu();
                    FlipScreen();
                    break;
                case ENGINE_MAINGAME: ProcessStage(); break;
                case ENGINE_INITSYSMENU:
                    LoadGameConfig("Data/Game/GameConfig.bin");
                    InitSystemMenu();
                    ResetCurrentStageFolder();
                    break;
                case ENGINE_EXITGAME: GameRunning = false; break;
                default: break;
            }
        }
    }

    frameStep = false;
}

// (Plus) What Run() did after its loop, without the window and settings.
void RetroEngine::Release() {
    ReleaseSoundDevice();
    ReleaseRenderDevice();
    initialised = false;
    GameRunning = false;
}

bool RetroEngine::LoadGameConfig(const char *filePath) {
    FileInfo info;
    byte fileBuffer  = 0;
    byte fileBuffer2 = 0;
    char data[0x40];
    char strBuf[0x80];
    byte strLen = 0;

    if (LoadFile(filePath, &info)) {
        FileRead(&fileBuffer, 1);
        FileRead(GameWindowText, fileBuffer);
        GameWindowText[fileBuffer] = 0;

        FileRead(&fileBuffer, 1);
        FileRead(&data, fileBuffer); // Load 'Data'
        data[fileBuffer] = 0;

        FileRead(&fileBuffer, 1);
        FileRead(GameDescriptionText, fileBuffer);
        GameDescriptionText[fileBuffer] = 0;

        // Read Script Paths
        byte scriptCount = 0;
        FileRead(&scriptCount, 1);
        for (byte s = 0; s < scriptCount; ++s) {
            FileRead(&fileBuffer, 1);
            for (byte i = 0; i < fileBuffer; ++i) FileRead(&fileBuffer2, 1);
        }

        byte varCount = 0;
        FileRead(&varCount, 1);
        NO_GLOBALVARIABLES = varCount;
        for (byte v = 0; v < varCount; ++v) {
            // Read Variable Name
            FileRead(&fileBuffer, 1);
            FileRead(&GlobalVariableNames[v], fileBuffer);
            GlobalVariableNames[v][fileBuffer] = 0;

            // Read Variable Value
            FileRead(&fileBuffer2, 1);
            GlobalVariables[v] = fileBuffer2 << 24;
            FileRead(&fileBuffer2, 1);
            GlobalVariables[v] += fileBuffer2 << 16;
            FileRead(&fileBuffer2, 1);
            GlobalVariables[v] += fileBuffer2 << 8;
            FileRead(&fileBuffer2, 1);
            GlobalVariables[v] += fileBuffer2;
        }

        // Read SFX
        byte sfxCount = 0;
        FileRead(&sfxCount, 1);
        for (byte s = 0; s < sfxCount; ++s) {
            FileRead(&fileBuffer, 1);
            for (byte i = 0; i < fileBuffer; ++i) FileRead(&fileBuffer2, 1);
        }

        // Read Player Names
        byte playerCount = 0;
        FileRead(&playerCount, 1);
        for (byte p = 0; p < playerCount; ++p) {
            // Player Anim
            FileRead(&strLen, 1);
            FileRead(&strBuf, strLen);
            strBuf[strLen] = 0;
            // Player Script
            FileRead(&strLen, 1);
            FileRead(&strBuf, strLen);
            strBuf[strLen] = 0;
            // Player Name
            FileRead(&strLen, 1);
            FileRead(&strBuf, strLen);
            strBuf[strLen] = 0;
        }

        for (int c = 0; c < 4; ++c) {
            // Special Stages are stored as cat 2 in file, but cat 3 in game :(
            int cat = c;
            if (c == 2)
                cat = 3;
            else if (c == 3)
                cat = 2;
            FileRead(&fileBuffer, 1);
            stageListCount[cat] = fileBuffer;
            for (int s = 0; s < stageListCount[cat]; ++s) {
                // Read Stage Folder
                FileRead(&fileBuffer, 1);
                FileRead(&stageList[cat][s].folder, fileBuffer);
                stageList[cat][s].folder[fileBuffer] = 0;

                // Read Stage ID
                FileRead(&fileBuffer, 1);
                FileRead(&stageList[cat][s].id, fileBuffer);
                stageList[cat][s].id[fileBuffer] = 0;

                // Read Stage Name
                FileRead(&fileBuffer, 1);
                FileRead(&stageList[cat][s].name, fileBuffer);
                stageList[cat][s].name[fileBuffer] = 0;

                // Read Stage Mode
                FileRead(&fileBuffer, 1);
                stageList[cat][s].highlighted = fileBuffer;
            }
        }

#if !RETRO_USE_ORIGINAL_CODE
        if (strlen(Engine.startSceneFolder) && strlen(Engine.startSceneID)) {
            SceneInfo *scene = &stageList[STAGELIST_BONUS][0xFE]; // slot 0xFF is used for "none" startStage
            strcpy(scene->name, "_RSDK_SCENE");
            strcpy(scene->folder, Engine.startSceneFolder);
            strcpy(scene->id, Engine.startSceneID);
            startList_Game  = STAGELIST_BONUS;
            startStage_Game = 0xFE;
        }
#endif

        CloseFile();
        return true;
    }

    return false;
}