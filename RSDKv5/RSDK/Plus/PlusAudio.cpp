// =====================
// Plus: Sonic Mania's audio for the retro games
// =====================
//
// Included by Plus.cpp. The Sonic Origins packs have no audio, and even with
// the earlier releases' packs the sounds Origins added (the Drop Dash,
// Knuckles' and Amy's moves) are missing. Sonic Mania's pack has many of them
// under the same names, and remixes of several of the retro zones' music. When
// "Sonic Mania audio" is on, the game switch loads Mania's pack as the audio
// pack (LoadAudioDataPack: only Data/SoundFX/ and Data/Music/ are looked up in
// it) and the legacy audio code asks here what to play for each sound effect
// and track the game's packs lack:
//   - sound effects: Mania's file of the same name (found by the engine by
//     itself), or its nearest one (the table below);
//   - music: the Mania remix of the same zone (its act 1 or act 2 version, as
//     the act being played) or jingle, with Mania's own loop point; with
//     "similar music" on, zones Mania has no version of get a Mania track of
//     similar feel;
//   - a Mania track that only shares a name with a retro one (its title
//     screen, credits...) is a different song: silence instead, unless chosen.
// Whatever the game's own packs have wins, except with "Sonic Mania remixes"
// on: then the zones Mania remixed play the remix even when the packs (the
// earlier releases') have the original.

