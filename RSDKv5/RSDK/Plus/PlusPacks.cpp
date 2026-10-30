// =====================
// Plus: the retro games and their packs
// =====================
//
// Included by Plus.cpp (one translation unit, like the legacy cores).
//
// Each retro game can have two packs, kept in the "retro" folder next to
// Sonic Mania's Data.rsdk under fixed names:
//   - <game>-origins.rsdk: the .rsdk from Sonic Origins (Sonic1u.rsdk,
//     Sonic2u.rsdk, SonicCDu.rsdk). The game, without music or sounds;
//   - <game>-classic.rsdk: the Data.rsdk of the earlier release (the 2013
//     Sonic 1 and 2, Data.rsdk.xmf in their Android APKs; Sonic CD from 2011,
//     its mobile pack or Steam's). Music and sounds, and a playable game when
//     its scripts are the bytecode the engine runs.
// Sonic CD's 2011 packs use the original RSDKv3 format, which the engine
// cannot open: they are converted in place the first time they are found.

namespace RSDK
{
namespace Plus
{

/** What a pack file turned out to be. */
enum PackState {
    PACK_MISSING,   ///< No file.
    PACK_READY,     ///< An RSDKv3/v4 pack the legacy cores open.
    PACK_OLDFORMAT, ///< Sonic CD's 2011 format (converted when found).
    PACK_INVALID,   ///< Not a legacy pack (Sonic Mania's RSDKv5) or damaged.
};

/** One pack of a retro game. */
struct Pack {
    PackState state = PACK_MISSING;
    std::string path;        ///< Relative to the user folder ("retro/sonic1-origins.rsdk").
    char version    = 0;     ///< '3', '4' or 'B' when ready.
    bool standalone = false; ///< Ready, with bytecode the engine runs (playable alone).
};

/** A game the Plus menu offers. */
struct GameInfo {
    const char *id;    ///< "mania", "sonic1", "sonic2", "soniccd".
    const char *title; ///< Shown in the menu.
};

/** Sonic Mania's id in Plus.ini and the menu. */
const char *const kManiaId = "mania";

/** The retro games, in menu order. */
const GameInfo kRetroGames[] = {
    { "sonic1", "Sonic the Hedgehog" },
    { "sonic2", "Sonic the Hedgehog 2" },
    { "soniccd", "Sonic CD" },
};

/** A retro game and the packs found for it. */
struct RetroGame {
    GameInfo info{};
    Pack origins;
    Pack classic;

    /** An Origins pack, or a classic pack that plays on its own. */
    bool Playable() const { return origins.state == PACK_READY || (classic.state == PACK_READY && classic.standalone); }

    /** The classic pack lends music and sound effects. */
    bool HasSoundPack() const { return classic.state == PACK_READY; }

