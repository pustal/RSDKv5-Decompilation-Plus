// =====================
// Plus: Sonic Origins' videos for the retro games
// =====================
//
// Included by Plus.cpp, after PlusOrigins.cpp.
//
// Sonic Origins keeps its cutscenes in .usm files (CRIWARE Sofdec2): VP9
// video in an IVF stream, and HCA sound encrypted with the key vgmstream
// lists for Sonic Origins. The engine plays Theora video (Graphics/Video.cpp),
// so "Convert Sonic Origins videos" (the Plus menu's Settings, or a port's
// own page) decodes the VP9 (libvpx, dependencies/all/libvpx), scales it down
// and encodes it as Theora in Ogg, and stores the sound as an HCA file without
// its encryption, which the engine plays (Audio/HcaAudio.hpp). One pack per
// game, "<game>-origins-video.rsdk", next to the "origins" folder the .usm
// files are in (the same folder as the audio's .acb and .awb files).
//
// What the packs hold, and when the videos play:
//   - Sonic CD's own: its scripts ask for Opening, Good_Ending and Bad_Ending
//     (from soniccd_op, cd_ed_good and cd_ed_bad);
//   - Sonic Origins' animated intro and outro of each game (hite_<game>_intro,
//     hite_<game>_outro), which the scripts know nothing of: the intro plays
//     when a new game starts (a zone's first act loaded from the title, the
//     save screen or Sonic CD's menu), the outro before the credits that
//     follow the ending (PlayCutsceneBeforeLoad());
//   - the Sonic Team logo (sonicteam_logo), before a game's first title screen.
// A video's sound is "Data/Music/Video/<name>.hca": OnVideoStarted() plays it
// on the stream channel, and the video keeps time with it.
//
// Converting needs libvpx and Theora's encoder, which the CMake builds have
// (PLUS_VIDEO_CONVERTER); the packs it writes play in any build.

#ifndef PLUS_VIDEO_CONVERTER
#define PLUS_VIDEO_CONVERTER 0
#endif

#if PLUS_VIDEO_CONVERTER
#include <theora/theoraenc.h>
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
#endif

