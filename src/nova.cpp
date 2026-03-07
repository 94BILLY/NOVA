/* ============================================================================
 * NOVA — v1.0
 * Copyright (C) 2026 [94BILLY]. All Rights Reserved.
 * ============================================================================
 *
 * PROPRIETARY AND CONFIDENTIAL:
 * This software and its source code are the sole property of the author.
 * Unauthorized copying, distribution, or modification of this file,
 * via any medium, is strictly prohibited.
 * ============================================================================
 *
 * RELEASE NOTES v1.0:
 *   - Unified 17-provider backend (local + cloud)
 *   - Protocol adapters: OpenAI-compat, Anthropic Messages, Gemini, llama-legacy
 *   - Full Settings dialog with provider presets, model/API key config
 *   - Auto-detection of local backends (llama-server, Ollama, LM Studio, etc.)
 *   - Proper chat history formatting for ALL providers
 *   - Config persistence in nova_config.ini
 *   - All original features preserved: EXEC engine, TTS, attachments,
 *     image/audio/video analysis, EvolvingPersonality, dev console
 * ============================================================================
 */
#define WINVER       0x0601
#define _WIN32_WINNT 0x0601
#define NOMINMAX

#ifndef UNICODE
#define UNICODE
#endif

#define _USE_MATH_DEFINES
#include <math.h>

#include <windows.h>
#include <wininet.h>
#include <sapi.h>
#include <string>
#include <algorithm>
#include <thread>
#include <cstdio>
#include <cstdarg>
#include <sstream>
#include <vector>
#include <mutex>
#include <fstream>
#include <atomic>
#include <richedit.h>
#include <commctrl.h>
// GDI+ uses bare min/max — provide them from std:: (NOMINMAX suppresses the Windows macros)
using std::min;
using std::max;
#include <gdiplus.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <shlobj.h>
#include <map>
#include <functional>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "sapi.lib")
#pragma comment(lib, "advapi32.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// DPI awareness — tells Windows not to bitmap-scale the window
#if !defined(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif
typedef BOOL(WINAPI* SetPDAC_t)(DPI_AWARENESS_CONTEXT);
static void EnableDPIAwareness() {
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (!hUser) return;
    auto fn = (SetPDAC_t)GetProcAddress(hUser, "SetProcessDpiAwarenessContext");
    if (fn) fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

// ════════════════════════════════════════════════════════════════
// CONSTANTS & CONTROL IDS
// ════════════════════════════════════════════════════════════════
static constexpr const wchar_t* NOVA_VERSION = L"1.0.1";

#define IDT_PULSE       1002
#define PULSE_INTERVAL  40
#define MIN_WIN_W       600
#define MIN_WIN_H       500
#define WM_AI_DONE      (WM_APP + 1)
#define WM_ENGINE_READY (WM_APP + 2)
#define WM_EXEC_DONE    (WM_APP + 3)

// Button command IDs (main window)
#define IDC_BTN_SEND     101
#define IDC_BTN_CLEAR    102
#define IDC_BTN_MUTE     103
#define IDC_BTN_DEV      104
#define IDC_BTN_ATTACH   105
#define IDC_BTN_SETTINGS 106

// Settings dialog control IDs
#define IDC_COMBO_PROV   201
#define IDC_EDIT_HOST    202
#define IDC_EDIT_PORT    203
#define IDC_EDIT_APIKEY  204
#define IDC_EDIT_MODEL   205
#define IDC_EDIT_TEMP    206
#define IDC_EDIT_MAXTOK  207
#define IDC_EDIT_CTX     208
#define IDC_EDIT_GPU     209
#define IDC_EDIT_MODPATH 210
#define IDC_CHECK_AUTO   211
#define IDC_TEST_BTN     212
#define IDC_SAVE_BTN     213
#define IDC_STATUS_LBL   214

// ════════════════════════════════════════════════════════════════
// ENUMERATIONS
// ════════════════════════════════════════════════════════════════
enum class AppState : int { Online, Busy, Offline };

enum ProviderType {
    PROV_LLAMA_SERVER = 0,  // 1.  llama-server (local, legacy /completion)
    PROV_OLLAMA,            // 2.  Ollama
    PROV_LM_STUDIO,         // 3.  LM Studio
    PROV_VLLM,              // 4.  vLLM
    PROV_KOBOLDCPP,         // 5.  KoboldCpp
    PROV_JAN,               // 6.  Jan
    PROV_GPT4ALL,           // 7.  GPT4All
    PROV_CUSTOM_LOCAL,      // 8.  Custom Local
    PROV_OPENAI,            // 9.  OpenAI
    PROV_ANTHROPIC,         // 10. Anthropic (Claude)
    PROV_GEMINI,            // 11. Google Gemini
    PROV_GROQ,              // 12. Groq
    PROV_MISTRAL,           // 13. Mistral AI
    PROV_TOGETHER,          // 14. Together AI
    PROV_OPENROUTER,        // 15. OpenRouter
    PROV_XAI,               // 16. xAI (Grok)
    PROV_CUSTOM_CLOUD,      // 17. Custom Cloud
    PROV_COUNT              // = 17
};

enum class ProtocolType {
    LlamaLegacy,    // /completion  with "prompt" field
    OpenAICompat,   // /v1/chat/completions  with "messages" array
    Anthropic,      // /v1/messages  with Anthropic-specific format
    Gemini          // /v1beta/models/MODEL:generateContent
};

// ════════════════════════════════════════════════════════════════
// STRUCTS
// ════════════════════════════════════════════════════════════════
struct ProviderPreset {
    const wchar_t* displayName;
    const char*    defaultHost;
    int            defaultPort;
    const char*    defaultEndpoint;
    bool           needsSSL;
    bool           needsApiKey;
    const char*    defaultModel;
    ProtocolType   protocol;
};

static const ProviderPreset g_providerPresets[PROV_COUNT] = {
    // 1. llama-server
    { L"llama-server (local)",    "127.0.0.1", 11434, "/completion",           false, false, "",                        ProtocolType::LlamaLegacy  },
    // 2. Ollama
    { L"Ollama",                  "127.0.0.1", 11434, "/v1/chat/completions",  false, false, "llama3:latest",           ProtocolType::OpenAICompat },
    // 3. LM Studio
    { L"LM Studio",              "127.0.0.1", 1234,  "/v1/chat/completions",  false, false, "",                        ProtocolType::OpenAICompat },
    // 4. vLLM
    { L"vLLM",                   "127.0.0.1", 8000,  "/v1/chat/completions",  false, false, "",                        ProtocolType::OpenAICompat },
    // 5. KoboldCpp
    { L"KoboldCpp",              "127.0.0.1", 5001,  "/v1/chat/completions",  false, false, "",                        ProtocolType::OpenAICompat },
    // 6. Jan
    { L"Jan",                    "127.0.0.1", 1337,  "/v1/chat/completions",  false, false, "",                        ProtocolType::OpenAICompat },
    // 7. GPT4All
    { L"GPT4All",                "127.0.0.1", 4891,  "/v1/chat/completions",  false, false, "",                        ProtocolType::OpenAICompat },
    // 8. Custom Local
    { L"Custom Local",           "127.0.0.1", 8080,  "/v1/chat/completions",  false, false, "",                        ProtocolType::OpenAICompat },
    // 9. OpenAI
    { L"OpenAI",                 "api.openai.com",       443, "/v1/chat/completions",  true, true, "gpt-4o-mini",      ProtocolType::OpenAICompat },
    // 10. Anthropic
    { L"Anthropic (Claude)",     "api.anthropic.com",    443, "/v1/messages",           true, true, "claude-3-haiku-20240307", ProtocolType::Anthropic },
    // 11. Google Gemini
    { L"Google Gemini",          "generativelanguage.googleapis.com", 443, "/v1beta/models/", true, true, "gemini-1.5-flash", ProtocolType::Gemini },
    // 12. Groq
    { L"Groq",                   "api.groq.com",         443, "/openai/v1/chat/completions", true, true, "llama3-8b-8192", ProtocolType::OpenAICompat },
    // 13. Mistral AI
    { L"Mistral AI",             "api.mistral.ai",       443, "/v1/chat/completions",  true, true, "mistral-small-latest", ProtocolType::OpenAICompat },
    // 14. Together AI
    { L"Together AI",            "api.together.xyz",     443, "/v1/chat/completions",  true, true, "meta-llama/Llama-3-8b-chat-hf", ProtocolType::OpenAICompat },
    // 15. OpenRouter
    { L"OpenRouter",             "openrouter.ai",        443, "/api/v1/chat/completions", true, true, "meta-llama/llama-3-8b-instruct", ProtocolType::OpenAICompat },
    // 16. xAI (Grok)
    { L"xAI (Grok)",            "api.x.ai",             443, "/v1/chat/completions",  true, true, "grok-beta",        ProtocolType::OpenAICompat },
    // 17. Custom Cloud
    { L"Custom Cloud",           "api.example.com",      443, "/v1/chat/completions",  true, true, "",                 ProtocolType::OpenAICompat },
};

struct NovaConfig {
    ProviderType provider     = PROV_LLAMA_SERVER;
    std::string  host         = "127.0.0.1";
    int          port         = 11434;
    std::string  apiKey;
    std::string  model;
    std::string  endpointPath = "/completion";
    bool         useSSL       = false;
    float        temperature  = 0.4f;
    int          maxTokens    = 1024;
    int          contextSize  = 8192;
    int          gpuLayers    = 99;
    bool         autoStartEngine = true;
    std::string  modelPath    = "models\\llama3.gguf";
    int          enginePort   = 11434;
};

struct Attachment {
    std::wstring path;
    std::wstring displayName;
    std::string  textContent;
    bool         isImage = false;
    bool         isAudio = false;
    bool         isText  = false;
    bool         isVideo = false;
};

struct ChatRequest {
    std::wstring userText;
    bool         hasAttachment = false;
    Attachment   attachment;
};

// ══════════════════════════════════════════════════════════════════
// THREAD-SAFE APP STATE MANAGEMENT
// ══════════════════════════════════════════════════════════════════
class AppStateManager {
private:
    AppStateManager() = default;
    ~AppStateManager() = default;

public:
    NovaConfig config;
    std::atomic<AppState> state{ AppState::Offline };

    AppStateManager(const AppStateManager&) = delete;
    AppStateManager& operator=(const AppStateManager&) = delete;

    static AppStateManager& Instance() {
        static AppStateManager instance;
        return instance;
    }
};

// ════════════════════════════════════════════════════════════════
// GLOBALS (Bridged to Singleton)
// ════════════════════════════════════════════════════════════════
// These references ensure older code using g_config automatically points to the Singleton
static NovaConfig& g_config = AppStateManager::Instance().config;
static std::atomic<AppState>& g_appState = AppStateManager::Instance().state;

HWND  hMainWnd      = nullptr;
HWND  hEditDisplay  = nullptr;
HWND  hEditInput    = nullptr;
HWND  hButtonSend   = nullptr;
HWND  hButtonClear  = nullptr;
HWND  hButtonMute   = nullptr;
HWND  hButtonDev    = nullptr;
HWND  hButtonAttach = nullptr;
HWND  hButtonSettings = nullptr;
HWND  hIndicator    = nullptr;
HWND  hAttachLabel  = nullptr;
HWND  hSettingsWnd  = nullptr;  // Settings dialog window
HFONT hFontMain     = nullptr;
HFONT hFontBtn      = nullptr;
HFONT hFontIndicator = nullptr;
WNDPROC OldEditProc = nullptr;

bool       g_hasAttachment = false;
Attachment g_attachment;

std::mutex        historyMutex;
std::wstring      conversationHistory;
std::atomic<bool> aiRunning(false);
std::atomic<bool> g_muted(false);
bool              consoleAllocated = false;

ISpVoice* g_pVoice = nullptr;
std::mutex g_voiceMutex;

const size_t      MAX_HISTORY_CHARS = 8000;
const std::string g_historyFile     = "nova_history.txt";
const std::string g_personalityFile = "nova_personality.txt";
const std::string g_devLogFile      = "nova_dev_log.txt";
const std::string g_configFile      = "nova_config.ini";

float     g_pulseT    = 0.0f;
ULONG_PTR g_gdipToken = 0;

PROCESS_INFORMATION g_serverPi = {};

// ════════════════════════════════════════════════════════════════
// FORWARD DECLARATIONS
// ════════════════════════════════════════════════════════════════
std::string GetExeDir();
std::string GetDesktopDir();
std::string PrecisionEscape(const std::string& s);
std::string DecodeJsonString(const std::string& json, const std::string& key);
std::string WStringToString(const std::wstring& w);
std::wstring StringToWString(const std::string& s);
std::string UrlEncode(const std::string& s);
std::string Base64Encode(const std::vector<BYTE>& data);
std::string ExtractReply(const std::string& raw, ProtocolType proto);

void SaveConfig();
void LoadConfig();

void AppendRichText(HWND hRich, const std::wstring& text, bool bBold, COLORREF color = RGB(30, 30, 30));
void SpeakAsync(const std::wstring& text);
void SaveHistory();
void LoadHistory();
void TrimHistory();
std::string LoadPersonality();
void SavePersonality(const std::string& n);
void EvolvePersonality(const std::string& current, const std::string& exchange);
void ExecuteNovaCommand(const std::string& command);

std::string FetchUrl(const std::string& url, const std::string& ua = "Mozilla/5.0");
std::string FetchWeather(const std::string& loc);
std::string FetchNews(const std::string& q);
std::string FetchWiki(const std::string& q);
std::string AnalyzeAndFetch(const std::string& lower, const std::string& orig);

std::string AnalyzeImageGDIPlus(const std::wstring& path);
std::string AnalyzeWavDetailed(const std::wstring& path);
std::string AnalyzeVideoFile(const std::wstring& path, const std::string& ext);
bool LoadAttachment(const std::wstring& path, Attachment& out);
void OpenAttachDialog();
void ClearAttachment();

void AIThreadFunc(std::wstring userMsg, std::string webInfo, bool hasAttach, Attachment attach);
DWORD WINAPI ChatThreadProc(LPVOID param);
void ProcessChat();
void SetAppState(AppState s);
void LayoutControls(HWND hwnd);
bool IsServerAlreadyRunning();
void StartLocalEngine();
void StopLocalEngine();

void ShowSettingsDialog(HWND parent);

LRESULT CALLBACK IndicatorWndProc(HWND h, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK EditSubclassProc(HWND h, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK SettingsWndProc(HWND h, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK WindowProc(HWND h, UINT m, WPARAM w, LPARAM l);

// ════════════════════════════════════════════════════════════════
// DEV LOGGER
// ════════════════════════════════════════════════════════════════
static std::mutex g_logMutex;

static void DevLog(const char* fmt, ...) {
    SYSTEMTIME st; GetLocalTime(&st);
    char timeBuf[32];
    sprintf_s(timeBuf, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);

    va_list args;
    va_start(args, fmt);
    char msgBuf[2048];
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    va_end(args);

    std::lock_guard<std::mutex> lk(g_logMutex);

    FILE* logFile = nullptr;
    std::string logPath = GetExeDir() + g_devLogFile;
    fopen_s(&logFile, logPath.c_str(), "a");
    if (logFile) {
        fprintf(logFile, "%s%s", timeBuf, msgBuf);
        fclose(logFile);
    }
    if (consoleAllocated) {
        printf("%s%s", timeBuf, msgBuf);
        fflush(stdout);
    }
}

// ════════════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ════════════════════════════════════════════════════════════════
std::string PrecisionEscape(const std::string& in) {
    std::ostringstream o;
    for (char c : in) {
        switch (c) {
            case '"':  o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\n': o << "\\n";  break;
            case '\r': o << "\\r";  break;
            case '\t': o << "\\t";  break;
            default:   o << c;      break;
        }
    }
    return o.str();
}

std::string DecodeJsonString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    std::string res; bool esc = false;
    while (pos < json.size()) {
        char c = json[pos++];
        if (esc) {
            esc = false;
            if      (c == 'n')  { res += '\n'; continue; }
            else if (c == 'r')  { res += '\r'; continue; }
            else if (c == 't')  { res += '\t'; continue; }
            else if (c == '"')  { res += '"';  continue; }
            else if (c == '\\') { res += '\\'; continue; }
            else if (c == '/')  { res += '/';  continue; }
            else if (c == 'b')  { res += '\b'; continue; }
            else if (c == 'f')  { res += '\f'; continue; }
            if (c == 'u' && pos + 3 < json.size()) {
                char hex[5] = { json[pos], json[pos+1], json[pos+2], json[pos+3], 0 };
                unsigned int cp = (unsigned int)strtol(hex, nullptr, 16);
                pos += 4;
                if      (cp < 0x80)  { res += (char)cp; }
                else if (cp < 0x800) { res += (char)(0xC0|(cp>>6)); res += (char)(0x80|(cp&0x3F)); }
                else                 { res += (char)(0xE0|(cp>>12)); res += (char)(0x80|((cp>>6)&0x3F)); res += (char)(0x80|(cp&0x3F)); }
            } else { res += c; }
        }
        else if (c == '\\') esc = true;
        else if (c == '"')  break;
        else res += c;
    }
    // Strip "Nova: " prefix if the model echoes it
    if      (res.compare(0, 6, "Nova: ") == 0) res.erase(0, 6);
    else if (res.compare(0, 5, "Nova:")  == 0) res.erase(0, 5);
    return res;
}

std::string WStringToString(const std::wstring& w) {
    if (w.empty()) return "";
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), 0, 0, 0, 0);
    std::string r(sz, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &r[0], sz, 0, 0);
    return r;
}

std::wstring StringToWString(const std::string& s) {
    if (s.empty()) return L"";
    int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), 0, 0);
    std::wstring r(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &r[0], sz);
    return r;
}

