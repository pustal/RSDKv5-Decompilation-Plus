// =====================
// Plus: checking for a newer release (Windows, macOS, Linux)
// =====================
//
// Included by Plus.cpp. Asks GitHub for the latest release of
// kReleasesRepository and compares its tag ("1.11.0", or "v1.11.0") with
// this build's version (PLUS_VERSION, from CMake's DECOMP_VERSION or Xcode's
// MARKETING_VERSION):
//   - once at start, when Plus.ini's updateCheck is on (the default), saying
//     so over the game when there is a newer one;
//   - from the Plus menu's Updates page, which also opens the releases page
//     in the browser to download it.
// The desktop builds cannot replace themselves, so they only tell. Android
// installs updates itself (Updater.java, with its own "Check for updates");
// the consoles do not check (their networking and certificates differ for
// each one, and the Wii and Wii U have no HTTPS library).
//
// The request runs on its own thread, through the system's HTTPS: WinHTTP on
// Windows, NSURLSession on macOS (cocoaHelpers.mm), and on Linux libcurl,
// loaded when needed (every desktop distribution has it), so building needs
// nothing more.

#ifndef PLUS_VERSION
#define PLUS_VERSION "" // A build without it (the Visual Studio projects) shows the latest version without comparing.
#endif

/** Whether this build checks for updates itself (see above): only the desktop builds have the threads and HTTPS it needs. */
#define PLUS_UPDATE_CHECK (RETRO_PLATFORM == RETRO_WIN || RETRO_PLATFORM == RETRO_OSX || RETRO_PLATFORM == RETRO_LINUX)

#if PLUS_UPDATE_CHECK
#include <mutex>
#include <thread>
#endif

#if RETRO_PLATFORM == RETRO_WIN
#include <winhttp.h>
#include <shellapi.h>
#if defined(_MSC_VER)
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#endif
#elif RETRO_PLATFORM == RETRO_LINUX
#include <dlfcn.h>
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
#endif

namespace RSDK
{
namespace Plus
{

/** Where the releases are published (owner/repository on GitHub). */
const char *const kReleasesRepository = "pustal/Sonic-Mania-Decompilation-Plus";
/** GitHub's API answer about the latest release. */
const char *const kLatestReleaseUrl = "https://api.github.com/repos/pustal/Sonic-Mania-Decompilation-Plus/releases/latest";
/** The page the browser opens to download a release. */
const char *const kReleasesPageUrl = "https://github.com/pustal/Sonic-Mania-Decompilation-Plus/releases/latest";

/** Whether this build checks for updates itself (Android does it in Java, with its own menu entry). */
constexpr bool UpdateCheckAvailable() { return PLUS_UPDATE_CHECK != 0; }

/** The version this build reports ("" when unknown). */
const char *InstalledVersion() { return PLUS_VERSION; }

/** Where a check is. */
enum class UpdateState {
    Idle,     ///< Never checked this session.
    Checking, ///< The request is on its way.
    Newer,    ///< A newer release is out (latest).
    Current,  ///< This is the latest release.
    Failed,   ///< The request failed (error).
};

#if PLUS_UPDATE_CHECK

/** A check's progress and result, shared between its thread and the engine's. */
struct UpdateCheck {
    std::mutex mutex;
    UpdateState state = UpdateState::Idle;
    std::string latest; ///< The latest release's version, when known.
    std::string error;  ///< Why it failed.
    bool automatic = false; ///< Started at start (its result shows as a notice), not from the menu.
    bool announced = false; ///< The notice for an automatic check was shown.
};

/**
 * The one check (allocated once and never freed: a request still running when
 * the game closes must not find it destroyed).
 */
UpdateCheck &UpdateCheckState()
{
    static UpdateCheck *check = new UpdateCheck();
    return *check;
}

/**
 * Compares two versions number by number ("1.10.0" is newer than "1.9.3");
 * missing or non-numeric parts count as 0.
 *
 * @return Negative, 0 or positive as @p a is older than, the same as or newer than @p b.
 */
int32 CompareVersions(const std::string &a, const std::string &b)
{
    size_t i = 0, j = 0;
    while (i < a.size() || j < b.size()) {
        int32 left = 0, right = 0;
        while (i < a.size() && a[i] >= '0' && a[i] <= '9') left = left * 10 + (a[i++] - '0');
        while (j < b.size() && b[j] >= '0' && b[j] <= '9') right = right * 10 + (b[j++] - '0');
        if (left != right)
            return left < right ? -1 : 1;
        // Past this part's non-digits and its dot.
        while (i < a.size() && a[i] != '.') ++i;
        while (j < b.size() && b[j] != '.') ++j;
        if (i < a.size())
            ++i;
        if (j < b.size())
            ++j;
    }
    return 0;
}

/** The latest release's tag in GitHub's JSON answer @p json, without a leading "v" ("" when not found). */
std::string ParseReleaseTag(const std::string &json)
{
    const size_t key = json.find("\"tag_name\"");
    if (key == std::string::npos)
        return std::string();
    const size_t open = json.find('"', json.find(':', key) + 1);
    if (open == std::string::npos)
        return std::string();
    const size_t close = json.find('"', open + 1);
    if (close == std::string::npos)
        return std::string();
    std::string tag = json.substr(open + 1, close - open - 1);
    if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V'))
        tag.erase(0, 1);
    return tag;
}

#if RETRO_PLATFORM == RETRO_WIN
/** @p text (ASCII) as a wide string, for WinHTTP. */
std::wstring Widen(const char *text)
{
    std::wstring wide;
    while (*text) wide += (wchar_t)(unsigned char)*text++;
    return wide;
}
#endif

#if RETRO_PLATFORM == RETRO_LINUX
/** libcurl, loaded when first needed (see HttpGet()). Only the few functions and options used here. */
struct Curl {
    void *(*easyInit)()                          = nullptr;
    int32 (*easySetopt)(void *, int32, ...)      = nullptr;
    int32 (*easyPerform)(void *)                 = nullptr;
    int32 (*easyGetinfo)(void *, int32, ...)     = nullptr;
    const char *(*easyStrerror)(int32)           = nullptr;
    void (*easyCleanup)(void *)                  = nullptr;
    bool loaded                                  = false;