namespace RSDK
{
namespace Plus
{

/** "<game>-origins-video.rsdk". */
std::string OriginsVideoPackName(const char *gameId) { return std::string(gameId) + "-origins-video.rsdk"; }

/** Sonic Origins' key for the sound of its .usm files (vgmstream, src/meta/hca_keys.h: "Sonic Origins (multi)"). */
const uint64 kOriginsMovieKey = 1991062320220623ull;

/** Suffix of a video's second sound track ("Opening_US.hca"): Sonic CD's American soundtrack. */
const char *const kSecondTrackSuffix = "_US";
/** Sonic CD's soundtrack option (0 Japanese, 1 American). */
const char *const kCdSoundtrackGlobal = "Options.Soundtrack";

/** The names the cutscenes Plus plays itself have in the packs (Data/Video/<name>.ogv). */
const char *const kCutsceneIntro = "OriginsIntro";
const char *const kCutsceneOutro = "OriginsOutro";
const char *const kCutsceneLogo  = "OriginsLogo";

/** A video of a game's pack: the .usm it is made from and the name it goes under. */
struct OriginsVideo {
    const char *usm;  ///< Without extension.
    const char *name; ///< Data/Video/<name>.ogv, Data/Music/Video/<name>.hca.
};

/** A game's videos. */
struct OriginsVideoGame {
    const char *id;
    std::vector<OriginsVideo> videos;
};

const std::vector<OriginsVideoGame> &OriginsVideoGames()
{
    static const std::vector<OriginsVideoGame> games = {
        { "sonic1", { { "hite_sonic1_intro", kCutsceneIntro }, { "hite_sonic1_outro", kCutsceneOutro }, { "sonicteam_logo", kCutsceneLogo } } },
        { "sonic2", { { "hite_sonic2_intro", kCutsceneIntro }, { "hite_sonic2_outro", kCutsceneOutro }, { "sonicteam_logo", kCutsceneLogo } } },
        { "soniccd",
          { { "soniccd_op", "Opening" },
            { "cd_ed_good", "Good_Ending" },
            { "cd_ed_bad", "Bad_Ending" },
            { "hite_soniccd_intro", kCutsceneIntro },
            { "hite_soniccd_outro", kCutsceneOutro },
            { "sonicteam_logo", kCutsceneLogo } } },
    };
    return games;
}

// ---------------------
// Playing them
// ---------------------

/** The cutscenes' state for the running game. */
struct CutsceneSession {
    bool logoPlayed  = false; ///< The Sonic Team logo played before the title (once per game start).
    bool resumeLoad  = false; ///< A cutscene held the stage load: when it ends, the load goes on.
    bool videoSound  = false; ///< OnVideoStarted() started the video's sound.
};

CutsceneSession cutscenes;

void ResetCutsceneSession() { cutscenes = CutsceneSession(); }

/** Start, A or B skips a cutscene, as in Sonic CD's videos. */
bool32 CutsceneSkip()
{
    const ControllerState &pad = controller[CONT_ANY];
    return pad.keyStart.press || pad.keyA.press || pad.keyB.press;
}

/** Plays Data/Video/<name>.ogv if a pack has it. @return True if it started. */
bool PlayCutscene(const char *name)
{
    const std::string file = std::string(name) + ".ogv";
    if (FindDataFileSource(("Data/Video/" + file).c_str()) == DATAFILE_SOURCE_NONE)
        return false;
    PrintLog(PRINT_NORMAL, "[Plus] Cutscene %s", name);
    return LoadVideo(file.c_str(), 0.0, CutsceneSkip);
}

/** True for the scenes the title screen and the menus lead to (choosing the credits there is not the ending). */
bool IsMenuScene(const std::string &name)
{
    static const char *const kMenus[] = { "TITLE SCREEN", "MENU", "STAGE MENU", "STAGE SELECT", "TIME ATTACK", "SOUND TEST", "HELP", "DA GARDEN", "SECRETS" };
    for (const char *menu : kMenus) {
        if (name == menu)
            return true;
    }
    return false;
}

/**
 * Called by OnLegacyStageLoad() (PlusRetro.cpp) before a stage loads, once the
 * save screen has been answered: plays the cutscene that goes before it, if
 * any and converted, holding the load until it ends.
 *
 * @param previousCategory The stage list of the scene loaded before.
 * @param previousName     Its name.
 * @param demo             The title is starting its demo.
 * @return True to hold the load (a cutscene plays).
 */
bool PlayCutsceneBeforeLoad(int32 previousCategory, const std::string &previousName, bool demo)
{
    if (cutscenes.resumeLoad) {
        cutscenes.resumeLoad = false; // The cutscene ended: this is the load it held.
        return false;
    }
    if (!sceneInfo.listData || !sceneInfo.listCategory)
        return false;

    std::string scene = sceneInfo.listData[sceneInfo.listPos].name;
    while (!scene.empty() && scene.back() == ' ') scene.pop_back();
    const char *video       = nullptr;
    if (!cutscenes.logoPlayed && sceneInfo.activeCategory == Legacy::STAGELIST_PRESENTATION && scene == "TITLE SCREEN") {
        cutscenes.logoPlayed = true;
        video                = kCutsceneLogo;
    }
    else if (!demo && previousCategory == Legacy::STAGELIST_PRESENTATION && sceneInfo.activeCategory == Legacy::STAGELIST_REGULAR
             && sceneInfo.listPos == sceneInfo.listCategory[Legacy::STAGELIST_REGULAR].sceneOffsetStart) {
        video = kCutsceneIntro; // A new game: the first zone's first act, from the title, save screen or menu.
    }
    else if (sceneInfo.activeCategory == Legacy::STAGELIST_PRESENTATION && scene.find("CREDITS") != std::string::npos && !IsMenuScene(previousName)) {
        video = kCutsceneOutro; // The ending's credits, not those chosen in a menu.
    }

    if (!video || !PlayCutscene(video))
        return false;
    cutscenes.resumeLoad = true;
    return true;
}

} // namespace Plus
} // namespace RSDK