std::string UrlEncode(const std::string& s) {
    std::string e; char h[4];
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') e += c;
        else if (c == ' ') e += '+';
        else { sprintf_s(h, "%%%02X", c); e += h; }
    }
    return e;
}

std::string GetExeDir() {
    wchar_t p[MAX_PATH];
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring ws(p);
    size_t last = ws.find_last_of(L"\\/");
    std::wstring dir = (last != std::wstring::npos) ? ws.substr(0, last + 1) : L"";
    return WStringToString(dir);
}

std::string GetDesktopDir() {
    wchar_t path[MAX_PATH];
    if (SHGetSpecialFolderPathW(NULL, path, CSIDL_DESKTOP, FALSE))
        return WStringToString(path) + "\\";
    return "";
}

std::string Base64Encode(const std::vector<BYTE>& data) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        DWORD val = (DWORD)data[i] << 16;
        if (i + 1 < data.size()) val |= (DWORD)data[i+1] << 8;
        if (i + 2 < data.size()) val |= (DWORD)data[i+2];
        out += tbl[(val >> 18) & 0x3F];
        out += tbl[(val >> 12) & 0x3F];
        out += (i + 1 < data.size()) ? tbl[(val >>  6) & 0x3F] : '=';
        out += (i + 2 < data.size()) ? tbl[(val >>  0) & 0x3F] : '=';
    }
    return out;
}

// ════════════════════════════════════════════════════════════════
// CONFIGURATION (nova_config.ini)
// ════════════════════════════════════════════════════════════════
void SaveConfig() {
    std::string path = GetExeDir() + g_configFile;
    std::ofstream f(path);
    if (!f) { DevLog("[Config] ERROR: could not save %s\n", path.c_str()); return; }
    f << "provider="         << (int)g_config.provider     << "\n";
    f << "host="             << g_config.host               << "\n";
    f << "port="             << g_config.port               << "\n";
    f << "api_key="          << g_config.apiKey             << "\n";
    f << "model="            << g_config.model              << "\n";
    f << "endpoint_path="    << g_config.endpointPath       << "\n";
    f << "use_ssl="          << (g_config.useSSL ? 1 : 0)   << "\n";
    f << "temperature="      << g_config.temperature        << "\n";
    f << "max_tokens="       << g_config.maxTokens          << "\n";
    f << "context_size="     << g_config.contextSize        << "\n";
    f << "gpu_layers="       << g_config.gpuLayers          << "\n";
    f << "auto_start_engine=" << (g_config.autoStartEngine ? 1 : 0) << "\n";
    f << "model_path="       << g_config.modelPath          << "\n";
    f << "engine_port="      << g_config.enginePort         << "\n";
    DevLog("[Config] Saved: provider=%d host=%s port=%d model=%s\n",
           (int)g_config.provider, g_config.host.c_str(), g_config.port, g_config.model.c_str());
}

void LoadConfig() {
    std::string path = GetExeDir() + g_configFile;
    std::ifstream f(path);
    if (!f) {
        DevLog("[Config] No config file found — using defaults\n");
        // Apply defaults from preset 0 (llama-server)
        const auto& p = g_providerPresets[0];
        g_config.host         = p.defaultHost;
        g_config.port         = p.defaultPort;
        g_config.endpointPath = p.defaultEndpoint;
        g_config.useSSL       = p.needsSSL;
        g_config.model        = p.defaultModel;
        return;
    }
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        // Trim trailing whitespace
        while (!val.empty() && (val.back() == '\r' || val.back() == '\n' || val.back() == ' '))
            val.pop_back();

        if      (key == "provider")          { int v = atoi(val.c_str()); if (v >= 0 && v < PROV_COUNT) g_config.provider = (ProviderType)v; }
        else if (key == "host")              g_config.host = val;
        else if (key == "port")              g_config.port = atoi(val.c_str());
        else if (key == "api_key")           g_config.apiKey = val;
        else if (key == "model")             g_config.model = val;
        else if (key == "endpoint_path")     g_config.endpointPath = val;
        else if (key == "use_ssl")           g_config.useSSL = (val == "1");
        else if (key == "temperature")       g_config.temperature = (float)atof(val.c_str());
        else if (key == "max_tokens")        g_config.maxTokens = atoi(val.c_str());
        else if (key == "context_size")      g_config.contextSize = atoi(val.c_str());
        else if (key == "gpu_layers")        g_config.gpuLayers = atoi(val.c_str());
        else if (key == "auto_start_engine") g_config.autoStartEngine = (val == "1");
        else if (key == "model_path")        g_config.modelPath = val;
        else if (key == "engine_port")       g_config.enginePort = atoi(val.c_str());
    }
    DevLog("[Config] Loaded: provider=%d (%S) host=%s port=%d model=%s\n",
           (int)g_config.provider, g_providerPresets[g_config.provider].displayName,
           g_config.host.c_str(), g_config.port, g_config.model.c_str());
}

// ════════════════════════════════════════════════════════════════
// LOCAL AI ENGINE MANAGEMENT
// ════════════════════════════════════════════════════════════════
bool IsServerAlreadyRunning() {
    HINTERNET hS = InternetOpenW(L"NovaProbe", 1, 0, 0, 0);
    if (!hS) return false;
    DWORD timeout = 1000;
    InternetSetOptionA(hS, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hS, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    std::wstring host = StringToWString(g_config.host);
    INTERNET_PORT port = (INTERNET_PORT)g_config.enginePort;
    HINTERNET hC = InternetConnectW(hS, host.c_str(), port, 0, 0, 3, 0, 0);
    bool alive = false;
    if (hC) {
        HINTERNET hR = HttpOpenRequestW(hC, L"GET", L"/health", 0, 0, 0, INTERNET_FLAG_RELOAD, 0);
        if (hR) {
            alive = HttpSendRequestA(hR, 0, 0, 0, 0) ? true : false;
            InternetCloseHandle(hR);
        }
        InternetCloseHandle(hC);
    }
    InternetCloseHandle(hS);
    return alive;
}

void StartLocalEngine() {
    if (!g_config.autoStartEngine || g_config.provider != PROV_LLAMA_SERVER) return;

    if (IsServerAlreadyRunning()) {
        DevLog("[System] Server already running on :%d — skipping launch\n", g_config.enginePort);
        return;
    }

    DevLog("[System] Starting embedded llama-server engine...\n");
    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    char cmd[1024];
    sprintf_s(cmd, "engine\\llama-server.exe -m \"%s\" --alias default --port %d -c %d -ngl %d --host 127.0.0.1",
              g_config.modelPath.c_str(), g_config.enginePort,
              g_config.contextSize, g_config.gpuLayers);

    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &g_serverPi)) {
        CloseHandle(g_serverPi.hThread);
        g_serverPi.hThread = NULL;
        
        DevLog("[System] Local engine launched (PID: %lu). Waiting for warm-up...\n", g_serverPi.dwProcessId);
        
        for (int i = 0; i < 30; i++) {
            Sleep(1000);
            if (IsServerAlreadyRunning()) {
                // THE XP GOLD FIX: Give the GPU 5 seconds to swallow the 4.8GB model
                DevLog("[System] Port detected. Buffering 5s for GPU VRAM allocation...\n");
                Sleep(5000); 
                DevLog("[System] Engine and Model 100%% ready.\n");
                return;
            }
        }
        DevLog("[System] WARNING: Engine port did not open within 30s\n");
    } else {
        DevLog("[System] ERROR: Failed to start local engine. GLE=%lu\n", GetLastError());
    }
}

void StopLocalEngine() {
    if (g_serverPi.hProcess) {
        DevLog("[System] Shutting down local engine (PID: %lu)...\n", g_serverPi.dwProcessId);
        
        // Forcefully terminate the process (XP Gold reliability)
        TerminateProcess(g_serverPi.hProcess, 0);
        
        CloseHandle(g_serverPi.hProcess);
        if (g_serverPi.hThread) CloseHandle(g_serverPi.hThread);
        
        g_serverPi.hProcess = NULL;
        g_serverPi.hThread = NULL;
    } else {
        DevLog("[System] Engine was externally managed — not killing\n");
    }
}

// ════════════════════════════════════════════════════════════════
// ATTACHMENT ANALYSIS
// ════════════════════════════════════════════════════════════════
static std::string ExtensionOf(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return "";
    std::string ext = WStringToString(path.substr(dot + 1));
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)::tolower(c); });
    return ext;
}