    /** One line on how the game will be played, for the menu. */
    std::string Description() const
    {
        const bool hasOrigins = origins.state == PACK_READY;
        if (hasOrigins && HasSoundPack())
            return "Sonic Origins, sound from the earlier release";
        if (hasOrigins)
            return "Sonic Origins (sound from Sonic Mania, if enabled)";
        if (HasSoundPack() && classic.standalone)
            return "Earlier release";
        if (HasSoundPack())
            return "Needs the Origins pack (this one only adds sound)";
        return "Not installed";
    }
};

/** Folder (inside the user folder) holding the retro packs, and each game's saves in "<folder>/<game>/". */
const char *const kRetroFolder = "retro";

/** Smallest file that can be a pack: a header and a file table. */
const int64 kMinimumPackBytes = 64;

/** @p relative inside the user folder (where Settings.ini and the saves are). */
std::string UserPath(const std::string &relative) { return std::string(SKU::userFileDir) + relative; }

/** "retro/sonic1-origins.rsdk" or "retro/sonic1-classic.rsdk". */
std::string PackPath(const char *gameId, bool origins) { return std::string(kRetroFolder) + "/" + gameId + (origins ? "-origins.rsdk" : "-classic.rsdk"); }

/** An fopen()ed file, closed when it goes out of scope. */
struct PackFile {
    FILE *file;
    explicit PackFile(const std::string &path, const char *mode) : file(fopen(path.c_str(), mode)) {}
    ~PackFile()
    {
        if (file)
            fclose(file);
    }
    PackFile(const PackFile &)            = delete;
    PackFile &operator=(const PackFile &) = delete;
};

int64 FileSizeOf(FILE *file)
{
    if (fseek(file, 0, SEEK_END) != 0)
        return -1;
    const int64 size = (int64)ftell(file);
    fseek(file, 0, SEEK_SET);
    return size;
}

bool ReadU8(FILE *file, uint8 *out) { return fread(out, 1, 1, file) == 1; }

bool ReadU16(FILE *file, uint16 *out)
{
    uint8 b[2];
    if (fread(b, 1, 2, file) != 2)
        return false;
    *out = (uint16)(b[0] | (b[1] << 8));
    return true;
}

bool ReadU32(FILE *file, uint32 *out)
{
    uint8 b[4];
    if (fread(b, 1, 4, file) != 4)
        return false;
    *out = (uint32)b[0] | ((uint32)b[1] << 8) | ((uint32)b[2] << 16) | ((uint32)b[3] << 24);
    return true;
}

void PutU32(uint8 *out, uint32 value)
{
    for (int32 i = 0; i < 4; ++i) out[i] = (uint8)(value >> (8 * i));
}

/**
 * The 16 bytes a hashed pack's table stores for @p path: the MD5 of the
 * lower-case path, each 32-bit word as LoadDataPack() reads it back.
 */
void PathKey(const std::string &path, uint8 key[16])
{
    char lower[0x400];
    StringLowerCase(lower, path.c_str());
    RETRO_HASH_MD5(hash);
    GEN_HASH_MD5_BUFFER(lower, hash);
    for (int32 word = 0; word < 4; ++word) {
        key[word * 4 + 0] = (uint8)(hash[word] >> 24);
        key[word * 4 + 1] = (uint8)(hash[word] >> 16);
        key[word * 4 + 2] = (uint8)(hash[word] >> 8);
        key[word * 4 + 3] = (uint8)(hash[word]);
    }
}

/** True when the hashed pack at @p packPath (a full path) lists @p filePath. */
bool PackContains(const std::string &packPath, const std::string &filePath)
{
    PackFile pack(packPath, "rb");
    uint8 header[8];
    if (!pack.file || fread(header, 1, sizeof(header), pack.file) != sizeof(header) || memcmp(header, "RSDK", 4) != 0)
        return false;

    const int32 count = header[6] | (header[7] << 8);
    uint8 key[16];
    PathKey(filePath, key);
    uint8 entry[24];
    for (int32 i = 0; i < count; ++i) {
        if (fread(entry, 1, sizeof(entry), pack.file) != sizeof(entry))
            return false;
        if (memcmp(entry, key, sizeof(key)) == 0)
            return true;
    }
    return false;
}

// ---------------------
// Sonic CD's 2011 format
// ---------------------

/** Cipher keys of the original RSDKv3 format (RSDKv3-Decompilation, Reader.cpp). */
const char kOldKeyA[] = "4RaS9D7KaEbxcp2o5r6t";
const char kOldKeyB[] = "3tRaUxLmEaSn";
/** Sanity limits for the original format's header. */
const uint32 kMaxOldDirectories = 4096;
const size_t kMaxPackFiles      = 0xFFFF; // A hashed table's count is 16 bits.

/** The original format's per-file cipher; its state carries across chunks. */
class OldV3Cipher
{
public:
    explicit OldV3Cipher(uint32 fileSize) : number((int32)((fileSize & 0x1FC) >> 2))
    {
        posB = number % 9 + 1;
        posA = number % posB + 1;
    }