void RSDK::Plus::OnVideoStarted(const char *fileName)
{
    using namespace RSDK::Plus;
    // "Opening.ogv" -> "Data/Music/Video/Opening.hca".
    std::string base = fileName ? fileName : "";
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos)
        base = base.substr(0, dot);
    if (base.empty())
        return;
    std::string sound = "Video/" + base + ".hca";
    // Sonic CD's videos have both soundtracks: the one chosen in its options.
    const std::string american = "Video/" + base + kSecondTrackSuffix + ".hca";
    if (engine.version == 3 && Legacy::GetGlobalVariableID(kCdSoundtrackGlobal) != 0xFF && Legacy::GetGlobalVariableByName(kCdSoundtrackGlobal) == 1
        && FindDataFileSource(("Data/Music/" + american).c_str()) != DATAFILE_SOURCE_NONE)
        sound = american;
    if (FindDataFileSource(("Data/Music/" + sound).c_str()) == DATAFILE_SOURCE_NONE)
        return;
    // The stream channel (0), loaded now so the video starts in time with it.
    cutscenes.videoSound = PlayStream(sound.c_str(), 0, 0, 0, false) == 0;
    PrintLog(PRINT_NORMAL, "[Plus] Video sound %s", sound.c_str());
}

void RSDK::Plus::OnVideoFinished()
{
    using namespace RSDK::Plus;
    if (!cutscenes.videoSound)
        return;
    cutscenes.videoSound = false;
    StopChannel(0);
}

bool32 RSDK::Plus::VideoSoundPlaying() { return RSDK::Plus::cutscenes.videoSound; }

// ---------------------
// Converting them
// ---------------------

namespace RSDK
{
namespace Plus
{

/** The conversion's state, shared between its thread and the engine's (as the audio's, PlusOrigins.cpp). */
OriginsAudioJob videoJob;

void SetVideoMessage(const std::string &text)
{
    std::lock_guard<std::mutex> guard(videoJob.lock);
    videoJob.message = text;
    PrintLog(PRINT_NORMAL, "[Plus] Sonic Origins videos: %s", text.c_str());
}

std::string VideoMessage()
{
    std::lock_guard<std::mutex> guard(videoJob.lock);
    return videoJob.message;
}

#if PLUS_VIDEO_CONVERTER

/** Height of the converted videos (the width follows the picture's shape): the engine draws them at most at the window's size. */
const int32 kVideoHeight = 360;
/** Highest frame rate kept (Sonic CD's 60 fps videos keep every other frame). */
const int32 kMaxVideoFps = 30;
/** Sound tracks kept from a .usm (Sonic CD's videos: Japanese, then American). */
const uint8 kMaxSoundTracks = 2;
/** Theora's quality, 0 to 63. */
const int32 kTheoraQuality = 44;

/**
 * A .usm file read chunk by chunk: "CRID", "@SFV" (video) and "@SFA" (sound)
 * chunks, each a big-endian u32 size after its signature, then (from offset
 * 8) u8, u8 payload offset, u16 padding, u8 channel, ..., u8 type at 15; type
 * 0 is stream data, 1 a header table (@UTF), 2 and 3 markers and seek data.
 */
class UsmReader
{
public:
    explicit UsmReader(const std::string &path) : file(path, "rb") {}

    bool IsOpen() const { return file.file != nullptr; }

    /** The next chunk's signature, channel, type and payload. @return False at the end. */
    bool Next(char signature[4], uint8 *channel, uint8 *type, std::vector<uint8> *payload)
    {
        uint8 header[16];
        if (!file.file || fread(header, 1, sizeof(header), file.file) != sizeof(header))
            return false;
        memcpy(signature, header, 4);
        const uint32 size  = ((uint32)header[4] << 24) | (header[5] << 16) | (header[6] << 8) | header[7];
        const uint32 start = header[9];
        const uint32 pad   = (header[10] << 8) | header[11];
        *channel           = header[12];
        *type              = header[15];
        if (size < 8 || size > 64 * 1024 * 1024 || start + pad > size || start < 8)
            return false;
        // The 8 bytes read past the size belong to the chunk's own header.
        std::vector<uint8> body(size - 8);
        if (!body.empty() && fread(body.data(), 1, body.size(), file.file) != body.size())
            return false;
        payload->assign(body.begin() + (start - 8), body.end() - pad);
        return true;
    }

private:
    PackFile file;
};

/** What a .usm's video header says (VIDEO_HDRINFO): frame count and rate, for the progress and the output. */
struct UsmVideoInfo {
    uint32 frames = 0;
    double fps    = 0.0;
};

/** Reads @p path's video header table. */
bool ReadUsmVideoInfo(const std::string &path, UsmVideoInfo *out)
{
    UsmReader reader(path);
    char signature[4];
    uint8 channel = 0, type = 0;
    std::vector<uint8> payload;
    while (reader.Next(signature, &channel, &type, &payload)) {
        if (memcmp(signature, "@SFV", 4) != 0 || type != 1 || payload.size() < 4 || memcmp(payload.data(), "@UTF", 4) != 0)
            continue;
        UtfTable table;
        if (!table.Parse({ payload.data(), payload.size() }, 0) || !table.Rows())
            return false;
        out->frames             = (uint32)table.Int(0, "total_frames");
        const uint64 numerator  = table.Int(0, "framerate_n");
        const uint64 denominator = table.Int(0, "framerate_d", 1);
        out->fps                = denominator ? (double)numerator / denominator : 0.0;
        return out->frames && out->fps > 0.0;
    }
    return false;
}

/** A frame as Theora takes it: three 8-bit planes, 4:2:0. */
struct YuvFrame {
    int32 width = 0, height = 0; ///< The luma plane (a multiple of 16).
    std::vector<uint8> y, u, v;