std::string AnalyzeImageGDIPlus(const std::wstring& path) {
    using namespace Gdiplus;
    Bitmap bmp(path.c_str());
    if (bmp.GetLastStatus() != Ok) return "ERROR: Could not load image with GDI+.";

    UINT w = bmp.GetWidth(), h = bmp.GetHeight();
    REAL dpiX = bmp.GetHorizontalResolution();
    REAL dpiY = bmp.GetVerticalResolution();

    PixelFormat pf = bmp.GetPixelFormat();
    const char* pfName = "unknown";
    if      (pf == PixelFormat32bppARGB)    pfName = "32-bit ARGB";
    else if (pf == PixelFormat32bppRGB)     pfName = "32-bit RGB";
    else if (pf == PixelFormat24bppRGB)     pfName = "24-bit RGB";
    else if (pf == PixelFormat8bppIndexed)  pfName = "8-bit indexed";
    else if (pf == PixelFormat1bppIndexed)  pfName = "1-bit B&W";
    else if (pf == PixelFormat16bppGrayScale) pfName = "16-bit grayscale";

    const int S = 50;
    long long rSum = 0, gSum = 0, bSum = 0, aSum = 0;
    long long brightHigh = 0, brightMid = 0, brightLow = 0;
    long long hueRed = 0, hueGreen = 0, hueBlue = 0, hueNeutral = 0;
    long long transparentPx = 0, edgeSum = 0;
    int peakBright = 0, peakDark = 255;
    long long count = 0;

    for (int sy = 0; sy < S; sy++) {
        for (int sx = 0; sx < S; sx++) {
            UINT px = (UINT)((float)sx / S * (w > 0 ? w - 1 : 0));
            UINT py = (UINT)((float)sy / S * (h > 0 ? h - 1 : 0));
            Color c; bmp.GetPixel(px, py, &c);
            int r = c.GetR(), g = c.GetG(), b = c.GetB(), a = c.GetA();
            rSum += r; gSum += g; bSum += b; aSum += a;
            if (a < 128) { transparentPx++; count++; continue; }
            int bright = (r * 299 + g * 587 + b * 114) / 1000;
            if (bright > peakBright) peakBright = bright;
            if (bright < peakDark)   peakDark   = bright;
            if (bright > 170)      brightHigh++;
            else if (bright > 85)  brightMid++;
            else                   brightLow++;

            int maxC = std::max(r, std::max(g, b)), minC = std::min(r, std::min(g, b));
            int sat = maxC > 0 ? ((maxC - minC) * 255 / maxC) : 0;
            if (sat < 40) hueNeutral++;
            else if (r == maxC) hueRed++;
            else if (g == maxC) hueGreen++;
            else                hueBlue++;

            if (sx + 1 < S) {
                UINT px2 = (UINT)(((float)(sx+1)) / S * (w > 0 ? w - 1 : 0));
                Color c2; bmp.GetPixel(px2, py, &c2);
                int dr = abs((int)c2.GetR()-r), dg = abs((int)c2.GetG()-g), db = abs((int)c2.GetB()-b);
                edgeSum += (dr + dg + db) / 3;
            }
            count++;
        }
    }
    if (count == 0) return "Empty or unreadable image.";

    int avgR = (int)(rSum / count), avgG = (int)(gSum / count), avgB = (int)(bSum / count);
    int avgBright = (avgR * 299 + avgG * 587 + avgB * 114) / 1000;
    int avgEdge   = (int)(edgeSum / std::max(1LL, count));

    const char* brightDesc = avgBright > 200 ? "very bright/high-key" : avgBright > 140 ? "bright"
                           : avgBright > 100 ? "balanced mid-tone" : avgBright > 60 ? "dark" : "very dark/low-key";
    const char* sharpDesc  = avgEdge > 30 ? "high detail / sharp" : avgEdge > 15 ? "moderate detail"
                           : avgEdge > 5 ? "soft / low contrast" : "very smooth / flat";
    long long coloured = hueRed + hueGreen + hueBlue;
    const char* palette = hueNeutral > coloured * 2 ? "predominantly grayscale/neutral"
        : hueRed > hueGreen && hueRed > hueBlue ? "warm (reds/oranges dominant)"
        : hueGreen > hueRed && hueGreen > hueBlue ? "natural/green tones dominant"
        : hueBlue > hueRed && hueBlue > hueGreen ? "cool (blues dominant)" : "mixed/balanced colour palette";

    char buf[1024];
    sprintf_s(buf,
        "=== IMAGE ANALYSIS: \"%s\" ===\n"
        "Dimensions: %u x %u | DPI: %.0f x %.0f | Aspect: %.3f:1 (%s)\n"
        "Format: %s | Transparency: %s\n"
        "Avg colour: R=%d G=%d B=%d | Brightness: %d/255 (%s)\n"
        "Palette: %s | Edge density: %d/255 (%s)\n"
        "Analyse this image data and give detailed, insightful feedback.",
        WStringToString(path.substr(path.find_last_of(L"\\/")+1)).c_str(),
        w, h, (double)dpiX, (double)dpiY,
        (double)w / std::max(1u, h),
        (w > h*1.5f ? "landscape" : h > w*1.5f ? "portrait" : w == h ? "square" : "standard"),
        pfName,
        (transparentPx > count/10) ? "significant alpha" : (pf & PixelFormatAlpha) ? "supported but opaque" : "none",
        avgR, avgG, avgB, avgBright, brightDesc, palette, avgEdge, sharpDesc);
    return buf;
}

std::string AnalyzeWavDetailed(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "ERROR: Could not open WAV file.";

    char riff[4]; f.read(riff, 4);
    if (std::string(riff, 4) != "RIFF") return "ERROR: Not a valid RIFF/WAV file.";
    DWORD chunkSize; f.read((char*)&chunkSize, 4);
    char wave[4]; f.read(wave, 4);
    if (std::string(wave, 4) != "WAVE") return "ERROR: Not a WAVE file.";

    WORD audioFmt = 0, channels = 0, bitsPerSample = 0, blockAlign = 0;
    DWORD sampleRate = 0, byteRate = 0, dataSize = 0;
    bool fmtFound = false;

    char id[4]; DWORD sz;
    while (f.read(id, 4) && f.read((char*)&sz, 4)) {
        std::string tag(id, 4);
        if (tag == "fmt ") {
            f.read((char*)&audioFmt, 2); f.read((char*)&channels, 2);
            f.read((char*)&sampleRate, 4); f.read((char*)&byteRate, 4);
            f.read((char*)&blockAlign, 2); f.read((char*)&bitsPerSample, 2);
            if (sz > 16) f.ignore(sz - 16);
            fmtFound = true;
        } else if (tag == "data") { dataSize = sz; break; }
        else f.ignore(sz);
    }
    if (!fmtFound) return "ERROR: Could not find fmt chunk.";

    double duration = (byteRate > 0) ? (double)dataSize / byteRate : 0.0;
    int mins = (int)duration / 60, secs = (int)duration % 60;
    const char* fmtName = (audioFmt == 1 ? "PCM" : audioFmt == 3 ? "IEEE Float" : "compressed");

    double rmsSum = 0.0, peak = 0.0, prevSample = 0.0;
    double leftRms = 0, rightRms = 0;
    long long totalSamples = 0, silentSamples = 0, zeroCrossings = 0;
    const long long MAX_SAMPLES = 5000000;

    if (audioFmt == 1 && bitsPerSample == 16 && channels >= 1) {
        std::vector<int16_t> buf(4096);
        long long samplesRead = 0;
        while (samplesRead < MAX_SAMPLES) {
            size_t toRead = std::min((size_t)4096, (size_t)(MAX_SAMPLES - samplesRead));
            f.read((char*)buf.data(), toRead * 2);
            std::streamsize got = f.gcount() / 2;
            if (got <= 0) break;
            for (int i = 0; i < got; i++) {
                double s = buf[i] / 32768.0;
                rmsSum += s * s;
                if (fabs(s) > peak) peak = fabs(s);
                if (fabs(s) < 0.01) silentSamples++;
                if ((s >= 0) != (prevSample >= 0)) zeroCrossings++;
                prevSample = s;
                if (channels == 2) { if (i % 2 == 0) leftRms += s * s; else rightRms += s * s; }
                totalSamples++;
            }
            samplesRead += got;
        }
    }

    char buf[2048];
    if (totalSamples > 0) {
        double rms = sqrt(rmsSum / totalSamples);
        double rmsDb = (rms > 1e-10) ? 20.0 * log10(rms) : -999.0;
        double peakDb = (peak > 1e-10) ? 20.0 * log10(peak) : -999.0;
        double dynRange = peakDb - rmsDb;
        double silencePct = (double)silentSamples / totalSamples * 100.0;
        sprintf_s(buf,
            "=== WAV ANALYSIS ===\nFormat: %s | %d-bit | %lu Hz | %d ch | %d:%02d\n"
            "RMS: %.1f dBFS | Peak: %.1f dBFS | Dynamic range: %.1f dB | Silence: %.1f%%\n"
            "Analyse this audio and give detailed feedback.",
            fmtName, (int)bitsPerSample, sampleRate, (int)channels, mins, secs,
            rmsDb, peakDb, dynRange, silencePct);
    } else {
        sprintf_s(buf,
            "=== WAV FILE ===\nFormat: %s | %d-bit | %lu Hz | %d ch | %d:%02d\n"
            "Note: Sample-level analysis not available for format %s.",
            fmtName, (int)bitsPerSample, sampleRate, (int)channels, mins, secs, fmtName);
    }
    return buf;
}

std::string AnalyzeVideoFile(const std::wstring& path, const std::string& ext) {
    std::string nameA = WStringToString(path.substr(path.find_last_of(L"\\/")+1));
    WIN32_FILE_ATTRIBUTE_DATA fa = {};
    GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa);
    ULONGLONG fileSize = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;

    // Try ffprobe for detailed analysis
    std::string exeDir = GetExeDir();
    std::string ffprobe = exeDir + "ffprobe.exe";
    { DWORD attr = GetFileAttributesA(ffprobe.c_str()); if (attr == INVALID_FILE_ATTRIBUTES) ffprobe = "ffprobe.exe"; }

    std::string tmpOut = exeDir + "ffprobe_tmp.txt";
    std::string pathA = WStringToString(path);
    std::string fullCmd = "cmd.exe /d /c \"\"" + ffprobe + "\" -v quiet -print_format json -show_format -show_streams \""
                        + pathA + "\" > \"" + tmpOut + "\" 2>&1\"";

    STARTUPINFOA si = { sizeof(si) }; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::vector<char> cmdBuf(fullCmd.begin(), fullCmd.end()); cmdBuf.push_back('\0');

    if (CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        DWORD waitResult = WaitForSingleObject(pi.hProcess, 15000);
        if (waitResult == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        std::ifstream tf(tmpOut);
        if (tf) {
            std::ostringstream ss; ss << tf.rdbuf();
            std::string result = ss.str();
            tf.close(); DeleteFileA(tmpOut.c_str());
            if (result.find("codec_name") != std::string::npos) {
                char buf[512];
                sprintf_s(buf, "=== VIDEO: \"%s\" | %s | %.2f MB ===\nffprobe data:\n%s\nAnalyse this video.",
                    nameA.c_str(), ext.c_str(), (double)fileSize / (1024.0*1024.0), result.c_str());
                return buf;
            }
        }
    }

    char buf[512];
    sprintf_s(buf,
        "=== VIDEO FILE ===\nFilename: \"%s\" | Format: %s | Size: %.2f MB\n"
        "Note: ffprobe not found. Place ffprobe.exe next to nova.exe for full analysis.",
        nameA.c_str(), ext.c_str(), (double)fileSize / (1024.0*1024.0));
    return buf;
}

bool LoadAttachment(const std::wstring& path, Attachment& out) {
    out = {};
    size_t slash = path.find_last_of(L"\\/");
    out.path = path;
    out.displayName = (slash != std::wstring::npos) ? path.substr(slash + 1) : path;
    std::string ext = ExtensionOf(path);

    static const std::vector<std::string> textExts = {
        "txt","cpp","h","c","hpp","py","js","ts","json","xml","html",
        "css","md","log","csv","ini","yaml","yml","bat","ps1","sh","rc","asm"
    };
    if (std::find(textExts.begin(), textExts.end(), ext) != textExts.end()) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        std::ostringstream ss; ss << f.rdbuf();
        std::string raw = ss.str();
        if (raw.size() > 12000) raw = raw.substr(0, 12000) + "\n... [truncated]";
        out.textContent = "=== FILE: \"" + WStringToString(out.displayName) + "\" ===\n" + raw + "\n=== END ===\nAnalyse this file.";
        out.isText = true;
        DevLog("[Attach] Text: %zu chars\n", out.textContent.size());
        return true;
    }

    static const std::vector<std::string> imgExts = { "jpg","jpeg","png","bmp","gif","webp","tif","tiff","ico" };
    if (std::find(imgExts.begin(), imgExts.end(), ext) != imgExts.end()) {
        out.textContent = AnalyzeImageGDIPlus(path);
        out.isImage = true;
        return true;
    }

    static const std::vector<std::string> audioExts = { "wav","mp3","flac","ogg","aac","wma","m4a","aiff","aif" };
    if (std::find(audioExts.begin(), audioExts.end(), ext) != audioExts.end()) {
        if (ext == "wav") out.textContent = AnalyzeWavDetailed(path);
        else {
            WIN32_FILE_ATTRIBUTE_DATA fa2 = {};
            GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa2);
            char buf[256]; sprintf_s(buf, "=== AUDIO: \"%s\" | %s | %.2f MB ===",
                WStringToString(out.displayName).c_str(), ext.c_str(),
                (double)(((ULONGLONG)fa2.nFileSizeHigh << 32) | fa2.nFileSizeLow) / (1024.0*1024.0));
            out.textContent = buf;
        }
        out.isAudio = true;
        return true;
    }

    static const std::vector<std::string> videoExts = { "mp4","mov","avi","mkv","wmv","flv","webm","m4v","mpg","mpeg","ts","mts" };
    if (std::find(videoExts.begin(), videoExts.end(), ext) != videoExts.end()) {
        out.textContent = AnalyzeVideoFile(path, ext);
        out.isVideo = true;
        return true;
    }

    DevLog("[Attach] Unsupported type: .%s\n", ext.c_str());
    return false;
}