    void Apply(uint8 *data, size_t size)
    {
        for (size_t i = 0; i < size; ++i) {
            uint8 d = (uint8)(data[i] ^ (uint8)kOldKeyB[posB++] ^ number);
            if (swap)
                d = (uint8)(((d & 0xF) << 4) | (d >> 4));
            d ^= (uint8)kOldKeyA[posA++];
            data[i] = d;

            if (posA <= 19 || posB <= 11) {
                if (posA > 19) {
                    posA = 1;
                    swap = !swap;
                }
                if (posB > 11) {
                    posB = 1;
                    swap = !swap;
                }
            }
            else {
                number = (number + 1) & 0x7F;
                if (swap) {
                    swap = false;
                    posA = number % 12 + 6;
                    posB = number % 5 + 4;
                }
                else {
                    swap = true;
                    posA = number % 15 + 3;
                    posB = number % 7 + 1;
                }
            }
        }
    }

private:
    int32 number;
    int32 posB = 0;
    int32 posA = 0;
    bool swap  = false;
};

/** A directory, or a file, of the original format. */
struct OldDirectory {
    std::string name; ///< With its trailing '/'.
    uint32 offset;    ///< From the end of the header.
};
struct OldFile {
    std::string path;
    uint32 dataStart; ///< Absolute offset of the encrypted data.
    uint32 size;
};

/** Reads a directory name (each byte XOR 0xFF - length). */
bool ReadDirectoryName(FILE *file, std::string *name)
{
    uint8 length = 0;
    if (!ReadU8(file, &length))
        return false;
    name->resize(length);
    if (length && fread(&(*name)[0], 1, length, file) != length)
        return false;
    for (char &c : *name) c = (char)((uint8)c ^ (uint8)(0xFF - length));
    return true;
}

/** True when the file at @p path (a full path) starts like a Sonic CD 2011 pack. */
bool LooksLikeOldV3Pack(const std::string &path)
{
    PackFile pack(path, "rb");
    if (!pack.file)
        return false;
    const int64 size  = FileSizeOf(pack.file);
    uint32 headerSize = 0;
    uint16 dirCount   = 0;
    std::string firstName;
    if (!ReadU32(pack.file, &headerSize) || !ReadU16(pack.file, &dirCount) || !ReadDirectoryName(pack.file, &firstName))
        return false;
    return headerSize >= 6 && headerSize <= size && dirCount > 0 && dirCount <= kMaxOldDirectories && firstName.compare(0, 5, "Data/") == 0;
}

/**
 * Rewrites the Sonic CD 2011 pack at @p inPath as a hashed, unencrypted
 * "RSDKv3" pack at @p outPath (full paths).
 *
 * @return False with a reason in @p error (the output is then removed).
 */
bool ConvertOldV3Pack(const std::string &inPath, const std::string &outPath, std::string *error)
{
    auto fail = [&](const char *reason) {
        *error = reason;
        remove(outPath.c_str());
        return false;
    };

    PackFile in(inPath, "rb");
    if (!in.file)
        return fail("cannot open the pack");
    const int64 inSize = FileSizeOf(in.file);

    uint32 headerSize = 0;
    uint16 dirCount   = 0;
    if (!ReadU32(in.file, &headerSize) || !ReadU16(in.file, &dirCount) || headerSize < 6 || headerSize > inSize || !dirCount
        || dirCount > kMaxOldDirectories)
        return fail("unreadable header");
    std::vector<OldDirectory> dirs;
    for (uint16 d = 0; d < dirCount; ++d) {
        OldDirectory dir;
        if (!ReadDirectoryName(in.file, &dir.name) || !ReadU32(in.file, &dir.offset) || (int64)headerSize + dir.offset > inSize)
            return fail("unreadable directory list");
        dirs.push_back(dir);
    }

    // Each directory's files: { u8 length, ~name, u32 size, data }.
    std::vector<OldFile> files;
    for (size_t d = 0; d < dirs.size(); ++d) {
        const int64 end = (int64)headerSize + (d + 1 < dirs.size() ? dirs[d + 1].offset : (uint32)(inSize - headerSize));
        int64 pos       = (int64)headerSize + dirs[d].offset;
        while (pos < end) {
            uint8 length = 0;
            uint32 size  = 0;
            std::string name;
            if (fseek(in.file, (long)pos, SEEK_SET) != 0 || !ReadU8(in.file, &length))
                return fail("damaged file entry");
            name.resize(length);
            if (length && fread(&name[0], 1, length, in.file) != length)
                return fail("damaged file name");
            for (char &c : name) c = (char)~(uint8)c;
            if (!ReadU32(in.file, &size))
                return fail("damaged file size");

            const int64 dataStart = pos + 1 + length + 4;
            if (dataStart + size > end || files.size() >= kMaxPackFiles)
                return fail("damaged directory");
            files.push_back({ dirs[d].name + name, (uint32)dataStart, size });
            pos = dataStart + size;
        }
    }
    if (files.empty())
        return fail("the pack has no files");

    PackFile out(outPath, "wb");
    if (!out.file)
        return fail("cannot create the converted pack");

    // Header and table first; the data follows in the same order.
    std::vector<uint8> table(8 + files.size() * 24);
    memcpy(table.data(), "RSDKv3", 6);
    table[6]      = (uint8)(files.size() & 0xFF);
    table[7]      = (uint8)(files.size() >> 8);
    uint64 offset = table.size();
    for (size_t i = 0; i < files.size(); ++i) {
        uint8 *entry = table.data() + 8 + i * 24;
        PathKey(files[i].path, entry);
        if (offset > 0xFFFFFFFFull)
            return fail("the converted pack would be too big");
        PutU32(entry + 16, (uint32)offset);
        PutU32(entry + 20, files[i].size & 0x7FFFFFFF); // Unencrypted.
        offset += files[i].size;
    }
    if (fwrite(table.data(), 1, table.size(), out.file) != table.size())
        return fail("cannot write the converted pack (disk full?)");

    std::vector<uint8> chunk(64 * 1024);
    for (const OldFile &file : files) {
        if (fseek(in.file, (long)file.dataStart, SEEK_SET) != 0)
            return fail("damaged data");
        OldV3Cipher cipher(file.size);
        uint32 remaining = file.size;
        while (remaining > 0) {
            const size_t take = remaining < chunk.size() ? remaining : chunk.size();
            if (fread(chunk.data(), 1, take, in.file) != take)
                return fail("truncated data");
            cipher.Apply(chunk.data(), take);
            if (fwrite(chunk.data(), 1, take, out.file) != take)
                return fail("cannot write the converted pack (disk full?)");
            remaining -= (uint32)take;
        }
    }
    if (fflush(out.file) != 0)
        return fail("cannot write the converted pack (disk full?)");
    return true;
}

// ---------------------
// Finding the packs
// ---------------------

/** Classifies the pack at @p relative (inside the user folder). */
Pack InspectPack(const std::string &relative)
{
    Pack pack;
    pack.path              = relative;
    const std::string full = UserPath(relative);
    PackFile file(full, "rb");
    if (!file.file)
        return pack;

    uint8 header[6] = {};
    const bool read = fread(header, 1, sizeof(header), file.file) == sizeof(header);
    const int64 size = FileSizeOf(file.file);
    if (!read || size < kMinimumPackBytes) {
        pack.state = PACK_INVALID;
        return pack;
    }
    if (memcmp(header, "RSDK", 4) != 0) {
        pack.state = LooksLikeOldV3Pack(full) ? PACK_OLDFORMAT : PACK_INVALID;
        return pack;
    }
    if (header[4] != 'v' || (header[5] != '3' && header[5] != '4' && header[5] != 'B')) {
        pack.state = PACK_INVALID; // 'v5' is Sonic Mania's format.
        return pack;
    }
    pack.state      = PACK_READY;
    pack.version    = (char)header[5];
    pack.standalone = PackContains(full, "Bytecode/GlobalCode.bin") || PackContains(full, "Data/Scripts/ByteCode/GlobalCode.bin");
    return pack;
}

/**
 * Converts a Sonic CD 2011 pack found at @p relative, replacing it.
 * @return The pack afterwards (invalid if the conversion failed).
 */
Pack ConvertInPlace(const std::string &relative)
{
    const std::string full      = UserPath(relative);
    const std::string converted = full + ".conv";
    std::string error;
    PrintLog(PRINT_NORMAL, "[Plus] Converting %s from Sonic CD's 2011 format...", relative.c_str());
    if (!ConvertOldV3Pack(full, converted, &error)) {
        PrintLog(PRINT_NORMAL, "[Plus] Could not convert %s: %s", relative.c_str(), error.c_str());
        Pack pack;
        pack.path  = relative;
        pack.state = PACK_INVALID;
        return pack;
    }
    remove(full.c_str());
    if (rename(converted.c_str(), full.c_str()) != 0)
        PrintLog(PRINT_NORMAL, "[Plus] Could not replace %s with its converted copy", relative.c_str());
    return InspectPack(relative);
}

/** Finds every retro game's packs (converting Sonic CD 2011 packs on the way). */
std::vector<RetroGame> ScanRetroGames()
{
    std::vector<RetroGame> games;
    for (const GameInfo &info : kRetroGames) {
        RetroGame game;
        game.info    = info;
        game.origins = InspectPack(PackPath(info.id, true));
        game.classic = InspectPack(PackPath(info.id, false));
        if (game.classic.state == PACK_OLDFORMAT)
            game.classic = ConvertInPlace(game.classic.path);
        games.push_back(game);
    }
    return games;
}

} // namespace Plus
} // namespace RSDK