    void Allocate(int32 w, int32 h)
    {
        width  = w;
        height = h;
        y.assign((size_t)w * h, 16);
        u.assign((size_t)(w / 2) * (h / 2), 128);
        v.assign((size_t)(w / 2) * (h / 2), 128);
    }
};

/**
 * Scales plane @p src (sw x sh, @p stride bytes a row) into @p dst (dw x dh,
 * tightly packed), each output pixel the average of the source area it covers.
 */
void ScalePlane(const uint8 *src, int32 sw, int32 sh, int32 stride, uint8 *dst, int32 dw, int32 dh, int32 dstStride)
{
    for (int32 y = 0; y < dh; ++y) {
        const int32 y0 = (int32)((int64)y * sh / dh);
        int32 y1       = (int32)((int64)(y + 1) * sh / dh);
        if (y1 <= y0)
            y1 = y0 + 1;
        for (int32 x = 0; x < dw; ++x) {
            const int32 x0 = (int32)((int64)x * sw / dw);
            int32 x1       = (int32)((int64)(x + 1) * sw / dw);
            if (x1 <= x0)
                x1 = x0 + 1;
            uint32 sum = 0;
            for (int32 sy = y0; sy < y1; ++sy) {
                const uint8 *row = src + (size_t)sy * stride;
                for (int32 sx = x0; sx < x1; ++sx) sum += row[sx];
            }
            dst[(size_t)y * dstStride + x] = (uint8)(sum / (uint32)((y1 - y0) * (x1 - x0)));
        }
    }
}

/** Writes Ogg pages to a pack being written, counting the bytes. */
struct OggWriter {
    FILE *file   = nullptr;
    uint64 bytes = 0;
    bool ok      = true;