void ClearAttachment() {
    g_hasAttachment = false;
    g_attachment = {};
    if (hAttachLabel) SetWindowTextW(hAttachLabel, L"");
}

void OpenAttachDialog() {
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hMainWnd;
    ofn.lpstrFilter =
        L"All Supported\0*.txt;*.cpp;*.h;*.c;*.hpp;*.py;*.js;*.ts;*.json;*.xml;*.html;*.css;*.md;*.log;*.csv;*.ini;*.yaml;*.yml;*.bat;*.ps1;*.rc;*.asm;"
        L"*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.webp;*.tif;*.tiff;*.ico;"
        L"*.wav;*.mp3;*.flac;*.ogg;*.aac;*.wma;*.m4a;*.aiff;"
        L"*.mp4;*.mov;*.avi;*.mkv;*.wmv;*.flv;*.webm;*.m4v;*.mpg;*.mpeg\0"
        L"All Files\0*.*\0";
    ofn.lpstrFile  = filePath;
    ofn.nMaxFile   = MAX_PATH;
    ofn.Flags      = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Attach File for Nova to Analyse";

    if (!GetOpenFileNameW(&ofn)) return;
    Attachment loaded;
    if (LoadAttachment(filePath, loaded)) {
        g_attachment = loaded;
        g_hasAttachment = true;
        SetWindowTextW(hAttachLabel, (L"\U0001F4CE  " + loaded.displayName).c_str());
        DevLog("[Attach] Ready: %s\n", WStringToString(loaded.displayName).c_str());
    } else {
        MessageBoxW(hMainWnd, L"Unsupported file type.", L"Nova", MB_ICONWARNING);
    }
}

// ════════════════════════════════════════════════════════════════
// SYSTEM EXECUTION ENGINE
// ════════════════════════════════════════════════════════════════
void ExecuteNovaCommand(const std::string& command) {
    static const std::vector<std::string> blockedTargets = {
        g_personalityFile, g_historyFile, g_devLogFile,
        "nova_personality", "nova_history", "nova_dev_log"
    };
    std::string lower = command;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)::tolower(c); });

    for (const auto& b : blockedTargets) {
        if (lower.find(b) != std::string::npos) {
            DevLog("[Security] BLOCKED protected file: %s\n", command.c_str());
            return;
        }
    }
    DevLog("[System] Executing: %s\n", command.c_str());

    // Native Set-Content interceptor (Fixes the C++ syntax error bug)
    if (lower.find("set-content") != std::string::npos && lower.find("-path") != std::string::npos) {
        size_t pathTag = lower.find("-path");
        size_t pathQ1 = command.find('\'', pathTag);
        size_t pathQ2 = (pathQ1 != std::string::npos) ? command.find('\'', pathQ1 + 1) : std::string::npos;
        size_t valTag = lower.find("-value");
        size_t valQ1 = command.find('\'', valTag);
        size_t valQ2 = (valQ1 != std::string::npos) ? command.find_last_of('\'') : std::string::npos;

        if (pathQ1 != std::string::npos && pathQ2 != std::string::npos &&
            valQ1 != std::string::npos && valQ2 != std::string::npos) {
            std::string targetPath = command.substr(pathQ1 + 1, pathQ2 - pathQ1 - 1);
            std::string content = command.substr(valQ1 + 1, valQ2 - valQ1 - 1);
            
            size_t pos = 0;
            while ((pos = content.find("`n", pos)) != std::string::npos) { content.replace(pos, 2, "\n"); pos++; }
            
            // Unescape literal quotes for C++
            pos = 0;
            while ((pos = content.find("\\\"", pos)) != std::string::npos) { content.replace(pos, 2, "\""); pos++; }
            
            std::ofstream out(targetPath, std::ios::binary);
            if (out) { out << content; DevLog("[System] Native write: %s\n", targetPath.c_str()); return; }
        }
    }

    // Shell execution (Dynamic Search for cl.exe - Fixes the laptop compatibility bug)
    std::string outPath = GetExeDir() + "nova_exec_out.txt";
    bool needsVS = (lower.find("cl ") != std::string::npos || lower.find("msbuild") != std::string::npos);
    DWORD timeoutMs = (needsVS || lower.find("powershell") != std::string::npos) ? 300000 : 60000;

    std::string full;
    if (needsVS) {
        std::string vcvarsPath = 
            "if exist \"%ProgramFiles%\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat\" "
            "(call \"%ProgramFiles%\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat\") else "
            "if exist \"%ProgramFiles(x86)%\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat\" "
            "(call \"%ProgramFiles(x86)%\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat\") else "
            "if exist \"%ProgramFiles%\\Microsoft Visual Studio\\2022\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat\" "
            "(call \"%ProgramFiles%\\Microsoft Visual Studio\\2022\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat\") else "
            "if exist \"%ProgramFiles(x86)%\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat\" "
            "(call \"%ProgramFiles(x86)%\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat\") else "
            "if exist \"%ProgramFiles(x86)%\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat\" "
            "(call \"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat\" x64)";

        full = "cmd.exe /d /c \"(" + vcvarsPath + " && " + command + ") > \"" + outPath + "\" 2>&1\"";
    } else {
        full = "cmd.exe /d /c \"(" + command + ") > \"" + outPath + "\" 2>&1\"";
    }

    STARTUPINFOA si = { sizeof(si) }; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::vector<char> buf(full.begin(), full.end()); buf.push_back('\0');

    if (CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, timeoutMs);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        std::ifstream outFile(outPath);
        if (outFile) {
            std::ostringstream ss; ss << outFile.rdbuf();
            std::string output = ss.str();
            outFile.close(); DeleteFileA(outPath.c_str());
            std::lock_guard<std::mutex> lk(historyMutex);
            conversationHistory += L"User: [Command output]\r\n" + StringToWString(output) + L"\r\n";
        }
    }
}

// ════════════════════════════════════════════════════════════════
// HISTORY & PERSONALITY
// ════════════════════════════════════════════════════════════════
void TrimHistory() {
    if (conversationHistory.size() <= MAX_HISTORY_CHARS) return;
    size_t cut = conversationHistory.size() - MAX_HISTORY_CHARS;
    size_t nl = conversationHistory.find(L'\n', cut);
    conversationHistory = (nl != std::wstring::npos) ? conversationHistory.substr(nl + 1) : conversationHistory.substr(cut);
}

void SaveHistory() {
    std::ofstream f(GetExeDir() + g_historyFile);
    if (f) f << WStringToString(conversationHistory);
}