    // curl.h's values (stable across libcurl versions).
    static constexpr int32 kOptWriteData      = 10001;
    static constexpr int32 kOptUrl            = 10002;
    static constexpr int32 kOptUserAgent      = 10018;
    static constexpr int32 kOptWriteFunction  = 20011;
    static constexpr int32 kOptFollowLocation = 52;
    static constexpr int32 kOptTimeout        = 13;
    static constexpr int32 kOptNoSignal       = 99;
    static constexpr int32 kInfoResponseCode  = 0x200002;
};

/** libcurl's functions, or not loaded when the system has none. Called on the request's thread only. */
const Curl &LoadCurl()
{
    static Curl curl;
    static bool tried = false;
    if (tried)
        return curl;
    tried = true;

    void *library = nullptr;
    for (const char *name : { "libcurl.so.4", "libcurl-gnutls.so.4", "libcurl.so" }) {
        if ((library = dlopen(name, RTLD_NOW | RTLD_LOCAL)) != nullptr)
            break;
    }
    if (!library)
        return curl;
    // dlsym() hands out untyped addresses: each is converted to the function's own type.
    curl.easyInit     = reinterpret_cast<void *(*)()>(dlsym(library, "curl_easy_init"));
    curl.easySetopt   = reinterpret_cast<int32 (*)(void *, int32, ...)>(dlsym(library, "curl_easy_setopt"));
    curl.easyPerform  = reinterpret_cast<int32 (*)(void *)>(dlsym(library, "curl_easy_perform"));
    curl.easyGetinfo  = reinterpret_cast<int32 (*)(void *, int32, ...)>(dlsym(library, "curl_easy_getinfo"));
    curl.easyStrerror = reinterpret_cast<const char *(*)(int32)>(dlsym(library, "curl_easy_strerror"));
    curl.easyCleanup  = reinterpret_cast<void (*)(void *)>(dlsym(library, "curl_easy_cleanup"));
    curl.loaded = curl.easyInit && curl.easySetopt && curl.easyPerform && curl.easyGetinfo && curl.easyStrerror && curl.easyCleanup;
    return curl;
}

/** libcurl's write callback: appends what arrives to the std::string @p user. */
size_t CurlWrite(char *data, size_t size, size_t count, void *user)
{
    static_cast<std::string *>(user)->append(data, size * count);
    return size * count;
}
#endif

/**
 * Fetches @p url over HTTPS into @p body, with the system's HTTP stack.
 *
 * @return True on HTTP 200; otherwise false, with @p error saying why.
 */
bool HttpGet(const char *url, std::string *body, std::string *error)
{
    const char *const userAgent = "RSDKv5-Plus";
#if RETRO_PLATFORM == RETRO_WIN
    URL_COMPONENTS parts = {};
    parts.dwStructSize   = sizeof(parts);
    wchar_t host[256]    = {};
    wchar_t path[1024]   = {};
    parts.lpszHostName     = host;
    parts.dwHostNameLength = sizeof(host) / sizeof(host[0]);
    parts.lpszUrlPath      = path;
    parts.dwUrlPathLength  = sizeof(path) / sizeof(path[0]);
    const std::wstring wideUrl = Widen(url);
    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts)) {
        *error = "bad address";
        return false;
    }