namespace RSDK
{
namespace Plus
{

/** A Mania sound effect standing in for a retro one (paths relative to Data/SoundFX/). */
struct SfxStandIn {
    const char *retro;
    const char *mania;
};

/**
 * Retro sound effects Mania has under another name. Those it has under the
 * same name (Global/Jump.wav, Global/DropDash.wav, Global/Grab.wav...) need
 * no entry: the engine finds them in Mania's pack by themselves.
 */
constexpr SfxStandIn kSfxStandIns[] = {
    // Amy's moves (Sonic Origins), which Mania does not have.
    { "Global/HammerJump.wav", "Global/Jump.wav" },
    { "Global/HammerHit.wav", "Stage/Whack.wav" },
    { "Global/HammerDash.wav", "Global/PeelRelease.wav" },
    // Knuckles, and the player's moves.
    { "Global/GlideDropLand.wav", "Global/Land.wav" },
    { "Global/Landing.wav", "Global/Land.wav" },
    { "Global/Rolling.wav", "Global/Roll.wav" },
    { "Global/Transform.wav", "Stage/Transform2.wav" },
    { "Global/LampPost.wav", "Global/StarPost.wav" },
    { "Global/Explosion.wav", "Stage/Explosion.wav" },
    { "Global/BonusPoints.wav", "Global/ScoreAdd.wav" },
    // Menus.
    { "Global/Select.wav", "Global/MenuAccept.wav" },
    { "Global/Resume.wav", "Global/MenuAccept.wav" },
    { "Global/MenuButton.wav", "Global/MenuBleep.wav" },
    { "Global/BackButton.wav", "Global/MenuWoosh.wav" },
    { "Stage/BackButton.wav", "Global/MenuWoosh.wav" },
    { "Global/Event.wav", "Special/Event.wav" },
    { "Stage/Event.wav", "Special/Event.wav" },
    // Sonic CD's time travel.
    { "Global/Future.wav", "SSZ1/Future.wav" },
    { "Global/Past.wav", "SSZ1/Future.wav" },
    { "Global/TimeWarp.wav", "Global/SpecialWarp.wav" },
    // Stage objects.
    { "Stage/AirAlert.wav", "Stage/DrownAlert.wav" },
    { "Stage/Drowning.wav", "Stage/Drown.wav" },
    { "Stage/Breathing.wav", "Stage/Breathe.wav" },
    { "Stage/Spikes.wav", "Global/Spike.wav" },
    { "Stage/Switch.wav", "Stage/Button.wav" },
    { "Stage/ButtonPress.wav", "Stage/Button.wav" },
    { "Stage/BlockBreak.wav", "Stage/LedgeBreak.wav" },
    { "Stage/Crumble.wav", "Stage/LedgeBreak.wav" },
    { "Stage/Bomb.wav", "Stage/Explosion.wav" },
    { "Stage/Emerald.wav", "Special/Emerald.wav" },
    { "Stage/Continue.wav", "Special/Continue.wav" },
    { "Stage/Exit_SS.wav", "Special/SSExit.wav" },
    { "Stage/GrittyGround.wav", "Special/GrittyGround.wav" },
    { "Stage/WaterSplash.wav", "Stage/Splash.wav" },
    { "Stage/WaterGeyser.wav", "HCZ/WaterGush.wav" },
    { "Stage/Launcher.wav", "Stage/Launch.wav" },
    { "Stage/FireBurn.wav", "Stage/Flame.wav" },
    { "Stage/FlameThrower.wav", "Stage/Flame2.wav" },
    { "Stage/SmallFireball.wav", "Stage/Fireball.wav" },
    { "Stage/Saw.wav", "Stage/Buzzsaw.wav" },
    { "Stage/ArrowHit.wav", "PSZ/ArrowHit.wav" },
    { "Stage/GasPop.wav", "OOZ/GasPop.wav" },
    { "Stage/Giggle.wav", "MMZ/Giggle.wav" },
    { "Stage/Elevator.wav", "FBZ/Elevator.wav" },
    { "Stage/Spear.wav", "HCZ/Spear.wav" },
    { "Stage/ChainPull.wav", "HCZ/PullChain.wav" },
    { "Stage/TargetBumper.wav", "Pinball/TargetBumper.wav" },
    { "Stage/Plunger.wav", "Pinball/Plunger.wav" },
    { "Stage/CNZBumper.wav", "Stage/Bumper.wav" },
    { "Stage/MiniBumper.wav", "Stage/Bumper2.wav" },
    { "Stage/Twinkle.wav", "Global/Twinkle.wav" },
    { "Stage/Impact1.wav", "Stage/Impact2.wav" },
    { "Stage/Electricity.wav", "Stage/Electrify.wav" },
    { "Stage/ElectricArc.wav", "Stage/ElecPulse.wav" },
    { "Stage/LaserCharge.wav", "Stage/ElecCharge.wav" },
    { "Stage/GlassSmash.wav", "Stage/WindowShatter.wav" },
    { "Stage/Thruster.wav", "Stage/RocketJet.wav" },
    { "Stage/PushBlock.wav", "Stage/Push.wav" },
    { "Stage/Clank.wav", "Stage/Clang.wav" },
    { "Stage/Door.wav", "Stage/Open.wav" },
    { "Stage/DoorClose.wav", "Stage/HullClose.wav" },
    { "Stage/Shot2.wav", "Stage/Shot.wav" },
    { "Stage/LargeBooster.wav", "Stage/SpeedBooster.wav" },
    { "Stage/SmallBooster.wav", "Stage/SpeedBooster.wav" },
    { "Stage/Crusher.wav", "MMZ/PistonLand.wav" },
    { "Stage/Jab.wav", "Stage/Whack.wav" },
    { "Stage/MechStep.wav", "LRZ/WalkerLegs.wav" },
    // Sonic CD's Metal Sonic.
    { "Stage/MSCharge.wav", "SSZ2/MSChargeFire.wav" },
    { "Stage/MSElectric.wav", "SSZ2/MSElecPulse.wav" },
    { "Stage/MSJet.wav", "SSZ2/SSBoost.wav" },
};

/** Which games a music entry applies to. */
enum StandInGames : uint32 { kSonic1 = 1, kSonic2 = 2, kSonicCd = 4, kAll = kSonic1 | kSonic2 | kSonicCd };

/**
 * A Mania track standing in for a retro one. @c retro is the retro track's
 * name without its folder ("JP/", "US/" for Sonic CD) or extension; @c loop is
 * the sample Mania loops the track at (from its scenes, or its Music object
 * for the jingles), 0 for a track that plays once.
 */
struct MusicStandIn {
    uint32 games;
    const char *retro;
    const char *mania;
    int32 loop;
    bool similar;                    ///< Only with "similar music" on: a different zone of similar feel.
    const char *maniaAct2 = nullptr; ///< Mania's act 2 version, played from the retro zone's act 2 on.
    int32 loopAct2        = 0;
    bool remix            = false;   ///< Mania remixed this zone: "Sonic Mania remixes" prefers it to the original.
};

constexpr MusicStandIn kMusicStandIns[] = {
    // Jingles and the zones Mania remixed.
    { kAll, "Invincibility", "Invincible.ogg", 139263, false },
    { kSonicCd, "SpeedShoes", "Sneakers.ogg", 120960, false },
    { kAll, "ActComplete", "ActClear.ogg", 0, false },
    { kSonicCd, "ZoneComplete", "ActClear.ogg", 0, false },
    { kAll, "Drowning", "Drowning.ogg", 0, false },
    { kAll, "GameOver", "GameOver.ogg", 0, false },
    { kSonic2, "Super", "Super.ogg", 165375, false },
    { kAll, "Boss", "BossEggman1.ogg", 282240, false },
    // Loop points from Mania's scenes (GHZ, CPZ, OOZ1/OOZ2, SSZ1/SSZ2, MMZ).
    { kSonic1, "GreenHill", "GreenHill1.ogg", 776160, false, "GreenHill2.ogg", 775552, true },
    { kSonic2, "ChemicalPlant", "ChemicalPlant1.ogg", 680400, false, "ChemicalPlant2.ogg", 680400, true },
    { kSonic2, "OilOcean", "OilOcean1.ogg", 358389, false, "OilOcean2.ogg", 176400, true },
    // Sonic CD: A present, B past, C good future, D bad future.
    { kSonicCd, "R7A", "StardustSpeedway1.ogg", 432894, false, nullptr, 0, true },
    { kSonicCd, "R7B", "StardustSpeedway1.ogg", 432894, false, nullptr, 0, true },
    { kSonicCd, "R7C", "StardustSpeedway1.ogg", 432894, false, nullptr, 0, true },
    { kSonicCd, "R7D", "StardustSpeedway2.ogg", 1, false, nullptr, 0, true },
    { kSonicCd, "R8A", "MetallicMadness1.ogg", 470400, false, nullptr, 0, true },
    { kSonicCd, "R8B", "MetallicMadness1.ogg", 470400, false, nullptr, 0, true },
    { kSonicCd, "R8C", "MetallicMadness1.ogg", 470400, false, nullptr, 0, true },
    { kSonicCd, "R8D", "MetallicMadness2.ogg", 296056, false, nullptr, 0, true },

    // Similar feel (opt-in).
    { kAll, "TitleScreen", "TitleScreen.ogg", 0, true },
    { kAll, "SpecialStage", "UFOSpecial.ogg", 288655, true },
    { kAll, "Credits", "Credits.ogg", 458752, true },
    { kSonic1, "Final", "BossFinal.ogg", 302400, true },
    { kSonic2 | kSonicCd, "FinalBoss", "BossFinal.ogg", 302400, true },
    { kSonic1, "Marble", "LavaReef1.ogg", 497329, true },
    { kSonic1, "SpringYard", "Studiopolis1.ogg", 1146600, true },
    { kSonic1, "Labyrinth", "Hydrocity1.ogg", 1, true },
    { kSonic1, "Starlight", "StardustSpeedway1.ogg", 432894, true },
    { kSonic1, "ScrapBrain", "MetallicMadness1.ogg", 470400, true },
    { kSonic2, "EmeraldHill", "GreenHill1.ogg", 776160, true },
    { kSonic2, "EmeraldHill2", "GreenHill2.ogg", 775552, true },
    { kSonic2, "AquaticRuin", "Hydrocity1.ogg", 1, true },
    { kSonic2, "CasinoNight", "Studiopolis1.ogg", 1146600, true },
    { kSonic2, "CasinoNight2", "Studiopolis2.ogg", 311142, true },
    { kSonic2, "HillTop", "AngelIsland2.ogg", 160668, true },
    { kSonic2, "MysticCave", "LavaReef1.ogg", 497329, true },
    { kSonic2, "MysticCave2", "LavaReef2.ogg", 769745, true },
    { kSonic2, "Metropolis", "MetallicMadness1.ogg", 470400, true },
    { kSonic2, "SkyChase", "FlyingBattery1.ogg", 158760, true },
    { kSonic2, "WingFortress", "FlyingBattery2.ogg", 70560, true },
    { kSonic2, "DeathEgg", "TitanicMonarch2.ogg", 384873, true },
    { kSonic2, "Options", "MainMenu.ogg", 202752, true },
    { kSonic2, "Results", "Results.ogg", 88991, true },
    { kSonicCd, "R1A", "GreenHill1.ogg", 776160, true }, // Palmtree Panic
    { kSonicCd, "R1B", "GreenHill1.ogg", 776160, true },
    { kSonicCd, "R1C", "GreenHill1.ogg", 776160, true },
    { kSonicCd, "R1D", "GreenHill2.ogg", 775552, true },
    { kSonicCd, "R3A", "Studiopolis1.ogg", 1146600, true }, // Collision Chaos
    { kSonicCd, "R3B", "Studiopolis1.ogg", 1146600, true },
    { kSonicCd, "R3C", "Studiopolis1.ogg", 1146600, true },
    { kSonicCd, "R3D", "Studiopolis2.ogg", 311142, true },
    { kSonicCd, "R4A", "Hydrocity1.ogg", 1, true }, // Tidal Tempest
    { kSonicCd, "R4B", "Hydrocity1.ogg", 1, true },
    { kSonicCd, "R4C", "Hydrocity1.ogg", 1, true },
    { kSonicCd, "R4D", "Hydrocity2.ogg", 329797, true },
    { kSonicCd, "R5A", "LavaReef1.ogg", 497329, true }, // Quartz Quadrant
    { kSonicCd, "R5B", "LavaReef1.ogg", 497329, true },
    { kSonicCd, "R5C", "LavaReef1.ogg", 497329, true },
    { kSonicCd, "R5D", "LavaReef2.ogg", 769745, true },
    { kSonicCd, "R6A", "FlyingBattery1.ogg", 158760, true }, // Wacky Workbench
    { kSonicCd, "R6B", "FlyingBattery1.ogg", 158760, true },
    { kSonicCd, "R6C", "FlyingBattery1.ogg", 158760, true },
    { kSonicCd, "R6D", "FlyingBattery2.ogg", 70560, true },
    { kSonicCd, "TimeAttack", "MainMenu.ogg", 202752, true },
};

/** Mania's speed-shoes jingle, for Sonic 1 and 2's sped-up zone tracks ("..._F.ogg"). */
constexpr MusicStandIn kSpeedShoes = { kSonic1 | kSonic2, "", "Sneakers.ogg", 120960, false };

/** Case-insensitive equality, as the engine compares pack paths. */
bool SameName(const std::string &a, const char *b)
{
    const size_t length = strlen(b);
    if (a.size() != length)
        return false;
    for (size_t i = 0; i < length; ++i) {
        if (tolower((uint8)a[i]) != tolower((uint8)b[i]))
            return false;
    }
    return true;
}

/** Copies @p text into the engine's buffer. @return False if it does not fit. */
bool CopyOut(const std::string &text, char *out, int32 outSize)
{
    if (outSize <= 0 || text.size() >= (size_t)outSize)
        return false;
    memcpy(out, text.c_str(), text.size() + 1);
    return true;
}

/** The stand-in table's bit for the retro game being played (0 for Sonic Mania). */
uint32 StandInGame()
{
    const std::string &game = CurrentGameId();
    return game == "sonic1" ? kSonic1 : game == "sonic2" ? kSonic2 : game == "soniccd" ? kSonicCd : 0;
}

/** Reports a stand-in once per name in the log. */
void LogStandIn(const char *what, const std::string &retro, const std::string &mania)
{
    static std::vector<std::string> logged;
    for (const std::string &name : logged) {
        if (name == retro)
            return;
    }
    logged.push_back(retro);
    PrintLog(PRINT_NORMAL, "[Plus] %s %s from Sonic Mania (%s)", what, retro.c_str(), mania.empty() ? "silent" : mania.c_str());
}

/** The act being played, from the stage list's id ("1", "2"...), or 1 outside a stage list. */
int32 CurrentAct()
{
    if (engine.version == 5 || !sceneInfo.listData)
        return 1;
    const int32 act = atoi(sceneInfo.listData[sceneInfo.listPos].id);
    return act > 0 ? act : 1;
}

/** The Mania track and loop point of @p entry for the act being played. */
std::pair<std::string, int32> TrackForAct(const MusicStandIn &entry)
{
    if (entry.maniaAct2 && CurrentAct() >= 2)
        return { entry.maniaAct2, entry.loopAct2 };
    return { entry.mania, entry.loop };
}

/** The music entry for retro track @p name, or null. */
const MusicStandIn *FindMusicStandIn(const std::string &name)
{
    const uint32 game = StandInGame();

    // "JP/R1A.ogg" -> "R1A"; "GreenHill_F.ogg" -> "GreenHill_F".
    std::string base         = name;
    const size_t slash       = base.find_last_of('/');
    if (slash != std::string::npos)
        base = base.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos)
        base = base.substr(0, dot);