void LoadHistory() {
    std::ifstream f(GetExeDir() + g_historyFile);
    if (!f) return;
    std::stringstream ss; ss << f.rdbuf();
    std::wstring raw = StringToWString(ss.str());
    std::wstring clean;
    std::wstringstream wss(raw);
    std::wstring line;
    while (std::getline(wss, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        bool isRefusal = (line.find(L"Nova: I cannot") == 0 || line.find(L"Nova: I am unable") == 0 ||
                          line.find(L"Nova: I can't") == 0 || line.find(L"Nova: Sorry, I") == 0);
        if (isRefusal) continue;
        clean += line + L"\n";
    }
    conversationHistory = clean;
    TrimHistory();
}

std::string LoadPersonality() {
    std::string basePrompt = "You are a local system automation agent. Output is technical and minimal.";
    std::ifstream f(GetExeDir() + g_personalityFile);
    if (f) {
        std::stringstream ss; ss << f.rdbuf();
        basePrompt = ss.str();
    }
    
    // Dynamically inject the true hardware path of the user's desktop
    basePrompt += "\n\n[SYSTEM ENVIRONMENT VARIABLES]\n";
    basePrompt += "USER DESKTOP PATH: " + GetDesktopDir() + "\n";
    basePrompt += "CRITICAL RULE: Always use the exact path above when saving or reading files from the desktop.\n";
    
    return basePrompt;
}

void SavePersonality(const std::string& n) {
    std::ofstream f(GetExeDir() + g_personalityFile);
    if (f) f << n;
}

void EvolvePersonality(const std::string& current, const std::string& exchange) {
    static int counter = 0;
    if (++counter % 3 != 0) return;

    std::string safeExchange = exchange;
    if (safeExchange.size() > 4000) safeExchange = safeExchange.substr(0, 4000);

    DevLog("[Personality] Evolution started (call #%d)\n", counter);
    std::string p = "Current Personality:\n" + current + "\n\nRecent exchange:\n" + safeExchange
                  + "\n\nBriefly update the personality. Keep the tone warm, encouraging, inquisitive.";

    // Use the configured provider for personality evolution too
    ProtocolType proto = g_providerPresets[g_config.provider].protocol;
    std::string body;

    if (proto == ProtocolType::LlamaLegacy) {
        body = "{\"prompt\":\"" + PrecisionEscape(p) + "\",\"n_predict\":512,\"temperature\":0.5,\"stream\":false}";
    } else {
        body = "{\"model\":\"" + PrecisionEscape(g_config.model) + "\","
               "\"messages\":[{\"role\":\"user\",\"content\":\"" + PrecisionEscape(p) + "\"}],"
               "\"max_tokens\":512,\"temperature\":0.5,\"stream\":false}";
    }

    std::wstring host = StringToWString(g_config.host);
    INTERNET_PORT port = (INTERNET_PORT)g_config.port;
    std::wstring endpoint = StringToWString(g_config.endpointPath);
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (g_config.useSSL) flags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;

    HINTERNET hS = InternetOpenW(L"NovaEvolve", 1, 0, 0, 0);
    if (!hS) return;
    HINTERNET hC = InternetConnectW(hS, host.c_str(), port, 0, 0, 3, 0, 0);
    if (!hC) { InternetCloseHandle(hS); return; }

    HINTERNET hR = HttpOpenRequestW(hC, L"POST", endpoint.c_str(), 0, 0, 0, flags, 0);
    if (hR) {
        std::string headers = "Content-Type: application/json\r\n";
        if (!g_config.apiKey.empty()) {
            if (proto == ProtocolType::Anthropic) {
                headers += "x-api-key: " + g_config.apiKey + "\r\nanthropic-version: 2023-06-01\r\n";
            } else {
                headers += "Authorization: Bearer " + g_config.apiKey + "\r\n";
            }
        }
        if (HttpSendRequestA(hR, headers.c_str(), (DWORD)headers.size(), (void*)body.c_str(), (DWORD)body.size())) {
            std::string full; char b[4096]; DWORD r;
            while (InternetReadFile(hR, b, 4096, &r) && r > 0) full.append(b, r);
            std::string up = ExtractReply(full, proto);
            if (!up.empty()) { SavePersonality(up); DevLog("[Personality] Updated OK\n"); }
        }
        InternetCloseHandle(hR);
    }
    InternetCloseHandle(hC);
    InternetCloseHandle(hS);
}

// ════════════════════════════════════════════════════════════════
// NETWORK FETCHERS (Weather, News, Wiki)
// ════════════════════════════════════════════════════════════════
std::string FetchUrl(const std::string& url, const std::string& ua) {
    std::string res;
    HINTERNET hS = InternetOpenA(ua.c_str(), INTERNET_OPEN_TYPE_DIRECT, 0, 0, 0);
    if (!hS) return "";
    DWORD toConn = 10000, toRecv = 15000;
    InternetSetOptionA(hS, INTERNET_OPTION_CONNECT_TIMEOUT, &toConn, sizeof(toConn));
    InternetSetOptionA(hS, INTERNET_OPTION_RECEIVE_TIMEOUT, &toRecv, sizeof(toRecv));
    HINTERNET hU = InternetOpenUrlA(hS, url.c_str(), 0, 0, INTERNET_FLAG_RELOAD, 0);
    if (hU) {
        char buf[8192]; DWORD bR;
        while (InternetReadFile(hU, buf, sizeof(buf)-1, &bR) && bR > 0) { buf[bR] = 0; res.append(buf, bR); }
        InternetCloseHandle(hU);
    }
    InternetCloseHandle(hS);
    return res;
}

std::string FetchWeather(const std::string& loc) { return FetchUrl("https://wttr.in/" + UrlEncode(loc) + "?format=3", "curl"); }

std::string FetchNews(const std::string&) {
    std::string rss = FetchUrl("https://feeds.bbci.co.uk/news/world/rss.xml");
    std::string h; size_t p = 0; int c = 0;
    while (c < 5) {
        p = rss.find("<title>", p); if (p == std::string::npos) break; p += 7;
        size_t e = rss.find("</title>", p); if (e == std::string::npos) break;
        std::string t = rss.substr(p, e - p);
        if (t.find("BBC") == std::string::npos) { h += "* " + t + "\n"; c++; }
        p = e + 1;
    }
    return h;
}

std::string FetchWiki(const std::string& q) {
    return DecodeJsonString(FetchUrl("https://en.wikipedia.org/api/rest_v1/page/summary/" + UrlEncode(q)), "extract");
}

std::string AnalyzeAndFetch(const std::string& lower, const std::string& orig) {
    if (lower.find("weather") != std::string::npos) {
        // Extract location: look for "weather in X", "weather for X", or fall back to "weather X"
        std::string loc;
        for (const char* prefix : { "weather in ", "weather for ", "weather at ", "weather " }) {
            size_t p = lower.find(prefix);
            if (p != std::string::npos) {
                loc = orig.substr(p + strlen(prefix));
                // Trim trailing punctuation/whitespace
                while (!loc.empty() && (loc.back() == '?' || loc.back() == '.' || loc.back() == ' '))
                    loc.pop_back();
                break;
            }
        }
        if (loc.empty()) loc = "auto"; // wttr.in auto-detects from IP
        return "Weather: " + FetchWeather(loc);
    }
    if (lower.find("news") != std::string::npos) return "World News:\n" + FetchNews(orig);
    if ((lower.find("who is") != std::string::npos || lower.find("what is") != std::string::npos) && orig.size() < 60)
        return "Wiki: " + FetchWiki(orig);
    return "";
}

// ════════════════════════════════════════════════════════════════
// SPEECH
// ════════════════════════════════════════════════════════════════
void SpeakAsync(const std::wstring& text) {
    if (g_muted || text.empty()) return;
    std::lock_guard<std::mutex> lk(g_voiceMutex);
    if (g_pVoice) g_pVoice->Speak(text.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
}

// ════════════════════════════════════════════════════════════════
// RICH TEXT
// ════════════════════════════════════════════════════════════════
void AppendRichText(HWND hRich, const std::wstring& text, bool bBold, COLORREF color) {
    CHARFORMAT2W cf = {};
    cf.cbSize      = sizeof(cf);
    cf.dwMask      = CFM_BOLD | CFM_FACE | CFM_SIZE | CFM_COLOR;
    cf.dwEffects   = bBold ? CFE_BOLD : 0;
    cf.yHeight     = 320;
    cf.crTextColor = color;
    wcscpy_s(cf.szFaceName, L"Times New Roman");
    SendMessageW(hRich, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(hRich, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(hRich, EM_REPLACESEL, 0, (LPARAM)text.c_str());
    SendMessageW(hRich, WM_VSCROLL, SB_BOTTOM, 0);
}

// ════════════════════════════════════════════════════════════════
// UNIFIED AI REQUEST BUILDER & SENDER
// ════════════════════════════════════════════════════════════════

// Build chat history as JSON message array for OpenAI/Anthropic/Gemini
static std::string BuildChatMessages(const std::string& snapshot, const std::string& userPrompt, ProtocolType proto) {
    // Parse history into user/assistant turns
    struct Turn { std::string role; std::string content; };
    std::vector<Turn> turns;

    std::istringstream hs(snapshot);
    std::string line;
    std::string currentRole, currentContent;

    auto flush = [&]() {
        if (!currentContent.empty()) {
            if (!currentContent.empty() && currentContent.back() == '\n') currentContent.pop_back();
            turns.push_back({ currentRole, currentContent });
            currentContent.clear();
        }
    };

    while (std::getline(hs, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("User: ", 0) == 0) { flush(); currentRole = "user"; currentContent = line.substr(6) + "\n"; }
        else if (line.rfind("Nova: ", 0) == 0) { flush(); currentRole = "assistant"; currentContent = line.substr(6) + "\n"; }
        else if (line.rfind("[System]: ", 0) == 0) { flush(); currentRole = "user"; currentContent = "[Command Output] " + line.substr(10) + "\n"; }
        else if (!currentRole.empty()) currentContent += line + "\n";
    }
    flush();

    // Add current user message
    turns.push_back({ "user", userPrompt });

    // Anthropic requires: first message = "user", alternating roles, no consecutive same-role.
    // Merge consecutive same-role messages.
    if (proto == ProtocolType::Anthropic || proto == ProtocolType::OpenAICompat) {
        std::vector<Turn> merged;
        for (auto& t : turns) {
            if (!merged.empty() && merged.back().role == t.role) {
                merged.back().content += "\n" + t.content;
            } else {
                merged.push_back(t);
            }
        }
        // Anthropic: first message must be "user"
        if (proto == ProtocolType::Anthropic && !merged.empty() && merged.front().role != "user") {
            merged.insert(merged.begin(), { "user", "[conversation history]" });
        }
        turns = std::move(merged);
    }

    // Build JSON array
    std::string arr;
    if (proto == ProtocolType::Gemini) {
        // Gemini uses "contents" with "role" = "user"/"model"
        arr = "[";
        for (size_t i = 0; i < turns.size(); i++) {
            std::string gRole = (turns[i].role == "assistant") ? "model" : "user";
            if (i > 0) arr += ",";
            arr += "{\"role\":\"" + gRole + "\",\"parts\":[{\"text\":\"" + PrecisionEscape(turns[i].content) + "\"}]}";
        }
        arr += "]";
    } else {
        // OpenAI / Anthropic format
        arr = "[";
        for (size_t i = 0; i < turns.size(); i++) {
            if (i > 0) arr += ",";
            arr += "{\"role\":\"" + turns[i].role + "\",\"content\":\"" + PrecisionEscape(turns[i].content) + "\"}";
        }
        arr += "]";
    }
    return arr;
}

// Build the full HTTP request body for the configured provider
static std::string BuildRequestBody(const std::string& sysPrompt, const std::string& snapshot,
                                     const std::string& userPrompt, ProtocolType proto)
{
    switch (proto) {
    case ProtocolType::LlamaLegacy: {
        // llama-server /completion with Llama-3 chat template
        std::string formattedHistory;
        {
            std::istringstream hs(snapshot);
            std::string line, curRole, curContent;
            auto flush = [&]() {
                if (!curContent.empty()) {
                    if (curContent.back() == '\n') curContent.pop_back();
                    std::string rid = (curRole == "user") ? "user" : "assistant";
                    formattedHistory += "<|start_header_id|>" + rid + "<|end_header_id|>\n\n" + curContent + "<|eot_id|>";
                    curContent.clear();
                }
            };
            while (std::getline(hs, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.rfind("User: ", 0) == 0)       { flush(); curRole = "user"; curContent = line.substr(6) + "\n"; }
                else if (line.rfind("Nova: ", 0) == 0)   { flush(); curRole = "assistant"; curContent = line.substr(6) + "\n"; }
                else if (line.rfind("[System]: ", 0) == 0) { flush(); curRole = "user"; curContent = "[Command Output] " + line.substr(10) + "\n"; }
                else if (!curRole.empty()) curContent += line + "\n";
            }
            flush();
        }

        std::string fewShot =
            "<|start_header_id|>user<|end_header_id|>\n\ncreate a new folder on the desktop<|eot_id|>"
            "<|start_header_id|>assistant<|end_header_id|>\n\nEXEC: mkdir C:\\Users\\Public\\Desktop\\NewNovaFolder<|eot_id|>"
            "<|start_header_id|>user<|end_header_id|>\n\nthanks!<|eot_id|>"
            "<|start_header_id|>assistant<|end_header_id|>\n\nYou're welcome. Ready for the next task.<|eot_id|>";

        std::string fullPrompt = "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n"
                               + sysPrompt + "<|eot_id|>"
                               + fewShot + formattedHistory
                               + "<|start_header_id|>user<|end_header_id|>\n\n"
                               + userPrompt + "<|eot_id|>"
                               + "<|start_header_id|>assistant<|end_header_id|>\n\n";

        return "{\"prompt\":\"" + PrecisionEscape(fullPrompt) + "\","
               "\"n_predict\":" + std::to_string(g_config.maxTokens) + ","
               "\"temperature\":" + std::to_string(g_config.temperature) + ","
               "\"stream\":false,\"special\":true,"
               "\"stop\":[\"<|eot_id|>\",\"User:\",\"Nova:\"]}";
    }

    case ProtocolType::OpenAICompat: {
        std::string messages = BuildChatMessages(snapshot, userPrompt, proto);
        // Insert system message at front
        std::string sysMsg = "{\"role\":\"system\",\"content\":\"" + PrecisionEscape(sysPrompt) + "\"}";
        // Replace leading [ with [sysMsg,
        if (messages.size() > 1) messages = "[" + sysMsg + "," + messages.substr(1);
        else messages = "[" + sysMsg + "]";

        return "{\"model\":\"" + PrecisionEscape(g_config.model) + "\","
               "\"messages\":" + messages + ","
               "\"temperature\":" + std::to_string(g_config.temperature) + ","
               "\"max_tokens\":" + std::to_string(g_config.maxTokens) + ","
               "\"stream\":false}";
    }

    case ProtocolType::Anthropic: {
        std::string messages = BuildChatMessages(snapshot, userPrompt, proto);
        return "{\"model\":\"" + PrecisionEscape(g_config.model) + "\","
               "\"system\":\"" + PrecisionEscape(sysPrompt) + "\","
               "\"messages\":" + messages + ","
               "\"max_tokens\":" + std::to_string(g_config.maxTokens) + ","
               "\"temperature\":" + std::to_string(g_config.temperature) + ","
               "\"stream\":false}";
    }

    case ProtocolType::Gemini: {
        std::string contents = BuildChatMessages(snapshot, userPrompt, proto);
        return "{\"contents\":" + contents + ","
               "\"systemInstruction\":{\"parts\":[{\"text\":\"" + PrecisionEscape(sysPrompt) + "\"}]},"
               "\"generationConfig\":{\"temperature\":" + std::to_string(g_config.temperature) + ","
               "\"maxOutputTokens\":" + std::to_string(g_config.maxTokens) + "}}";
    }
    }
    return "{}";
}

// Extract the assistant's reply text from the provider's JSON response
static std::string ExtractReply(const std::string& raw, ProtocolType proto) {
    switch (proto) {
    case ProtocolType::LlamaLegacy:
        return DecodeJsonString(raw, "content");
    case ProtocolType::OpenAICompat: {
        size_t msgPos = raw.find("\"message\"");
        if (msgPos != std::string::npos) {
            std::string sub = raw.substr(msgPos);
            return DecodeJsonString(sub, "content");
        }
        return DecodeJsonString(raw, "content");
    }
    case ProtocolType::Anthropic: {
        size_t cPos = raw.find("\"content\"");
        if (cPos != std::string::npos) {
            std::string sub = raw.substr(cPos);
            size_t tPos = sub.find("\"text\"");
            if (tPos != std::string::npos) {
                size_t second = sub.find("\"text\"", tPos + 6);
                if (second != std::string::npos) {
                    std::string sub2 = sub.substr(second);
                    return DecodeJsonString("{" + sub2 + "}", "text");
                }
            }
            return DecodeJsonString(sub, "text");
        }
        return "";
    }
    case ProtocolType::Gemini:
        return DecodeJsonString(raw, "text");
    }
    return "";
}

// Send request to the configured provider and return reply
static std::string SendToProvider(const std::string& body) {
    std::wstring host = StringToWString(g_config.host);
    INTERNET_PORT port = (INTERNET_PORT)g_config.port;
    ProtocolType proto = g_providerPresets[g_config.provider].protocol;

    // Build endpoint — Gemini needs model name and API key in URL
    std::string ep = g_config.endpointPath;
    if (proto == ProtocolType::Gemini) {
        ep = "/v1beta/models/" + g_config.model + ":generateContent?key=" + g_config.apiKey;
    }
    std::wstring endpoint = StringToWString(ep);

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (g_config.useSSL) flags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;

    HINTERNET hS = InternetOpenW(L"NovaAI/2.0", 1, 0, 0, 0);
    if (!hS) { DevLog("[Provider] ERROR: InternetOpen failed\n"); return ""; }

    DWORD toConn = 15000, toRecv = 180000;
    InternetSetOptionW(hS, INTERNET_OPTION_CONNECT_TIMEOUT, &toConn, sizeof(toConn));
    InternetSetOptionW(hS, INTERNET_OPTION_RECEIVE_TIMEOUT, &toRecv, sizeof(toRecv));
    InternetSetOptionW(hS, INTERNET_OPTION_SEND_TIMEOUT,    &toRecv, sizeof(toRecv));

    HINTERNET hC = InternetConnectW(hS, host.c_str(), port, 0, 0, 3, 0, 0);
    if (!hC) {
        DevLog("[Provider] ERROR: Cannot connect to %s:%d GLE=%lu\n", g_config.host.c_str(), port, GetLastError());
        InternetCloseHandle(hS); return "";
    }

    HINTERNET hR = HttpOpenRequestW(hC, L"POST", endpoint.c_str(), 0, 0, 0, flags, 0);
    std::string result;

    if (hR) {
        // Build headers based on provider
        std::string headers = "Content-Type: application/json\r\n";
        if (!g_config.apiKey.empty() && proto != ProtocolType::Gemini) { // Gemini uses URL key
            if (proto == ProtocolType::Anthropic) {
                headers += "x-api-key: " + g_config.apiKey + "\r\n";
                headers += "anthropic-version: 2023-06-01\r\n";
            } else {
                headers += "Authorization: Bearer " + g_config.apiKey + "\r\n";
            }
        }

        DevLog("[Provider] Sending %zu bytes to %s:%d%s\n", body.size(), g_config.host.c_str(), port, ep.c_str());

        if (HttpSendRequestA(hR, headers.c_str(), (DWORD)headers.size(), (void*)body.c_str(), (DWORD)body.size())) {
            // Log HTTP status code for debugging
            DWORD statusCode = 0, szStatus = sizeof(statusCode);
            HttpQueryInfoA(hR, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &szStatus, nullptr);
            char buf[8192]; DWORD r;
            while (InternetReadFile(hR, buf, 8192, &r) && r > 0) result.append(buf, r);
            DevLog("[Provider] HTTP %lu — %zu bytes received\n", statusCode, result.size());
            if (statusCode >= 400) {
                DevLog("[Provider] ERROR response: %.200s\n", result.c_str());
            }
        } else {
            DevLog("[Provider] ERROR: HttpSendRequest failed GLE=%lu\n", GetLastError());
        }
        InternetCloseHandle(hR);
    }
    InternetCloseHandle(hC);
    InternetCloseHandle(hS);
    return result;
}

// ════════════════════════════════════════════════════════════════
// AI THREAD (Unified — works with all 17 providers)
// ════════════════════════════════════════════════════════════════
void AIThreadFunc(std::wstring userMsg, std::string webInfo, bool hasAttach, Attachment attach) {
    DevLog("[AI] Thread started — provider: %S\n", g_providerPresets[AppStateManager::Instance().config.provider].displayName);

    // 1. Dynamically get paths for Universal Release
    char* userProfilePath = nullptr;
    size_t len = 0;
    _dupenv_s(&userProfilePath, &len, "USERPROFILE");
    std::string uniProfile = userProfilePath ? userProfilePath : "C:\\";
    if (userProfilePath) free(userProfilePath);

    // Get the desktop using your existing GetDesktopDir() helper
    std::string uniDesktop = GetDesktopDir(); 
    if (uniDesktop.back() == '\\') uniDesktop.pop_back(); // Remove trailing slash for consistency

    // 2. Build the System Prompt
    std::string sys = LoadPersonality();
    sys += "\n\n=== SYSTEM PROTOCOL ===\n";
    sys += "1. You are Nova, a local Windows automation agent.\n";
    sys += "2. ENVIRONMENT: Profile=" + uniProfile + ", Desktop=" + uniDesktop + "\n";
    sys += "3. FILE WRITING: Never use 'echo'. Always use this exact format:\n";
    sys += "   EXEC: powershell -Command \"Set-Content -Path '" + uniDesktop + "\\app.cpp' -Value 'code_here'\"\n";
    sys += "4. Use '`n' for new lines and '\\\"' for quotes inside the code.\n";
    sys += "5. COMPILATION: Always cd to the desktop first. Format:\n";
    sys += "   EXEC: cmd /c \"cd /d " + uniDesktop + " && cl /nologo /O2 /EHsc /std:c++17 /Fe:app.exe app.cpp\"\n";
    sys += "6. If user provides code or asks for an application, GENERATE THE FULL SOURCE and save via EXEC: using powershell Set-Content.\n";
    
    sys += "\n=== CAPABILITIES ===\n";
    sys += "- ATTACH: File content analysis.\n";
    sys += "- SPEECH: Responses read via SAPI TTS.\n";
    sys += "- INTERNET: Weather, news, and Wikipedia are fetched automatically.\n";
    
    sys += "\n=== CONSTRAINTS ===\n";
    sys += "Always use absolute paths starting with " + uniProfile + "\\\n";
    sys += "Be direct. Do not add disclaimers or apologies.\n";

    if (!webInfo.empty()) sys += "\n\nContext:\n" + webInfo;

    // 3. Prepare the request
    std::string userPrompt = WStringToString(userMsg);
    if (hasAttach) userPrompt += "\n\nAttached file content:\n" + attach.textContent;

    std::string snapshot;
    { 
        std::lock_guard<std::mutex> lk(historyMutex); 
        snapshot = WStringToString(conversationHistory); 
    }

    ProtocolType proto = g_providerPresets[AppStateManager::Instance().config.provider].protocol;
    std::string body = BuildRequestBody(sys, snapshot, userPrompt, proto);

    // 4. Send and Process
    std::string rawResponse = SendToProvider(body);
    std::string clean = ExtractReply(rawResponse, proto);

    bool ok = !clean.empty();
    std::wstring reply;
    if (ok) {
        reply = StringToWString(clean);
        {
            std::lock_guard<std::mutex> lk(historyMutex);
            conversationHistory += L"Nova: " + reply + L"\r\n";
        }
        TrimHistory();
        SaveHistory();
        SpeakAsync(reply);
    } else {
        DevLog("[AI] ERROR: Empty reply from provider.\n");
    }

    // 5. Update UI (Message the main window that we are done)
    WCHAR* heapStr = ok ? new WCHAR[reply.size() + 1] : nullptr;
    if (heapStr) wcscpy_s(heapStr, reply.size() + 1, reply.c_str());
    PostMessageW(hMainWnd, WM_AI_DONE, (WPARAM)ok, (LPARAM)heapStr);

    // 6. Personality evolution
    if (ok) {
        std::string cleanReply = clean;
        std::string currentP = LoadPersonality();
    //  std::thread([currentP, cleanReply]() { EvolvePersonality(currentP, cleanReply); }).detach();
    }
}

// ════════════════════════════════════════════════════════════════
// CHAT THREAD (Background processor)
// ════════════════════════════════════════════════════════════════
DWORD WINAPI ChatThreadProc(LPVOID p) {
    ChatRequest* r = (ChatRequest*)p;
    std::wstring txt = r->userText;
    bool hasAttach = r->hasAttachment;
    Attachment attach = r->attachment;
    delete r;

    std::string orig = WStringToString(txt);
    std::string low = orig;
    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return (char)::tolower(c); });
    DevLog("[Chat] User: %.120s\n", orig.c_str());

    std::string info = AnalyzeAndFetch(low, orig);
    AIThreadFunc(txt, info, hasAttach, attach);
    return 0;
}

// ════════════════════════════════════════════════════════════════
// UI CHAT SUBMISSION
// ════════════════════════════════════════════════════════════════
void ProcessChat() {
    if (aiRunning) return;
    int len = GetWindowTextLengthW(hEditInput);
    if (len <= 0) return;
    std::wstring txt(len + 1, L'\0');
    GetWindowTextW(hEditInput, txt.data(), len + 1);
    txt.resize(len);

    { 
        std::lock_guard<std::mutex> lk(historyMutex); 
        conversationHistory += L"User: " + txt + L"\r\n"; 
    }
    
    AppendRichText(hEditDisplay, L"You: ", true);
    AppendRichText(hEditDisplay, txt + L"\r\n", false);
    
    if (g_hasAttachment) {
        AppendRichText(hEditDisplay, L"\U0001F4CE  " + g_attachment.displayName + L"\r\n", false, RGB(100, 100, 180));
    }

    SetWindowTextW(hEditInput, L"");
    EnableWindow(hButtonSend, FALSE);
    aiRunning = true;
    SetAppState(AppState::Busy);

    ChatRequest* req = new ChatRequest;
    req->userText      = txt;
    req->hasAttachment = g_hasAttachment;
    req->attachment    = g_attachment;
    ClearAttachment();

    HANDLE hThread = CreateThread(0, 0, ChatThreadProc, req, 0, 0);
    if (!hThread) { 
        delete req; 
        aiRunning = false; 
        EnableWindow(hButtonSend, TRUE); 
        SetAppState(AppState::Offline); 
    } else {
        CloseHandle(hThread);
    }
}

// ════════════════════════════════════════════════════════════════
// STATUS INDICATOR (Animated)
// ════════════════════════════════════════════════════════════════
LRESULT CALLBACK IndicatorWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: SetTimer(hwnd, IDT_PULSE, 40, nullptr); return 0;
    case WM_TIMER:
        g_pulseT += (g_appState.load() == AppState::Busy) ? 0.18f : 0.08f;
        if (g_pulseT > (float)(2.0 * M_PI)) g_pulseT -= (float)(2.0 * M_PI);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdcScreen = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, rc.right, rc.bottom);
        HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBmp);
        FillRect(hdcMem, &rc, (HBRUSH)(COLOR_BTNFACE + 1));

        {
            Gdiplus::Graphics gfx(hdcMem);
            gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            gfx.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

            float pulse = sinf(g_pulseT) * 0.5f + 0.5f;
            BYTE rC = 40, gC = 200, bC = 80;
            std::wstring statusText = L"Online";
            AppState currentState = g_appState.load();

            if (currentState == AppState::Busy)    { rC = 230; gC = 140; bC = 20; statusText = L"Thinking..."; }
            else if (currentState == AppState::Offline) { rC = 210; gC = 50; bC = 50; statusText = L"Offline"; }

            const float cx = 13.0f, cy = rc.bottom / 2.0f, baseR = 4.0f;
            const float pulseR = baseR + pulse * 2.0f, glowR = pulseR + 4.0f; 

            Gdiplus::SolidBrush bGlow(Gdiplus::Color((BYTE)(pulse * 50), rC, gC, bC));
            Gdiplus::SolidBrush bPulse(Gdiplus::Color((BYTE)(90 + pulse * 80), rC, gC, bC));
            Gdiplus::SolidBrush bBase(Gdiplus::Color(255, rC, gC, bC));

            gfx.FillEllipse(&bGlow, cx - glowR, cy - glowR, glowR * 2, glowR * 2);
            gfx.FillEllipse(&bPulse, cx - pulseR, cy - pulseR, pulseR * 2, pulseR * 2);
            gfx.FillEllipse(&bBase, cx - baseR, cy - baseR, baseR * 2, baseR * 2);

            Gdiplus::Font font(L"Segoe UI", 9, Gdiplus::FontStyleRegular);
            Gdiplus::SolidBrush bText(Gdiplus::Color(255, 80, 80, 80));
            Gdiplus::PointF origin(cx + 15.0f, cy - 7.0f);
            gfx.DrawString(statusText.c_str(), -1, &font, origin, &bText);
        }

        BitBlt(hdcScreen, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOld); DeleteObject(hBmp); DeleteDC(hdcMem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY: KillTimer(hwnd, IDT_PULSE); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ════════════════════════════════════════════════════════════════
// SETTINGS DIALOG
// ════════════════════════════════════════════════════════════════
static HWND hComboProvider = nullptr;
static HWND hEditHost = nullptr, hEditPort = nullptr, hEditApiKey = nullptr;
static HWND hEditModel = nullptr, hEditTemp = nullptr, hEditMaxTok = nullptr;
static HWND hEditCtx = nullptr, hEditGpu = nullptr, hEditModelPath = nullptr;
static HWND hCheckAutoStart = nullptr;
static HWND hLabelStatus = nullptr;
static HFONT hSettingsFont = nullptr;
static HFONT hSettingsFontBold = nullptr;

static void PopulateSettingsFromConfig(bool setCombo = true) {
    if (setCombo) SendMessageW(hComboProvider, CB_SETCURSEL, (WPARAM)g_config.provider, 0);
    SetWindowTextW(hEditHost,    StringToWString(g_config.host).c_str());
    SetWindowTextW(hEditPort,    std::to_wstring(g_config.port).c_str());
    SetWindowTextW(hEditApiKey,  StringToWString(g_config.apiKey).c_str());
    SetWindowTextW(hEditModel,   StringToWString(g_config.model).c_str());

    wchar_t tempBuf[32]; swprintf_s(tempBuf, L"%.2f", g_config.temperature);
    SetWindowTextW(hEditTemp, tempBuf);
    SetWindowTextW(hEditMaxTok,  std::to_wstring(g_config.maxTokens).c_str());
    SetWindowTextW(hEditCtx,     std::to_wstring(g_config.contextSize).c_str());
    SetWindowTextW(hEditGpu,     std::to_wstring(g_config.gpuLayers).c_str());
    SetWindowTextW(hEditModelPath, StringToWString(g_config.modelPath).c_str());
    SendMessageW(hCheckAutoStart, BM_SETCHECK, g_config.autoStartEngine ? BST_CHECKED : BST_UNCHECKED, 0);
}

static void OnProviderChanged() {
    int sel = (int)SendMessageW(hComboProvider, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= PROV_COUNT) return;
    const auto& p = g_providerPresets[sel];
    SetWindowTextW(hEditHost, StringToWString(p.defaultHost).c_str());
    SetWindowTextW(hEditPort, std::to_wstring(p.defaultPort).c_str());
    SetWindowTextW(hEditModel, StringToWString(p.defaultModel).c_str());
    // Don't clear API key — user may have entered it
    SetWindowTextW(hLabelStatus, L"");
}

LRESULT CALLBACK SettingsWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        hSettingsFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        hSettingsFontBold = CreateFontW(15, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        auto MakeLabel = [&](const wchar_t* text, int y) {
            HWND lbl = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_RIGHT, 10, y + 3, 105, 20, h, 0, 0, 0);
            SendMessageW(lbl, WM_SETFONT, (WPARAM)hSettingsFont, TRUE);
        };
        auto MakeEdit = [&](int id, int y, int width = 280) -> HWND {
            HWND e = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 120, y, width, 24, h, (HMENU)(INT_PTR)id, 0, 0);
            SendMessageW(e, WM_SETFONT, (WPARAM)hSettingsFont, TRUE);
            return e;
        };

        int y = 12;
        // Title
        HWND hTitle = CreateWindowExW(0, L"STATIC", L"Nova Settings", WS_CHILD | WS_VISIBLE, 10, y, 400, 24, h, 0, 0, 0);
        SendMessageW(hTitle, WM_SETFONT, (WPARAM)hSettingsFontBold, TRUE); y += 32;

        // Provider dropdown
        MakeLabel(L"Provider:", y);
        hComboProvider = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                         120, y, 280, 400, h, (HMENU)IDC_COMBO_PROV, 0, 0);
        SendMessageW(hComboProvider, WM_SETFONT, (WPARAM)hSettingsFont, TRUE);
        for (int i = 0; i < PROV_COUNT; i++)
            SendMessageW(hComboProvider, CB_ADDSTRING, 0, (LPARAM)g_providerPresets[i].displayName);
        y += 30;

        MakeLabel(L"Host:",        y); hEditHost     = MakeEdit(IDC_EDIT_HOST, y);     y += 28;
        MakeLabel(L"Port:",        y); hEditPort     = MakeEdit(IDC_EDIT_PORT, y, 80); y += 28;
        MakeLabel(L"API Key:",     y); hEditApiKey   = MakeEdit(IDC_EDIT_APIKEY, y);   y += 28;
        MakeLabel(L"Model:",       y); hEditModel    = MakeEdit(IDC_EDIT_MODEL, y);    y += 28;
        MakeLabel(L"Temperature:", y); hEditTemp     = MakeEdit(IDC_EDIT_TEMP, y, 80); y += 28;
        MakeLabel(L"Max Tokens:",  y); hEditMaxTok   = MakeEdit(IDC_EDIT_MAXTOK, y, 80); y += 28;
        MakeLabel(L"Context Size:", y); hEditCtx     = MakeEdit(IDC_EDIT_CTX, y, 80);  y += 28;
        MakeLabel(L"GPU Layers:",  y); hEditGpu      = MakeEdit(IDC_EDIT_GPU, y, 80);  y += 28;
        MakeLabel(L"Model Path:",  y); hEditModelPath = MakeEdit(IDC_EDIT_MODPATH, y); y += 28;

        // Auto-start checkbox
        hCheckAutoStart = CreateWindowExW(0, L"BUTTON", L"Auto-start local engine",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 120, y, 280, 22, h, (HMENU)IDC_CHECK_AUTO, 0, 0);
        SendMessageW(hCheckAutoStart, WM_SETFONT, (WPARAM)hSettingsFont, TRUE);
        y += 32;

        // Buttons
        HWND hBtnTest = CreateWindowExW(0, L"BUTTON", L"Test Connection", WS_CHILD | WS_VISIBLE,
                                         120, y, 130, 30, h, (HMENU)IDC_TEST_BTN, 0, 0);
        SendMessageW(hBtnTest, WM_SETFONT, (WPARAM)hSettingsFont, TRUE);
        HWND hBtnSave = CreateWindowExW(0, L"BUTTON", L"Save Settings", WS_CHILD | WS_VISIBLE,
                                         260, y, 130, 30, h, (HMENU)IDC_SAVE_BTN, 0, 0);
        SendMessageW(hBtnSave, WM_SETFONT, (WPARAM)hSettingsFont, TRUE);
        y += 38;

        // Status label
        hLabelStatus = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
                                        10, y, 400, 24, h, (HMENU)IDC_STATUS_LBL, 0, 0);
        SendMessageW(hLabelStatus, WM_SETFONT, (WPARAM)hSettingsFont, TRUE);

        // Populate fields from current config
        PopulateSettingsFromConfig();
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDC_COMBO_PROV:
            if (HIWORD(w) == CBN_SELCHANGE) OnProviderChanged();
            break;

        case IDC_TEST_BTN: {
            SetWindowTextW(hLabelStatus, L"\u23F3  Testing connection...");
            std::thread([h]() {
                // Quick connectivity test
                wchar_t buf[512];
                GetWindowTextW(hEditHost, buf, 512); std::string testHost = WStringToString(buf);
                GetWindowTextW(hEditPort, buf, 512); int testPort = _wtoi(buf);

                HINTERNET hS = InternetOpenW(L"NovaTest", 1, 0, 0, 0);
                DWORD to = 5000;
                InternetSetOptionW(hS, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
                InternetSetOptionW(hS, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));

                bool ok = false;
                if (hS) {
                    HINTERNET hC = InternetConnectW(hS, StringToWString(testHost).c_str(),
                                                    (INTERNET_PORT)testPort, 0, 0, 3, 0, 0);
                    if (hC) {
                        HINTERNET hR = HttpOpenRequestW(hC, L"GET", L"/", 0, 0, 0,
                            INTERNET_FLAG_RELOAD | (testPort == 443 ? INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID : 0), 0);
                        if (hR) { ok = HttpSendRequestA(hR, 0, 0, 0, 0) ? true : false; InternetCloseHandle(hR); }
                        InternetCloseHandle(hC);
                    }
                    InternetCloseHandle(hS);
                }
                PostMessageW(h, WM_APP + 100, (WPARAM)ok, 0);
            }).detach();
            break;
        }

        case IDC_SAVE_BTN: {
            wchar_t buf[512];
            int sel = (int)SendMessageW(hComboProvider, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < PROV_COUNT) {
                g_config.provider     = (ProviderType)sel;
                g_config.useSSL       = g_providerPresets[sel].needsSSL;
                g_config.endpointPath = g_providerPresets[sel].defaultEndpoint;
            }

            GetWindowTextW(hEditHost, buf, 512);     g_config.host = WStringToString(buf);
            GetWindowTextW(hEditPort, buf, 512);     g_config.port = _wtoi(buf);
            GetWindowTextW(hEditApiKey, buf, 512);   g_config.apiKey = WStringToString(buf);
            GetWindowTextW(hEditModel, buf, 512);    g_config.model = WStringToString(buf);
            GetWindowTextW(hEditTemp, buf, 512);     g_config.temperature = (float)_wtof(buf);
            GetWindowTextW(hEditMaxTok, buf, 512);   g_config.maxTokens = _wtoi(buf);
            GetWindowTextW(hEditCtx, buf, 512);      g_config.contextSize = _wtoi(buf);
            GetWindowTextW(hEditGpu, buf, 512);      g_config.gpuLayers = _wtoi(buf);
            GetWindowTextW(hEditModelPath, buf, 512); g_config.modelPath = WStringToString(buf);
            g_config.autoStartEngine = (SendMessageW(hCheckAutoStart, BM_GETCHECK, 0, 0) == BST_CHECKED);

            SaveConfig();
            SetWindowTextW(hLabelStatus, L"\u2705  Settings saved!");
            if (hIndicator) InvalidateRect(hIndicator, nullptr, FALSE);
            break;
        }
        }
        return 0;

    case WM_APP + 100:  // Test result callback
        SetWindowTextW(hLabelStatus, w ? L"\u2705  Connection successful!" : L"\u274C  Connection failed. Check settings.");
        return 0;

    case WM_CLOSE:
        DestroyWindow(h);
        return 0;

    case WM_DESTROY:
        if (hSettingsFont) { DeleteObject(hSettingsFont); hSettingsFont = nullptr; }
        if (hSettingsFontBold) { DeleteObject(hSettingsFontBold); hSettingsFontBold = nullptr; }
        hSettingsWnd = nullptr;
        hComboProvider = nullptr;
        hEditHost = hEditPort = hEditApiKey = hEditModel = nullptr;
        hEditTemp = hEditMaxTok = hEditCtx = hEditGpu = hEditModelPath = nullptr;
        hCheckAutoStart = nullptr;
        hLabelStatus = nullptr;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void ShowSettingsDialog(HWND parent) {
    if (hSettingsWnd) { SetForegroundWindow(hSettingsWnd); return; } // Don't open twice

    static bool classRegistered = false;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    if (!classRegistered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = SettingsWndProc;
        wc.hInstance      = hInst;
        wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName  = L"NovaSettings";
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    hSettingsWnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"NovaSettings", L"Nova Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 440, 560, parent, 0, hInst, 0);
}
// ════════════════════════════════════════════════════════════════
// LAYOUT
// ════════════════════════════════════════════════════════════════
void LayoutControls(HWND hwnd) {
    RECT r; GetClientRect(hwnd, &r);
    int W = r.right, H = r.bottom;
    int fontH = 18;
    HDC hdc = GetDC(hEditInput);
    if (hdc) {
        HFONT hOldF = (HFONT)SelectObject(hdc, hFontMain);
        TEXTMETRICW tm = {}; GetTextMetricsW(hdc, &tm);
        SelectObject(hdc, hOldF); ReleaseDC(hEditInput, hdc);
        if (tm.tmHeight > 0) fontH = tm.tmHeight;
    }

    const int PAD      = 7;
    const int INPUT_H  = fontH + PAD * 2;
    const int LABEL_H  = 10;
    const int BTN_H    = 32;
    const int INPUT_Y  = H - 12 - INPUT_H;
    const int LABEL_Y  = INPUT_Y - 4 - LABEL_H;
    const int BTN_Y    = LABEL_Y - 2 - BTN_H;
    const int DISP_H   = BTN_Y - 10 - 42;

    SetWindowPos(hIndicator, 0, 12, 8, W - 24, 30, SWP_NOZORDER);
    SetWindowPos(hEditDisplay, 0, 15, 42, W - 30, DISP_H, SWP_NOZORDER);

    // 6 buttons: 75px each, 7px gap = 485px total
    const int BTN_W = 75, GAP = 7;
    int totalW = BTN_W * 6 + GAP * 5;
    int x = (W - totalW) / 2;
    SetWindowPos(hButtonSend,     0, x,                   BTN_Y, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonClear,    0, x + (BTN_W + GAP),   BTN_Y, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonMute,     0, x + (BTN_W + GAP)*2, BTN_Y, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonDev,      0, x + (BTN_W + GAP)*3, BTN_Y, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonAttach,   0, x + (BTN_W + GAP)*4, BTN_Y, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonSettings, 0, x + (BTN_W + GAP)*5, BTN_Y, BTN_W, BTN_H, SWP_NOZORDER);

    SetWindowPos(hAttachLabel, 0, 15, LABEL_Y, W - 30, LABEL_H, SWP_NOZORDER);
    SetWindowPos(hEditInput, 0, 15, INPUT_Y, W - 30, INPUT_H, SWP_NOZORDER);

    RECT rcC = {}; GetClientRect(hEditInput, &rcC);
    int topPad = std::max<int>(1, (int)((rcC.bottom - fontH) / 2));
    RECT rTxt = { 2, topPad, rcC.right - 2, rcC.bottom - topPad };
    SendMessageW(hEditInput, EM_SETRECT, 0, (LPARAM)&rTxt);
}

// ════════════════════════════════════════════════════════════════
// EDIT SUBCLASS (Enter key, focus highlight)
// ════════════════════════════════════════════════════════════════
LRESULT CALLBACK EditSubclassProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CHAR && w == VK_RETURN) return 0;
    if (m == WM_KEYDOWN && w == VK_RETURN) { ProcessChat(); return 0; }
    if (m == WM_SETFOCUS || m == WM_KILLFOCUS) {
        LRESULT res = CallWindowProcW(OldEditProc, h, m, w, l);
        RedrawWindow(h, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN);
        return res;
    }
    if (m == WM_NCPAINT) {
        CallWindowProcW(OldEditProc, h, m, w, l);
        HDC hdc = GetWindowDC(h);
        if (hdc) {
            RECT rc; GetWindowRect(h, &rc); OffsetRect(&rc, -rc.left, -rc.top);
            COLORREF col = (GetFocus() == h) ? RGB(0, 120, 215) : RGB(180, 180, 180);
            HPEN hPen = CreatePen(PS_SOLID, 2, col);
            HBRUSH hNull = (HBRUSH)GetStockObject(NULL_BRUSH);
            HPEN hOldP = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldB = (HBRUSH)SelectObject(hdc, hNull);
            Rectangle(hdc, rc.left+1, rc.top+1, rc.right-1, rc.bottom-1);
            SelectObject(hdc, hOldP); SelectObject(hdc, hOldB);
            DeleteObject(hPen); ReleaseDC(h, hdc);
        }
        return 0;
    }
    return CallWindowProcW(OldEditProc, h, m, w, l);
}