    void Page(const ogg_page &page)
    {
        if (fwrite(page.header, 1, page.header_len, file) != (size_t)page.header_len || fwrite(page.body, 1, page.body_len, file) != (size_t)page.body_len)
            ok = false;
        bytes += page.header_len + page.body_len;
    }
};

/** Converts one .usm's video into Theora (written at the file's position) and returns its sound. */
class UsmConverter
{
public:
    /**
     * @param onFrame Called after each decoded frame (for the progress); false stops.
     * @param sounds  Receives each sound track (HCA files without their encryption; Sonic CD's
     *                videos have two, its Japanese and its American soundtrack), or stays empty.
     * @param bytes   Receives the Ogg file's size.
     */
    bool Convert(const std::string &path, FILE *out, const std::function<bool()> &onFrame, std::vector<std::vector<uint8>> *sounds,
                 uint64 *bytes, std::string *error)
    {
        UsmVideoInfo info;
        if (!ReadUsmVideoInfo(path, &info)) {
            *error = "not a Sonic Origins video";
            return false;
        }
        step      = info.fps > kMaxVideoFps + 0.5 ? (int32)((info.fps + kMaxVideoFps - 1) / kMaxVideoFps) : 1;
        outputFps = info.fps / step;

        vpx_codec_dec_cfg_t config = {};
        const uint32 cores         = std::thread::hardware_concurrency();
        config.threads             = cores > 1 ? (cores > 4 ? 4 : cores) : 1;
        if (vpx_codec_dec_init(&decoder, vpx_codec_vp9_dx(), &config, 0) != VPX_CODEC_OK) {
            *error = "cannot start the video decoder";
            return false;
        }
        decoderReady = true;

        writer.file = out;
        UsmReader reader(path);
        char signature[4];
        uint8 channel = 0, type = 0;
        std::vector<uint8> payload;
        std::vector<uint8> ivf; // Video stream bytes not yet made into frames.
        size_t ivfPos   = 0;
        bool ivfHeader  = false;
        while (reader.Next(signature, &channel, &type, &payload)) {
            if (type != 0)
                continue;
            if (!memcmp(signature, "@SFA", 4)) {
                if (channel < kMaxSoundTracks) {
                    if (sounds->size() <= channel)
                        sounds->resize(channel + 1);
                    (*sounds)[channel].insert((*sounds)[channel].end(), payload.begin(), payload.end());
                }
                continue;
            }
            if (memcmp(signature, "@SFV", 4) != 0 || channel != 0)
                continue;

            // The video payloads make an IVF file: a 32-byte header, then
            // each frame's u32 size and u64 time (little-endian) and data.
            ivf.insert(ivf.end(), payload.begin(), payload.end());
            if (!ivfHeader) {
                if (ivf.size() < 32)
                    continue;
                if (memcmp(ivf.data(), "DKIF", 4) != 0) {
                    *error = "unexpected video stream";
                    return false;
                }
                ivfPos    = 32;
                ivfHeader = true;
            }
            while (ivf.size() - ivfPos >= 12) {
                const uint32 size = ivf[ivfPos] | (ivf[ivfPos + 1] << 8) | (ivf[ivfPos + 2] << 16) | ((uint32)ivf[ivfPos + 3] << 24);
                if (ivf.size() - ivfPos - 12 < size)
                    break;
                if (!DecodeFrame(ivf.data() + ivfPos + 12, size, onFrame, error))
                    return false;
                ivfPos += 12 + size;
            }
            if (ivfPos > (1 << 20)) {
                ivf.erase(ivf.begin(), ivf.begin() + ivfPos);
                ivfPos = 0;
            }
        }
        if (!DecodeFrame(nullptr, 0, onFrame, error) || !Finish(error))
            return false;
        *bytes = writer.bytes;

        for (std::vector<uint8> &sound : *sounds) {
            if (!sound.empty() && !MakeKeylessHca(sound, kOriginsMovieKey))
                sound.clear(); // A video without its sound is still worth having.
        }
        return true;
    }

    ~UsmConverter()
    {
        if (decoderReady)
            vpx_codec_destroy(&decoder);
        if (encoder)
            th_encode_free(encoder);
        if (streamReady)
            ogg_stream_clear(&stream);
    }

private:
    /** Decodes @p size bytes (null to flush) and encodes the frames it gives. */
    bool DecodeFrame(const uint8 *data, uint32 size, const std::function<bool()> &onFrame, std::string *error)
    {
        if (vpx_codec_decode(&decoder, data, size, nullptr, 0) != VPX_CODEC_OK) {
            *error = std::string("video decoding failed (") + vpx_codec_error(&decoder) + ")";
            return false;
        }
        vpx_codec_iter_t iterator = nullptr;
        while (vpx_image_t *image = vpx_codec_get_frame(&decoder, &iterator)) {
            if (!onFrame()) {
                *error = "cancelled";
                return false;
            }
            if (decoded++ % step != 0)
                continue;
            if (image->fmt != VPX_IMG_FMT_I420) {
                *error = "unexpected picture format";
                return false;
            }
            if (!encoder && !StartEncoder(image->d_w, image->d_h, error))
                return false;

            // Encoded one frame late, so the last one can be marked as such.
            if (havePending && !EncodePending(false, error))
                return false;
            ScalePlane(image->planes[0], image->d_w, image->d_h, image->stride[0], pending.y.data(), pictureWidth, pictureHeight, pending.width);
            ScalePlane(image->planes[1], (image->d_w + 1) / 2, (image->d_h + 1) / 2, image->stride[1], pending.u.data(), pictureWidth / 2,
                       pictureHeight / 2, pending.width / 2);
            ScalePlane(image->planes[2], (image->d_w + 1) / 2, (image->d_h + 1) / 2, image->stride[2], pending.v.data(), pictureWidth / 2,
                       pictureHeight / 2, pending.width / 2);
            havePending = true;
        }
        return true;
    }