    // Sonic 1 and 2 swap in a sped-up copy of the zone's track for speed
    // shoes; Mania plays its speed-shoes jingle instead.
    const bool fast = base.size() > 2 && SameName(base.substr(base.size() - 2), "_F");
    if (fast)
        base = base.substr(0, base.size() - 2);
    if (fast && !SameName(base, "Invincibility"))
        return (kSpeedShoes.games & game) ? &kSpeedShoes : nullptr;

    for (const MusicStandIn &entry : kMusicStandIns) {
        if ((entry.games & game) && SameName(base, entry.retro) && (!entry.similar || SimilarMusicEnabled()))
            return &entry;
    }
    return nullptr;
}

/** True when @p name is the sped-up track ("..._F.ogg") of a zone Mania remixed. */
bool RemixedZoneFast(const std::string &name)
{
    const size_t dot = name.find_last_of('.');
    std::string base = dot == std::string::npos ? name : name.substr(0, dot);
    if (base.size() <= 2 || !SameName(base.substr(base.size() - 2), "_F"))
        return false;
    const MusicStandIn *zone = FindMusicStandIn(base.substr(0, base.size() - 2) + ".ogg");
    return zone && zone->remix;
}

} // namespace Plus
} // namespace RSDK

bool32 RSDK::Plus::ResolveLegacySfx(const char *name, char *out, int32 outSize)
{
    if (!StandInGame() || !name)
        return false;
    const std::string retro = name;
    // The game's own packs, or a Mania file of the same name, need nothing.
    if (FindDataFileSource(("Data/SoundFX/" + retro).c_str()) != DATAFILE_SOURCE_NONE)
        return false;
    for (const SfxStandIn &entry : kSfxStandIns) {
        if (SameName(retro, entry.retro) && FindDataFileSource((std::string("Data/SoundFX/") + entry.mania).c_str()) == DATAFILE_SOURCE_AUDIO) {
            LogStandIn("sound", retro, entry.mania);
            return CopyOut(entry.mania, out, outSize);
        }
    }
    return false;
}