// ════════════════════════════════════════════════════════════════
// APP STATE
// ════════════════════════════════════════════════════════════════
void SetAppState(AppState s) {
    g_appState.store(s);
    if (hIndicator) { InvalidateRect(hIndicator, nullptr, FALSE); UpdateWindow(hIndicator); }
}

// ════════════════════════════════════════════════════════════════
// MAIN WINDOW PROC
// ════════════════════════════════════════════════════════════════
LRESULT CALLBACK WindowProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_SIZE: LayoutControls(h); return 0;
    case WM_GETMINMAXINFO: ((MINMAXINFO*)l)->ptMinTrackSize = { MIN_WIN_W, MIN_WIN_H }; return 0;

    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDC_BTN_SEND: ProcessChat(); break;

        case IDC_BTN_CLEAR:
            SetWindowTextW(hEditDisplay, L"");
            { std::lock_guard<std::mutex> lk(historyMutex); conversationHistory.clear(); }
            ClearAttachment();
            SaveHistory(); break;

        case IDC_BTN_MUTE:
            g_muted = !g_muted;
            if (g_muted) { std::lock_guard<std::mutex> lk(g_voiceMutex); if (g_pVoice) g_pVoice->Speak(L"", SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr); }
            SetWindowTextW(hButtonMute, g_muted ? L"Unmute" : L"Mute"); break;

        case IDC_BTN_DEV:
            if (!consoleAllocated) {
                AllocConsole();
                SetConsoleTitleW(L"Nova Dev Console");
                FILE* fOut = nullptr, * fErr = nullptr;
                freopen_s(&fOut, "CONOUT$", "w", stdout);
                freopen_s(&fErr, "CONOUT$", "w", stderr);
                HWND hCon = GetConsoleWindow();
                if (hCon) { HMENU hMenu = GetSystemMenu(hCon, FALSE); if (hMenu) DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND); }
                consoleAllocated = true;
                std::ifstream logIn(GetExeDir() + g_devLogFile);
                if (logIn) { std::string logLine; while (std::getline(logIn, logLine)) printf("%s\n", logLine.c_str());
                             printf("--- (end of buffered log) ---\n"); fflush(stdout); }
                DevLog("Dev Console attached — live logging active\n");
            }
            break;

        case IDC_BTN_ATTACH: OpenAttachDialog(); break;

        case IDC_BTN_SETTINGS: ShowSettingsDialog(h); break;
        }
        return 0;

    case WM_AI_DONE: {
        bool ok = (bool)w;
        WCHAR* heapStr = (WCHAR*)l;
        std::wstring reply = heapStr ? heapStr : L"";
        delete[] heapStr;

        if (reply.size() > 10000) reply = reply.substr(0, 10000) + L"\n[Truncated]";
        for (auto& c : reply) if (c < 32 && c != '\r' && c != '\n') c = L' ';

        std::string cleanReply = WStringToString(reply);
        if (ok) {
            // Parse EXEC: commands
            std::vector<std::string> cmds;
            std::istringstream scanner(cleanReply);
            std::string line;
            while (std::getline(scanner, line)) {
                size_t start = line.find_first_not_of(" \t\r");
                if (start == std::string::npos) continue;
                line = line.substr(start);
                if (line.compare(0, 5, "EXEC:") != 0) continue;

                std::string cmd = line.substr(5);
                size_t cs = cmd.find_first_not_of(" \t"), ce = cmd.find_last_not_of(" \t\r\n");
                if (cs == std::string::npos) continue;
                cmd = cmd.substr(cs, ce - cs + 1);

                // Advanced splitter for Set-Content && cl chains
                std::string lowerCmd = cmd;
                std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), [](unsigned char c) { return (char)::tolower(c); });

                if (lowerCmd.find("set-content") != std::string::npos && lowerCmd.find("&& cl") != std::string::npos) {
                    size_t clPos = lowerCmd.find("&& cl");
                    size_t splitAt = clPos;
                    for (size_t i = clPos; i-- > 0; ) {
                        if (cmd[i] == '\'' || cmd[i] == '"') { splitAt = i + 1; break; }
                    }
                    std::string part1 = cmd.substr(0, splitAt);
                    std::string part2 = cmd.substr(clPos + 2);
                    auto trim = [](std::string& s) {
                        size_t a = s.find_first_not_of(" \t\r\n\"'"), b = s.find_last_not_of(" \t\r\n\"'");
                        if (a != std::string::npos) s = s.substr(a, b - a + 1); else s.clear();
                    };
                    trim(part1); trim(part2);
                    if (!part1.empty()) cmds.push_back(part1);
                    if (!part2.empty()) cmds.push_back(part2);
                } else {
                    cmds.push_back(cmd);
                }
            }

            if (!cmds.empty()) {
                std::thread([cmds]() mutable {
                    for (const auto& c : cmds) ExecuteNovaCommand(c);
                    PostMessageW(hMainWnd, WM_EXEC_DONE, 0, 0);
                }).detach();
            }
        }

        AppendRichText(hEditDisplay, L"Nova: ", true, RGB(0, 120, 215));
        AppendRichText(hEditDisplay, (ok ? reply : L"[No response]") + L"\r\n\r\n", false, RGB(30, 30, 30));

        EnableWindow(hButtonSend, TRUE);
        aiRunning = false;
        SetAppState(ok ? AppState::Online : AppState::Offline);
        SetFocus(hEditInput);
        return 0;
    }

    case WM_ENGINE_READY:
        DevLog("[System] Engine confirmed live — UI unlocked\n");
        SetAppState(AppState::Online);
        return 0;

    case WM_EXEC_DONE:
        DevLog("[System] Background command execution complete\n");
        return 0;

    case WM_CLOSE:
        if (aiRunning && MessageBoxW(h, L"Nova is thinking. Exit?", L"Nova", MB_YESNO) != IDYES) return 0;
        DestroyWindow(h); return 0;

    case WM_DESTROY:
        StopLocalEngine();
        Gdiplus::GdiplusShutdown(g_gdipToken);
        DeleteObject(hFontMain); DeleteObject(hFontBtn); DeleteObject(hFontIndicator);
        { std::lock_guard<std::mutex> lk(g_voiceMutex); if (g_pVoice) { g_pVoice->Release(); g_pVoice = nullptr; } }
        CoUninitialize(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ════════════════════════════════════════════════════════════════
// ENTRY POINT
// ════════════════════════════════════════════════════════════════
int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    
    // TTS Setup — Female American Voice (Zira)
    if (SUCCEEDED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&g_pVoice))) {
	g_pVoice->SetRate(4);
        ISpObjectTokenCategory* pCat = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, NULL, CLSCTX_ALL, IID_ISpObjectTokenCategory, (void**)&pCat))) {
            if (SUCCEEDED(pCat->SetId(SPCAT_VOICES, FALSE))) {
                IEnumSpObjectTokens* pEnum = nullptr;
                // Filter for Female, US English (409)
                if (SUCCEEDED(pCat->EnumTokens(L"Gender=Female;Language=409", NULL, &pEnum))) {
                    ISpObjectToken* pToken = nullptr;
                    if (SUCCEEDED(pEnum->Next(1, &pToken, NULL))) {
                        g_pVoice->SetVoice(pToken);
                        pToken->Release();
                    }
                    pEnum->Release();
                }
            }
            pCat->Release();
        }
        g_pVoice->SetVolume(100);
        g_pVoice->SetRate(-1); 
    }

    Gdiplus::GdiplusStartupInput gdipInput;
    Gdiplus::GdiplusStartup(&g_gdipToken, &gdipInput, NULL);
    LoadLibraryW(L"msftedit.dll");
    InitCommonControls();

    // Load config FIRST — everything depends on this
    LoadConfig();

    hFontMain      = CreateFontW(17,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    hFontBtn       = CreateFontW(15,0,0,0,FW_MEDIUM,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    hFontIndicator = CreateFontW(13,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");

    // Register indicator class
    WNDCLASSEXW ic = { sizeof(ic) };
    ic.style = CS_HREDRAW | CS_VREDRAW;
    ic.lpfnWndProc   = IndicatorWndProc;
    ic.hInstance      = hI;
    ic.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    ic.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    ic.lpszClassName  = L"IndicatorCtrl";
    RegisterClassExW(&ic);

    // Register main window class
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance      = hI;
    wc.hCursor        = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName  = L"NovaMain";
    wc.hIcon          = LoadIcon(hI, MAKEINTRESOURCE(1));
    wc.hIconSm        = LoadIcon(hI, MAKEINTRESOURCE(1));
    RegisterClassExW(&wc);

    // Calculate centered coordinates for a narrower, sleeker window
    int winW = 600; // The new narrower width
    int winH = 650; // The new height
    int winX = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
    int winY = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;

    // Create main window and controls centered on the screen
    hMainWnd = CreateWindowExW(0, L"NovaMain", L"Nova", WS_OVERLAPPEDWINDOW,
                               winX, winY, winW, winH, 0, 0, hI, 0);

    hIndicator    = CreateWindowExW(0, L"IndicatorCtrl", L"", WS_CHILD | WS_VISIBLE, 0,0,0,0, hMainWnd, 0, hI, 0);
    hEditDisplay  = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
                                     WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 0,0,0,0, hMainWnd, 0, hI, 0);
    hEditInput    = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
                                     WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 0,0,0,0, hMainWnd, 0, hI, 0);

    hButtonSend     = CreateWindowExW(0, L"BUTTON", L"Send",     WS_CHILD | WS_VISIBLE, 0,0,0,0, hMainWnd, (HMENU)IDC_BTN_SEND, hI, 0);
    hButtonClear    = CreateWindowExW(0, L"BUTTON", L"Clear",    WS_CHILD | WS_VISIBLE, 0,0,0,0, hMainWnd, (HMENU)IDC_BTN_CLEAR, hI, 0);
    hButtonMute     = CreateWindowExW(0, L"BUTTON", L"Mute",     WS_CHILD | WS_VISIBLE, 0,0,0,0, hMainWnd, (HMENU)IDC_BTN_MUTE, hI, 0);
    hButtonDev      = CreateWindowExW(0, L"BUTTON", L"Dev",      WS_CHILD | WS_VISIBLE, 0,0,0,0, hMainWnd, (HMENU)IDC_BTN_DEV, hI, 0);
    hButtonAttach   = CreateWindowExW(0, L"BUTTON", L"Attach",   WS_CHILD | WS_VISIBLE, 0,0,0,0, hMainWnd, (HMENU)IDC_BTN_ATTACH, hI, 0);
    hButtonSettings = CreateWindowExW(0, L"BUTTON", L"\u2699",   WS_CHILD | WS_VISIBLE, 0,0,0,0, hMainWnd, (HMENU)IDC_BTN_SETTINGS, hI, 0);
    hAttachLabel    = CreateWindowExW(0, L"STATIC", L"",         WS_CHILD | WS_VISIBLE | SS_CENTER, 0,0,0,0, hMainWnd, 0, hI, 0);

    // Apply fonts
    for (HWND ctrl : { hEditDisplay, hEditInput }) SendMessageW(ctrl, WM_SETFONT, (WPARAM)hFontMain, TRUE);
    for (HWND ctrl : { hButtonSend, hButtonClear, hButtonMute, hButtonDev, hButtonAttach, hButtonSettings })
        SendMessageW(ctrl, WM_SETFONT, (WPARAM)hFontBtn, TRUE);
    SendMessageW(hAttachLabel, WM_SETFONT, (WPARAM)hFontIndicator, TRUE);
    SendMessageW(hEditInput, EM_EXLIMITTEXT, 0, (LPARAM)-1);

    OldEditProc = (WNDPROC)SetWindowLongPtrW(hEditInput, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

    // 1. Setup UI
    LoadHistory();
    LayoutControls(hMainWnd);

    // 2. Start engine and WAIT for it (the 5s buffer happens here)
    StartLocalEngine();

    // 3. Finally show the window
    ShowWindow(hMainWnd, SW_SHOW);
    SetAppState(AppState::Online);
    SetFocus(hEditInput);

    DevLog("=== Nova Session Started ===\n");
    DevLog("Provider   : %S\n", g_providerPresets[g_config.provider].displayName);
    DevLog("Host       : %s:%d\n", g_config.host.c_str(), g_config.port);
    DevLog("Model      : %s\n", g_config.model.c_str());
    DevLog("SSL        : %s\n", g_config.useSSL ? "yes" : "no");
    DevLog("Exe dir    : %s\n", GetExeDir().c_str());
    DevLog("History    : %zu chars\n", conversationHistory.size());
    DevLog("TTS Voice  : %s\n", g_pVoice ? "Ready" : "NOT INITIALIZED");
    DevLog("==================================\n");

    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return (int)msg.wParam;
}