    /** Sets up Theora for pictures of @p sourceW x @p sourceH, and writes its headers. */
    bool StartEncoder(uint32 sourceW, uint32 sourceH, std::string *error)
    {
        pictureHeight = kVideoHeight < (int32)sourceH ? kVideoHeight : (int32)sourceH & ~1;
        pictureWidth  = (int32)((uint64)sourceW * pictureHeight / sourceH) & ~1;
        pending.Allocate((pictureWidth + 15) & ~15, (pictureHeight + 15) & ~15);

        th_info info;
        th_info_init(&info);
        info.frame_width        = pending.width;
        info.frame_height       = pending.height;
        info.pic_width          = pictureWidth;
        info.pic_height         = pictureHeight;
        info.pic_x              = 0;
        info.pic_y              = 0;
        info.fps_numerator      = (ogg_uint32_t)(outputFps * 1000.0 + 0.5);
        info.fps_denominator    = 1000;
        info.aspect_numerator   = 1;
        info.aspect_denominator = 1;
        info.colorspace         = TH_CS_UNSPECIFIED;
        info.pixel_fmt          = TH_PF_420;
        info.target_bitrate     = 0;
        info.quality            = kTheoraQuality;
        info.keyframe_granule_shift = 6;
        encoder                 = th_encode_alloc(&info);
        th_info_clear(&info);
        if (!encoder) {
            *error = "cannot start the Theora encoder";
            return false;
        }
        int32 speed = 0;
        if (th_encode_ctl(encoder, TH_ENCCTL_GET_SPLEVEL_MAX, &speed, sizeof(speed)) == 0)
            th_encode_ctl(encoder, TH_ENCCTL_SET_SPLEVEL, &speed, sizeof(speed)); // Fastest: the TV does this.

        ogg_stream_init(&stream, 0x53574d00); // Any serial number: the file has one stream.
        streamReady = true;
        th_comment comment;
        th_comment_init(&comment);
        ogg_packet packet;
        while (th_encode_flushheader(encoder, &comment, &packet) > 0) ogg_stream_packetin(&stream, &packet);
        th_comment_clear(&comment);
        ogg_page page;
        while (ogg_stream_flush(&stream, &page)) writer.Page(page);
        return writer.ok;
    }

    /** Encodes the pending frame (@p last: the video's last). */
    bool EncodePending(bool last, std::string *error)
    {
        th_ycbcr_buffer buffer;
        buffer[0] = { pending.width, pending.height, pending.width, pending.y.data() };
        buffer[1] = { pending.width / 2, pending.height / 2, pending.width / 2, pending.u.data() };
        buffer[2] = { pending.width / 2, pending.height / 2, pending.width / 2, pending.v.data() };
        if (th_encode_ycbcr_in(encoder, buffer) != 0) {
            *error = "Theora encoding failed";
            return false;
        }
        ogg_packet packet;
        while (th_encode_packetout(encoder, last ? 1 : 0, &packet) > 0) ogg_stream_packetin(&stream, &packet);
        ogg_page page;
        while (last ? ogg_stream_flush(&stream, &page) : ogg_stream_pageout(&stream, &page)) writer.Page(page);
        if (!writer.ok)
            *error = "cannot write the pack (disk full?)";
        return writer.ok;
    }

    bool Finish(std::string *error)
    {
        if (!havePending) {
            *error = "the video has no frames";
            return false;
        }
        havePending = false;
        return EncodePending(true, error);
    }