    bool ok          = false;
    HINTERNET session = WinHttpOpen(Widen(userAgent).c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET connection = session ? WinHttpConnect(session, host, parts.nPort, 0) : nullptr;
    HINTERNET request    = connection ? WinHttpOpenRequest(connection, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                      : nullptr;
    if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        && WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0, statusSize = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status,
                            &statusSize, WINHTTP_NO_HEADER_INDEX);
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
            std::string chunk(available, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, &chunk[0], available, &read) || read == 0)
                break;
            body->append(chunk.data(), read);
        }
        ok = status == 200;
        if (!ok)
            *error = "HTTP " + std::to_string(status);
    }
    else {
        *error = "no connection (error " + std::to_string(GetLastError()) + ")";
    }
    if (request)
        WinHttpCloseHandle(request);
    if (connection)
        WinHttpCloseHandle(connection);
    if (session)
        WinHttpCloseHandle(session);
    return ok;
#elif RETRO_PLATFORM == RETRO_OSX
    char message[0x100] = {};
    char *text          = nullptr;
    const int32 status  = httpGetText(url, userAgent, &text, message, sizeof(message));
    if (text) {
        body->assign(text);
        free(text);
    }
    if (status != 200)
        *error = message[0] ? message : "HTTP " + std::to_string(status);
    return status == 200;
#elif RETRO_PLATFORM == RETRO_LINUX
    const Curl &curl = LoadCurl();
    if (!curl.loaded) {
        *error = "libcurl is not installed";
        return false;
    }
    void *handle = curl.easyInit();
    if (!handle) {
        *error = "libcurl failed";
        return false;
    }
    curl.easySetopt(handle, Curl::kOptUrl, url);
    curl.easySetopt(handle, Curl::kOptUserAgent, userAgent);
    curl.easySetopt(handle, Curl::kOptFollowLocation, 1L);
    curl.easySetopt(handle, Curl::kOptTimeout, 20L);
    curl.easySetopt(handle, Curl::kOptNoSignal, 1L); // No signals from another thread.
    curl.easySetopt(handle, Curl::kOptWriteFunction, &CurlWrite);
    curl.easySetopt(handle, Curl::kOptWriteData, body);
    const int32 result = curl.easyPerform(handle);
    long status        = 0;
    curl.easyGetinfo(handle, Curl::kInfoResponseCode, &status);
    curl.easyCleanup(handle);
    if (result != 0)
        *error = curl.easyStrerror(result);
    else if (status != 200)
        *error = "HTTP " + std::to_string(status);
    return result == 0 && status == 200;
#else
    (void)url;
    (void)body;
    (void)userAgent;
    *error = "not on this platform";
    return false;
#endif
}

/** Whether this build can open the releases page in a browser. */
constexpr bool OpenReleasesPageAvailable() { return UpdateCheckAvailable(); }