bool32 RSDK::Plus::ResolveLegacyMusic(const char *name, char *out, int32 outSize, int32 *loopPoint)
{
    if (!StandInGame() || !name || !*name)
        return false;
    const std::string retro   = name;
    const int32 source        = FindDataFileSource(("Data/Music/" + retro).c_str());
    const MusicStandIn *entry = FindMusicStandIn(retro);

    // The game's own track wins, unless it is a zone Mania remixed and the
    // remixes are preferred. A remixed zone's sped-up track (speed shoes)
    // becomes Mania's jingle too, rather than the original sped up.
    if (source == DATAFILE_SOURCE_GAME) {
        const bool remixed = entry && RemixesEnabled() && (entry->remix || entry == &kSpeedShoes);
        if (!remixed || (entry == &kSpeedShoes && !RemixedZoneFast(retro)))
            return false;
    }

    if (entry) {
        const std::pair<std::string, int32> track = TrackForAct(*entry);
        if (FindDataFileSource(("Data/Music/" + track.first).c_str()) == DATAFILE_SOURCE_AUDIO) {
            if (!CopyOut(track.first, out, outSize))
                return false;
            if (loopPoint)
                *loopPoint = track.second;
            LogStandIn("music", retro, track.first);
            return true;
        }
    }
    // A Mania track that only shares the name is a different song: play nothing.
    if (source == DATAFILE_SOURCE_AUDIO) {
        LogStandIn("music", retro, "");
        return CopyOut("", out, outSize);
    }
    return false;
}