    vpx_codec_ctx_t decoder = {};
    bool decoderReady       = false;
    th_enc_ctx *encoder     = nullptr;
    ogg_stream_state stream = {};
    bool streamReady        = false;
    OggWriter writer;
    YuvFrame pending;
    bool havePending     = false;
    int32 pictureWidth   = 0, pictureHeight = 0;
    int32 step           = 1;
    double outputFps     = 30.0;
    uint64 decoded       = 0;
};

/** One file of a game's video pack, as written. */
struct VideoEntry {
    std::string path;
    uint64 offset = 0;
    uint64 size   = 0;
};

/**
 * Writes @p game's video pack, next to the "origins" folder its first video
 * was found in. @p done and @p total count frames over all games.
 *
 * @return False with a reason in @p error ("no files" when the game has none).
 */
bool ConvertVideoGame(const OriginsVideoGame &game, uint64 *done, uint64 total, int32 *written, std::string *error)
{
    std::vector<std::pair<const OriginsVideo *, std::string>> sources;
    std::string outputFolder; // That of the game's first video, the shared logo aside.
    for (const OriginsVideo &video : game.videos) {
        std::string folder;
        const std::string path = FindOriginsFile(std::string(video.usm) + ".usm", &folder);
        if (path.empty())
            continue;
        if (outputFolder.empty() && strcmp(video.usm, "sonicteam_logo") != 0)
            outputFolder = folder;
        sources.push_back({ &video, path });
    }
    // The logo alone (shared by all) makes no pack.
    if (sources.empty() || (sources.size() == 1 && !strcmp(sources[0].first->usm, "sonicteam_logo"))) {
        *error = "no files";
        return false;
    }

    const std::string finalPath = outputFolder + OriginsVideoPackName(game.id);
#if RETRO_PLATFORM == RETRO_ANDROID
    const std::string path = finalPath;
#else
    const std::string path = finalPath + ".part";
#endif
    // At most a video and its sound tracks each.
    const size_t maxEntries = sources.size() * (1 + kMaxSoundTracks);
    std::vector<VideoEntry> entries;
    bool ok = true;
    {
        PackFile out(path, "wb");
        std::vector<uint8> table(8 + maxEntries * 24, 0);
        if (!out.file || fwrite(table.data(), 1, table.size(), out.file) != table.size()) {
            *error = "cannot write " + OriginsVideoPackName(game.id);
            return false;
        }
        uint64 offset = table.size();
        for (const auto &source : sources) {
            SetVideoMessage(std::string("Converting ") + source.first->usm + "...");
            std::vector<std::vector<uint8>> sounds;
            uint64 bytes = 0;
            UsmConverter converter;
            auto onFrame = [&]() {
                ++*done;
                const uint64 percent = total ? *done * 100 / total : 0;
                videoJob.percent     = (int32)(percent > 99 ? 99 : percent);
                return !videoJob.cancel.load();
            };
            if (!converter.Convert(source.second, out.file, onFrame, &sounds, &bytes, error)) {
                *error = std::string(source.first->usm) + ": " + *error;
                ok     = false;
                break;
            }
            entries.push_back({ std::string("Data/Video/") + source.first->name + ".ogv", offset, bytes });
            offset += bytes;
            for (size_t track = 0; track < sounds.size() && ok; ++track) {
                const std::vector<uint8> &sound = sounds[track];
                if (sound.empty())
                    continue;
                if (fwrite(sound.data(), 1, sound.size(), out.file) != sound.size()) {
                    *error = "cannot write " + OriginsVideoPackName(game.id) + " (disk full?)";
                    ok     = false;
                    break;
                }
                entries.push_back({ std::string("Data/Music/Video/") + source.first->name + (track ? kSecondTrackSuffix : "") + ".hca", offset,
                                    sound.size() });
                offset += sound.size();
            }
            if (!ok)
                break;
            if (offset > 0x7FFFFFFFull) {
                *error = "the pack would be too big";
                ok     = false;
                break;
            }
        }

        // The table (unused rows stay zero: no path hashes to them), then the signature.
        if (ok) {
            table[4] = 'v';
            table[5] = '5';
            table[6] = (uint8)(maxEntries & 0xFF);
            table[7] = (uint8)(maxEntries >> 8);
            for (size_t e = 0; e < entries.size(); ++e) {
                uint8 *at = table.data() + 8 + e * 24;
                PathKey(entries[e].path, at);
                PutU32(at + 16, (uint32)entries[e].offset);
                PutU32(at + 20, (uint32)entries[e].size & 0x7FFFFFFF);
            }
            if (fseek(out.file, 4, SEEK_SET) != 0 || fwrite(table.data() + 4, 1, table.size() - 4, out.file) != table.size() - 4
                || fflush(out.file) != 0 || fseek(out.file, 0, SEEK_SET) != 0 || fwrite("RSDK", 1, 4, out.file) != 4 || fflush(out.file) != 0) {
                *error = "cannot write " + OriginsVideoPackName(game.id) + " (disk full?)";
                ok     = false;
            }
        }
    }
#if RETRO_PLATFORM != RETRO_ANDROID
    if (!ok) {
        remove(path.c_str());
        return false;
    }
    remove(finalPath.c_str()); // Windows' rename() does not replace.
    if (rename(path.c_str(), finalPath.c_str()) != 0) {
        *error = "cannot rename " + OriginsVideoPackName(game.id) + ".part";
        remove(path.c_str());
        return false;
    }
#else
    if (!ok)
        return false;
#endif
    *written = (int32)sources.size();
    return true;
}

/** The conversion thread's work. */
void RunVideoConversion()
{
    // The frames to decode, for the progress (the logo counts once per game).
    uint64 total = 0;
    for (const OriginsVideoGame &game : OriginsVideoGames()) {
        for (const OriginsVideo &video : game.videos) {
            UsmVideoInfo info;
            const std::string path = FindOriginsFile(std::string(video.usm) + ".usm", nullptr);
            if (!path.empty() && ReadUsmVideoInfo(path, &info))
                total += info.frames;
        }
    }
    if (!total) {
        SetVideoMessage("No videos found: copy the .usm files from Sonic Origins' image/x64/raw/movie folder into the \"origins\" "
                        "folder next to the retro packs.");
        videoJob.running  = false;
        videoJob.finished = true;
        return;
    }

    uint64 done = 0;
    std::string summary, failures;
    for (const OriginsVideoGame &game : OriginsVideoGames()) {
        if (videoJob.cancel)
            break;
        std::string title = game.id;
        for (const GameInfo &info : kRetroGames) {
            if (!strcmp(info.id, game.id))
                title = info.title;
        }
        int32 written = 0;
        std::string error;
        if (ConvertVideoGame(game, &done, total, &written, &error))
            summary += (summary.empty() ? "" : ", ") + title + " (" + std::to_string(written) + " videos)";
        else if (error != "no files")
            failures += (failures.empty() ? "" : "; ") + title + ": " + error;
    }

    std::string text = summary.empty() ? "Nothing converted." : "Converted " + summary + ".";
    if (!failures.empty())
        text += " Failed: " + failures + ".";
    SetVideoMessage(text);
    videoJob.percent  = 100;
    videoJob.running  = false;
    videoJob.finished = true;
}

#endif // PLUS_VIDEO_CONVERTER

/** Whether this build can convert the videos (playing converted packs needs nothing more). */
constexpr bool VideoConverterAvailable() { return PLUS_VIDEO_CONVERTER != 0; }

/** Starts the conversion on its own thread. @return False if one is running, or this build cannot convert. */
bool StartVideoConversion()
{
#if PLUS_VIDEO_CONVERTER
    static std::mutex starting;
    std::lock_guard<std::mutex> guard(starting);
    if (videoJob.running)
        return false;
    if (videoJob.worker.joinable())
        videoJob.worker.join();
    videoJob.cancel   = false;
    videoJob.finished = false;
    videoJob.percent  = 0;
    videoJob.running  = true;
    SetVideoMessage("Starting...");
    videoJob.worker = std::thread(RunVideoConversion);
    return true;
#else
    SetVideoMessage("This build cannot convert videos (built without PLUS_VIDEO_CONVERTER). Packs converted elsewhere play.");
    return false;
#endif
}

/** Stops a running conversion and waits for its thread (at shutdown). */
void StopVideoConversion()
{
    videoJob.cancel = true;
    if (videoJob.worker.joinable())
        videoJob.worker.join();
}

/** A game's converted video pack (as FindOriginsAudioPack() finds the audio's). */
Pack FindOriginsVideoPack(const char *gameId)
{
    std::vector<std::string> candidates = { std::string(kRetroFolder) + "/" + OriginsVideoPackName(gameId) };
    for (const std::string &folder : extraPackFolders) candidates.push_back(folder + OriginsVideoPackName(gameId));
    Pack pack;
    for (const std::string &candidate : candidates) {
        PackFile file(UserPath(candidate), "rb");
        uint8 header[6] = {};
        if (!file.file || fread(header, 1, sizeof(header), file.file) != sizeof(header) || memcmp(header, "RSDK", 4) != 0)
            continue;
        pack.state   = PACK_READY;
        pack.path    = candidate;
        pack.version = (char)header[5];
        return pack;
    }
    return pack;
}

} // namespace Plus
} // namespace RSDK