/** Opens the releases page in the system's browser. */
void OpenReleasesPage()
{
#if RETRO_PLATFORM == RETRO_WIN
    ShellExecuteA(nullptr, "open", kReleasesPageUrl, nullptr, nullptr, SW_SHOWNORMAL);
#elif RETRO_PLATFORM == RETRO_OSX
    openURL(kReleasesPageUrl);
#elif RETRO_PLATFORM == RETRO_LINUX
    // xdg-open, without waiting for it: the browser keeps running after it returns.
    // posix_spawnp() takes writable strings.
    std::string program = "xdg-open";
    std::string url     = kReleasesPageUrl;
    char *const arguments[] = { &program[0], &url[0], nullptr };
    pid_t child             = 0;
    if (posix_spawnp(&child, "xdg-open", nullptr, nullptr, arguments, environ) == 0)
        std::thread([child] { waitpid(child, nullptr, 0); }).detach(); // Collected, so it leaves no zombie.
#endif
}

/**
 * Starts a check on its own thread, unless one is running. @p automatic: the
 * start-up check, whose result shows as a notice (UpdateFrame()).
 */
void StartUpdateCheck(bool automatic)
{
    if (!UpdateCheckAvailable())
        return;
    UpdateCheck &check = UpdateCheckState();
    {
        std::lock_guard<std::mutex> lock(check.mutex);
        if (check.state == UpdateState::Checking)
            return;
        check.state     = UpdateState::Checking;
        check.automatic = automatic;
        check.announced = false;
    }
    std::thread([] {
        std::string body, error;
        const bool fetched       = HttpGet(kLatestReleaseUrl, &body, &error);
        const std::string latest = fetched ? ParseReleaseTag(body) : std::string();

        UpdateCheck &result = UpdateCheckState();
        std::lock_guard<std::mutex> lock(result.mutex);
        result.latest = latest;
        if (!fetched || latest.empty()) {
            result.state = UpdateState::Failed;
            result.error = fetched ? "no release found" : error;
        }
        else {
            const bool unknown = !*InstalledVersion();
            result.state       = !unknown && CompareVersions(latest, InstalledVersion()) <= 0 ? UpdateState::Current : UpdateState::Newer;
        }
    }).detach();
}

/** A snapshot of the check, for the menu. */
struct UpdateStatus {
    UpdateState state;
    std::string latest;
    std::string error;
};

UpdateStatus CurrentUpdateStatus()
{
    UpdateCheck &check = UpdateCheckState();
    std::lock_guard<std::mutex> lock(check.mutex);
    return { check.state, check.latest, check.error };
}

/**
 * Once per frame (Update()): starts the start-up check on the first frame
 * when Plus.ini's updateCheck is on, and says over the game when it found a
 * newer release. @p enabled: that setting.
 */
void UpdateFrame(bool enabled)
{
    static bool started = false;
    if (!UpdateCheckAvailable())
        return;
    if (!started) {
        started = true;
        if (enabled)
            StartUpdateCheck(true);
        return;
    }

    UpdateCheck &check = UpdateCheckState();
    std::string newer;
    {
        std::lock_guard<std::mutex> lock(check.mutex);
        if (!check.automatic || check.announced || check.state == UpdateState::Checking)
            return;
        check.announced = true;
        if (check.state == UpdateState::Newer)
            newer = check.latest;
    }
    if (!newer.empty())
        Notify("Version " + newer + " is out: Plus menu, Updates");
}

#else // !PLUS_UPDATE_CHECK

// The consoles and Android: no check here (see above), so the menu has no Updates page.

/** A snapshot of the check, for the menu. */
struct UpdateStatus {
    UpdateState state;
    std::string latest;
    std::string error;
};

UpdateStatus CurrentUpdateStatus() { return { UpdateState::Idle, std::string(), std::string() }; }
void StartUpdateCheck(bool) {}
void UpdateFrame(bool) {}
constexpr bool OpenReleasesPageAvailable() { return false; }
void OpenReleasesPage() {}

#endif // PLUS_UPDATE_CHECK

} // namespace Plus
} // namespace RSDK
