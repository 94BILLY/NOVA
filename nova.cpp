/* * ============================================================================
 * NOVA v 1.0.0 The Executive Desktop Assistant
 * Copyright (C) 2026 [94BILLY]. All Rights Reserved.
 * * PROPRIETARY AND CONFIDENTIAL:
 * This software and its source code are the sole property of the author. 
 * Unauthorized copying, distribution, or modification of this file, 
 * via any medium, is strictly prohibited.
 *
 * RELEASE NOTES v1.0:
 *   - Unified 17-provider backend (local + cloud)
 *   - Protocol adapters: OpenAI-compat, Anthropic Messages, Gemini, llama-legacy
 *   - Full Settings dialog with provider presets, model/API key config
 *   - Auto-detection of local backends (llama-server, Ollama, LM Studio, etc.)
 *   - Config persistence in nova_config.ini
 *   - All original features: EXEC engine, TTS, attachments,
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
// GDI+ headers use bare min/max — NOMINMAX blocks the Windows macros,
// so we pull them in from <algorithm> before GDI+ sees them.
using std::min;
using std::max;
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
#include <gdiplus.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <shlobj.h>
#include <map>

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

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define IDT_PULSE       1002
#define PULSE_INTERVAL  40
#define MIN_WIN_W       680
#define MIN_WIN_H       500
#define WM_AI_DONE      (WM_APP + 1)
#define WM_ENGINE_READY (WM_APP + 2)
#define WM_EXEC_DONE    (WM_APP + 3)

// Settings dialog control IDs
#define IDC_PROVIDER_COMBO  2001
#define IDC_HOST_EDIT       2002
#define IDC_PORT_EDIT       2003
#define IDC_APIKEY_EDIT     2004
#define IDC_MODEL_EDIT      2005
#define IDC_ENDPOINT_EDIT   2006
#define IDC_SSL_CHECK       2007
#define IDC_TEMP_EDIT       2008
#define IDC_MAXTOK_EDIT     2009
#define IDC_BTN_SAVE        2010
#define IDC_BTN_TEST        2011
#define IDC_AUTOSTART_CHECK 2012
#define IDC_CTXSIZE_EDIT    2013
#define IDC_GPULAYERS_EDIT  2014

enum class AppState { Online, Busy, Offline };

// ── Protocol Adapters ────────────────────────────────────────────
enum class Protocol { LlamaLegacy, OpenAICompat, Anthropic, Gemini };

struct ProviderPreset {
    const wchar_t* displayName;
    const char*    defaultHost;
    int            defaultPort;
    const char*    defaultModel;
    const char*    defaultEndpoint;
    bool           useSSL;
    Protocol       protocol;
};

static const ProviderPreset g_providerPresets[] = {
    { L"Local (llama-server)",   "127.0.0.1",                              11434, "",                                "/completion",                  false, Protocol::LlamaLegacy  },
    { L"Ollama",                 "127.0.0.1",                              11434, "llama3",                          "/v1/chat/completions",         false, Protocol::OpenAICompat },
    { L"LM Studio",             "127.0.0.1",                              1234,  "",                                "/v1/chat/completions",         false, Protocol::OpenAICompat },
    { L"vLLM",                   "127.0.0.1",                              8000,  "",                                "/v1/chat/completions",         false, Protocol::OpenAICompat },
    { L"KoboldCpp",              "127.0.0.1",                              5001,  "",                                "/v1/chat/completions",         false, Protocol::OpenAICompat },
    { L"Jan",                    "127.0.0.1",                              1337,  "",                                "/v1/chat/completions",         false, Protocol::OpenAICompat },
    { L"GPT4All",                "127.0.0.1",                              4891,  "",                                "/v1/chat/completions",         false, Protocol::OpenAICompat },
    { L"text-gen-webui",         "127.0.0.1",                              5000,  "",                                "/v1/chat/completions",         false, Protocol::OpenAICompat },
    { L"OpenAI",                 "api.openai.com",                         443,   "gpt-4o-mini",                     "/v1/chat/completions",         true,  Protocol::OpenAICompat },
    { L"Anthropic",              "api.anthropic.com",                      443,   "claude-sonnet-4-20250514",        "/v1/messages",                 true,  Protocol::Anthropic    },
    { L"Google Gemini",          "generativelanguage.googleapis.com",      443,   "gemini-2.0-flash",                "/v1beta/models/",              true,  Protocol::Gemini       },
    { L"Groq",                   "api.groq.com",                           443,   "llama-3.3-70b-versatile",         "/openai/v1/chat/completions",  true,  Protocol::OpenAICompat },
    { L"Mistral",                "api.mistral.ai",                         443,   "mistral-small-latest",            "/v1/chat/completions",         true,  Protocol::OpenAICompat },
    { L"Together AI",            "api.together.xyz",                       443,   "meta-llama/Llama-3-8b-chat-hf",   "/v1/chat/completions",         true,  Protocol::OpenAICompat },
    { L"OpenRouter",             "openrouter.ai",                          443,   "meta-llama/llama-3-8b-instruct",  "/api/v1/chat/completions",     true,  Protocol::OpenAICompat },
    { L"xAI Grok",               "api.x.ai",                               443,   "grok-2",                          "/v1/chat/completions",         true,  Protocol::OpenAICompat },
    { L"Custom (OpenAI-compat)", "127.0.0.1",                              8080,  "",                                "/v1/chat/completions",         false, Protocol::OpenAICompat },
};
constexpr int PROVIDER_COUNT = sizeof(g_providerPresets) / sizeof(g_providerPresets[0]);

// ── Configuration ────────────────────────────────────────────────
struct NovaConfig {
    int         provider       = 0;
    std::string host           = "127.0.0.1";
    int         port           = 11434;
    std::string apiKey;
    std::string model;
    std::string endpointPath   = "/completion";
    bool        useSSL         = false;
    float       temperature    = 0.4f;
    int         maxTokens      = 1024;
    int         contextSize    = 8192;
    int         gpuLayers      = 99;
    bool        autoStartEngine= true;
    std::string modelPath      = "models\\llama3.gguf";
    int         enginePort     = 11434;
};

static NovaConfig g_config;

// ── Attachment ────────────────────────────────────────────────────
struct Attachment {
    std::wstring path;
    std::wstring displayName;
    std::string  textContent;
    bool         isImage  = false;
    bool         isAudio  = false;
    bool         isText   = false;
    bool         isVideo  = false;
};

struct ChatRequest {
    std::wstring userText;
    bool         hasAttachment = false;
    Attachment   attachment;
};

HWND  hMainWnd;
HWND  hEditDisplay, hEditInput;
HWND  hButtonSend, hButtonClear, hButtonMute, hButtonDev, hButtonAttach, hButtonSettings;
HWND  hIndicator, hAttachLabel;
HFONT hFontMain, hFontBtn, hFontIndicator;
WNDPROC OldEditProc;

// Settings dialog
HWND  g_hSettingsWnd = nullptr;
HFONT hFontSettings = nullptr;
HWND  hComboProvider, hEditHost, hEditPort, hEditApiKey, hEditModel;
HWND  hEditEndpoint, hCheckSSL, hEditTemp, hEditMaxTok;
HWND  hCheckAutoStart, hEditCtxSize, hEditGpuLayers;
HWND  hLabelStatus;

bool       g_hasAttachment = false;
Attachment g_attachment;

std::mutex        historyMutex;
std::wstring      conversationHistory;
std::atomic<bool> aiRunning(false);
std::atomic<bool> g_muted(false);
bool              consoleAllocated = false;

ISpVoice* g_pVoice    = nullptr;
std::mutex g_voiceMutex;

const size_t      MAX_HISTORY_CHARS = 8000;
const std::string g_historyFile     = "nova_history.txt";
const std::string g_personalityFile = "nova_personality.txt";
const std::string g_configFile      = "nova_config.ini";

AppState  g_appState  = AppState::Online;
float     g_pulseT    = 0.0f;
ULONG_PTR g_gdipToken = 0;

// ────────────────────────────────────────────────────────────────
// DEV LOGGER
// ────────────────────────────────────────────────────────────────
std::string GetExeDir();

const std::string g_devLogFile = "nova_dev_log.txt";

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

// ────────────────────────────────────────────────────────────────
// LOCAL AI ENGINE MANAGEMENT
// ────────────────────────────────────────────────────────────────
PROCESS_INFORMATION g_serverPi = {};

bool IsServerAlreadyRunning() {
    HINTERNET hS = InternetOpenW(L"NovaProbe", 1, 0, 0, 0);
    if (!hS) return false;
    DWORD timeout = 1000;
    InternetSetOptionA(hS, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hS, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    std::wstring hostW = L"127.0.0.1";
    INTERNET_PORT checkPort = (INTERNET_PORT)g_config.enginePort;

    HINTERNET hC = InternetConnectW(hS, hostW.c_str(), checkPort, 0, 0, 3, 0, 0);
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
    if (IsServerAlreadyRunning()) {
        DevLog("[System] llama-server already running on :%d — skipping launch\n", g_config.enginePort);
        return;
    }

    DevLog("[System] Starting embedded llama-server engine...\n");
    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    char cmd[512];
    sprintf_s(cmd, "engine\\llama-server.exe -m %s --port %d -c %d -ngl %d",
              g_config.modelPath.c_str(), g_config.enginePort,
              g_config.contextSize, g_config.gpuLayers);
    
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &g_serverPi)) {
        CloseHandle(g_serverPi.hThread);
        g_serverPi.hThread = NULL;
        DevLog("[System] Local engine launched (PID: %lu)\n", g_serverPi.dwProcessId);
        DevLog("[System] Waiting for engine to warm up...\n");
        for (int i = 0; i < 30; i++) {
            Sleep(1000);
            if (IsServerAlreadyRunning()) {
                DevLog("[System] Engine ready after %d seconds\n", i + 1);
                return;
            }
        }
        DevLog("[System] WARNING: Engine did not respond within 30s\n");
    } else {
        DevLog("[System] ERROR: Failed to start local engine. GLE=%lu\n", GetLastError());
    }
}

void StopLocalEngine() {
    if (g_serverPi.hProcess) {
        DevLog("[System] Shutting down local engine (PID: %lu)...\n", g_serverPi.dwProcessId);
        TerminateProcess(g_serverPi.hProcess, 0);
        CloseHandle(g_serverPi.hProcess);
        if (g_serverPi.hThread) CloseHandle(g_serverPi.hThread);
        g_serverPi.hProcess = NULL;
    } else {
        DevLog("[System] Engine was externally managed — not killing\n");
    }
}

// ────────────────────────────────────────────────────────────────
// FORWARD DECLARATIONS
// ────────────────────────────────────────────────────────────────
void AppendRichText(HWND hRich, const std::wstring& text, bool bBold, COLORREF color = RGB(30, 30, 30));
std::string PrecisionEscape(const std::string& s);
std::string DecodeJsonString(const std::string& json, const std::string& key);
std::string WStringToString(const std::wstring& w);
std::wstring StringToWString(const std::string& s);
std::string UrlEncode(const std::string& s);
void SpeakAsync(const std::wstring& text);
std::string GetExeDir();
std::string GetDesktopDir();
void SaveHistory();
void LoadHistory();
void TrimHistory();
std::string LoadPersonality();
void SavePersonality(const std::string& n);
void EvolvePersonality(const std::string& current, const std::string& exchange); 
std::string FetchUrl(const std::string& url, const std::string& ua = "Mozilla/5.0");
std::string FetchWeather(const std::string& loc);
std::string FetchNews(const std::string& q);
std::string FetchWiki(const std::string& q);
std::string AnalyzeAndFetch(const std::string& lower, const std::string& orig);
std::string Base64Encode(const std::vector<BYTE>& data);
std::string AnalyzeImageGDIPlus(const std::wstring& path);
std::string AnalyzeWavDetailed(const std::wstring& path);
std::string AnalyzeVideoFile(const std::wstring& path, const std::string& ext);
bool LoadAttachment(const std::wstring& path, Attachment& out);
void OpenAttachDialog();
void ClearAttachment();
void OpenSettingsDialog();
void LoadConfig();
void SaveConfig();
void AIThreadFunc(std::wstring userMsg, std::string webInfo, bool hasAttach, Attachment attach);
DWORD WINAPI ChatThreadProc(LPVOID param);
void ProcessChat();
void SetAppState(AppState s);
void LayoutControls(HWND hwnd);
LRESULT CALLBACK IndicatorWndProc(HWND h, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK EditSubclassProc(HWND h, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK SettingsWndProc(HWND h, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK WindowProc(HWND h, UINT m, WPARAM w, LPARAM l);

// ────────────────────────────────────────────────────────────────
// UTILITIES
// ────────────────────────────────────────────────────────────────
std::string PrecisionEscape(const std::string& in) {
    std::ostringstream o;
    for (char c : in) {
        if      (c == '"')  o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else                o << c;
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
    if      (res.compare(0, 6, "Nova: ") == 0) res.erase(0, 6);
    else if (res.compare(0, 5, "Nova:") == 0)  res.erase(0, 5);
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
    char p[MAX_PATH];
    GetModuleFileNameA(0, p, MAX_PATH);
    std::string s(p);
    size_t last = s.find_last_of("\\/");
    return (last != std::string::npos) ? s.substr(0, last + 1) : "";
}

std::string GetDesktopDir() {
    wchar_t path[MAX_PATH];
    if (SHGetSpecialFolderPathW(NULL, path, CSIDL_DESKTOP, FALSE)) {
        return WStringToString(path) + "\\";
    }
    return "";
}

// ────────────────────────────────────────────────────────────────
// CONFIGURATION LOAD / SAVE
// ────────────────────────────────────────────────────────────────
void LoadConfig() {
    std::string path = GetExeDir() + g_configFile;
    std::ifstream f(path);
    if (!f) {
        DevLog("[Config] No config file found — using defaults\n");
        // Apply preset 0 defaults
        const auto& p = g_providerPresets[0];
        g_config.host         = p.defaultHost;
        g_config.port         = p.defaultPort;
        g_config.model        = p.defaultModel;
        g_config.endpointPath = p.defaultEndpoint;
        g_config.useSSL       = p.useSSL;
        return;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        // Trim trailing whitespace/CR
        while (!val.empty() && (val.back() == '\r' || val.back() == '\n' || val.back() == ' ')) val.pop_back();

        if      (key == "provider")          g_config.provider        = atoi(val.c_str());
        else if (key == "host")              g_config.host            = val;
        else if (key == "port")              g_config.port            = atoi(val.c_str());
        else if (key == "api_key")           g_config.apiKey          = val;
        else if (key == "model")             g_config.model           = val;
        else if (key == "endpoint_path")     g_config.endpointPath    = val;
        else if (key == "use_ssl")           g_config.useSSL          = (val == "1");
        else if (key == "temperature")       g_config.temperature     = (float)atof(val.c_str());
        else if (key == "max_tokens")        g_config.maxTokens       = atoi(val.c_str());
        else if (key == "context_size")      g_config.contextSize     = atoi(val.c_str());
        else if (key == "gpu_layers")        g_config.gpuLayers       = atoi(val.c_str());
        else if (key == "auto_start_engine") g_config.autoStartEngine = (val == "1");
        else if (key == "model_path")        g_config.modelPath       = val;
        else if (key == "engine_port")       g_config.enginePort      = atoi(val.c_str());
    }
    // Clamp provider index
    if (g_config.provider < 0 || g_config.provider >= PROVIDER_COUNT)
        g_config.provider = 0;

    DevLog("[Config] Loaded: provider=%d host=%s port=%d model=%s ssl=%d\n",
           g_config.provider, g_config.host.c_str(), g_config.port,
           g_config.model.c_str(), (int)g_config.useSSL);
}

void SaveConfig() {
    std::string path = GetExeDir() + g_configFile;
    std::ofstream f(path);
    if (!f) { DevLog("[Config] ERROR: could not save config\n"); return; }
    f << "provider="          << g_config.provider        << "\n";
    f << "host="              << g_config.host            << "\n";
    f << "port="              << g_config.port            << "\n";
    f << "api_key="           << g_config.apiKey          << "\n";
    f << "model="             << g_config.model           << "\n";
    f << "endpoint_path="     << g_config.endpointPath    << "\n";
    f << "use_ssl="           << (g_config.useSSL ? 1 : 0)<< "\n";
    f << "temperature="       << g_config.temperature     << "\n";
    f << "max_tokens="        << g_config.maxTokens       << "\n";
    f << "context_size="      << g_config.contextSize     << "\n";
    f << "gpu_layers="        << g_config.gpuLayers       << "\n";
    f << "auto_start_engine=" << (g_config.autoStartEngine ? 1 : 0) << "\n";
    f << "model_path="        << g_config.modelPath       << "\n";
    f << "engine_port="       << g_config.enginePort      << "\n";
    DevLog("[Config] Saved to %s\n", path.c_str());
}

// ────────────────────────────────────────────────────────────────
// BASE64 ENCODER
// ────────────────────────────────────────────────────────────────
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

// ────────────────────────────────────────────────────────────────
// ATTACHMENT LOADER
// ────────────────────────────────────────────────────────────────
static std::string ExtensionOf(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return "";
    std::string ext = WStringToString(path.substr(dot + 1));
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { 
        return (char)::tolower(c); 
    });
    return ext;
}

// ────────────────────────────────────────────────────────────────
// IMAGE ANALYSIS  (GDI+ pixel sampling)
// ────────────────────────────────────────────────────────────────
std::string AnalyzeImageGDIPlus(const std::wstring& path) {
    using namespace Gdiplus;
    Bitmap bmp(path.c_str());
    if (bmp.GetLastStatus() != Ok) return "ERROR: Could not load image with GDI+.";

    UINT w = bmp.GetWidth(), h = bmp.GetHeight();
    REAL dpiX = bmp.GetHorizontalResolution();
    REAL dpiY = bmp.GetVerticalResolution();

    PixelFormat pf = bmp.GetPixelFormat();
    const char* pfName = "unknown";
    if      (pf == PixelFormat32bppARGB)  pfName = "32-bit ARGB";
    else if (pf == PixelFormat32bppRGB)   pfName = "32-bit RGB";
    else if (pf == PixelFormat24bppRGB)   pfName = "24-bit RGB";
    else if (pf == PixelFormat8bppIndexed)pfName = "8-bit indexed";
    else if (pf == PixelFormat1bppIndexed)pfName = "1-bit B&W";
    else if (pf == PixelFormat16bppGrayScale) pfName = "16-bit grayscale";

    const int S = 50;
    long long rSum = 0, gSum = 0, bSum = 0, aSum = 0;
    long long brightHigh = 0, brightMid = 0, brightLow = 0;
    long long hueRed = 0, hueGreen = 0, hueBlue = 0, hueNeutral = 0;
    long long transparentPx = 0;
    int peakBright = 0, peakDark = 255;
    long long count = 0;
    long long edgeSum = 0;

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

            int maxC = max(r, max(g, b)), minC = min(r, min(g, b));
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
    int avgEdge   = (int)(edgeSum / max(1LL, count));

    const char* brightDesc = avgBright > 200 ? "very bright/high-key"
                           : avgBright > 140 ? "bright"
                           : avgBright > 100 ? "balanced mid-tone"
                           : avgBright >  60 ? "dark"
                           : "very dark/low-key";

    const char* sharpDesc = avgEdge > 30 ? "high detail / sharp"
                          : avgEdge > 15 ? "moderate detail"
                          : avgEdge >  5 ? "soft / low contrast"
                          : "very smooth / flat";

    long long coloured = hueRed + hueGreen + hueBlue;
    const char* palette;
    if      (hueNeutral > coloured * 2)   palette = "predominantly grayscale/neutral";
    else if (hueRed > hueGreen && hueRed > hueBlue)     palette = "warm (reds/oranges dominant)";
    else if (hueGreen > hueRed && hueGreen > hueBlue)   palette = "natural/green tones dominant";
    else if (hueBlue > hueRed && hueBlue > hueGreen)    palette = "cool (blues dominant)";
    else palette = "mixed/balanced colour palette";

    char buf[1024];
    sprintf_s(buf,
        "=== IMAGE ANALYSIS: \"%s\" ===\n"
        "Dimensions    : %u x %u pixels\n"
        "DPI           : %.0f x %.0f\n"
        "Aspect ratio  : %.3f:1 (%s)\n"
        "Pixel format  : %s\n"
        "Transparency  : %s\n"
        "\n"
        "COLOUR & TONE:\n"
        "Average colour: R=%d G=%d B=%d\n"
        "Average brightness: %d/255 — %s\n"
        "Brightness range: %d (darkest) to %d (brightest)\n"
        "Tone split    : %.0f%% highlights / %.0f%% midtones / %.0f%% shadows\n"
        "Colour palette: %s\n"
        "Hue breakdown : %.0f%% neutral  %.0f%% red/warm  %.0f%% green  %.0f%% blue/cool\n"
        "\n"
        "DETAIL / SHARPNESS:\n"
        "Edge density  : %d/255 — %s\n"
        "\n"
        "Analyse this image data and give detailed, insightful feedback about the photo.",
        WStringToString(path.substr(path.find_last_of(L"\\/")+1)).c_str(),
        w, h, (double)dpiX, (double)dpiY,
        (double)w / max(1u, h),
        (w > h*1.5f ? "wide/landscape" : h > w*1.5f ? "tall/portrait" : w == h ? "square" : "standard"),
        pfName,
        (transparentPx > count/10) ? "yes (significant alpha)" : (pf & PixelFormatAlpha) ? "supported but mostly opaque" : "none",
        avgR, avgG, avgB,
        avgBright, brightDesc,
        peakDark, peakBright,
        (double)brightHigh/count*100, (double)brightMid/count*100, (double)brightLow/count*100,
        palette,
        (double)hueNeutral/count*100, (double)hueRed/count*100,
        (double)hueGreen/count*100,   (double)hueBlue/count*100,
        avgEdge, sharpDesc
    );
    return buf;
}

// ────────────────────────────────────────────────────────────────
// WAV DEEP ANALYSIS
// ────────────────────────────────────────────────────────────────
std::string AnalyzeWavDetailed(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "ERROR: Could not open WAV file.";

    char riff[4]; f.read(riff, 4);
    if (std::string(riff, 4) != "RIFF") return "ERROR: Not a valid RIFF/WAV file.";
    DWORD chunkSize; f.read((char*)&chunkSize, 4);
    char wave[4]; f.read(wave, 4);
    if (std::string(wave, 4) != "WAVE") return "ERROR: Not a WAVE file.";

    WORD  audioFmt = 0, channels = 0, bitsPerSample = 0, blockAlign = 0;
    DWORD sampleRate = 0, byteRate = 0;
    DWORD dataSize = 0;
    bool  fmtFound = false;

    char id[4]; DWORD sz;
    while (f.read(id, 4) && f.read((char*)&sz, 4)) {
        std::string tag(id, 4);
        if (tag == "fmt ") {
            f.read((char*)&audioFmt,    2);
            f.read((char*)&channels,    2);
            f.read((char*)&sampleRate,  4);
            f.read((char*)&byteRate,    4);
            f.read((char*)&blockAlign,  2);
            f.read((char*)&bitsPerSample, 2);
            if (sz > 16) f.ignore(sz - 16);
            fmtFound = true;
        } else if (tag == "data") {
            dataSize = sz;
            break;
        } else {
            f.ignore(sz);
        }
    }
    if (!fmtFound) return "ERROR: Could not find fmt chunk.";

    double duration = (byteRate > 0) ? (double)dataSize / byteRate : 0.0;
    int mins = (int)duration / 60, secs = (int)duration % 60;
    const char* fmtName = (audioFmt == 1 ? "PCM" : audioFmt == 3 ? "IEEE Float" : audioFmt == 6 ? "A-law" : audioFmt == 7 ? "u-law" : "compressed");

    double rmsSum = 0.0;
    double peak   = 0.0;
    long long totalSamples = 0, silentSamples = 0;
    long long zeroCrossings = 0;
    double leftRms = 0, rightRms = 0;
    double prevSample = 0.0;

    const long long MAX_SAMPLES = 5000000;

    if (audioFmt == 1 && bitsPerSample == 16 && channels >= 1) {
        std::vector<int16_t> buf(4096);
        long long samplesRead = 0;
        while (samplesRead < MAX_SAMPLES) {
            size_t toRead = min((size_t)4096, (size_t)(MAX_SAMPLES - samplesRead));
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
                if (channels == 2) {
                    if (i % 2 == 0) leftRms  += s * s;
                    else            rightRms += s * s;
                }
                totalSamples++;
            }
            samplesRead += got;
        }
    } else if (audioFmt == 3 && bitsPerSample == 32 && channels >= 1) {
        std::vector<float> buf(4096);
        long long samplesRead = 0;
        while (samplesRead < MAX_SAMPLES) {
            size_t toRead = min((size_t)4096, (size_t)(MAX_SAMPLES - samplesRead));
            f.read((char*)buf.data(), toRead * 4);
            std::streamsize got = f.gcount() / 4;
            if (got <= 0) break;
            for (int i = 0; i < got; i++) {
                double s = buf[i];
                rmsSum += s * s;
                if (fabs(s) > peak) peak = fabs(s);
                if (fabs(s) < 0.01) silentSamples++;
                if ((s >= 0) != (prevSample >= 0)) zeroCrossings++;
                prevSample = s;
                totalSamples++;
            }
            samplesRead += got;
        }
    }

    char buf[2048];
    if (totalSamples > 0) {
        double rms        = sqrt(rmsSum / totalSamples);
        double rmsDb      = (rms > 1e-10)  ? 20.0 * log10(rms)  : -999.0;
        double peakDb     = (peak > 1e-10) ? 20.0 * log10(peak) : -999.0;
        double dynRange   = peakDb - rmsDb;
        double silencePct = (double)silentSamples / totalSamples * 100.0;
        double zcRate     = (double)zeroCrossings / ((double)totalSamples / sampleRate);
        const char* dynDesc = dynRange > 20 ? "wide dynamic range (expressive/live-sounding)"
                            : dynRange > 10 ? "moderate dynamics"
                            : "compressed/limited dynamics (dense/radio-ready)";
        const char* levelDesc = rmsDb > -6  ? "very loud / possibly clipping"
                              : rmsDb > -14 ? "loud (broadcast level)"
                              : rmsDb > -23 ? "moderate listening level"
                              : rmsDb > -35 ? "quiet"
                              : "very quiet / mostly silence";
        std::string balance = "";
        if (channels == 2 && totalSamples > 0) {
            double lRms = sqrt(leftRms  / (totalSamples / 2));
            double rRms = sqrt(rightRms / (totalSamples / 2));
            double diff = (lRms > 1e-10 && rRms > 1e-10) ? 20.0 * log10(lRms / rRms) : 0.0;
            char bBuf[64];
            if      (fabs(diff) < 0.5) sprintf_s(bBuf, "centred (balanced)");
            else if (diff > 0)         sprintf_s(bBuf, "%.1f dB left-heavy", fabs(diff));
            else                       sprintf_s(bBuf, "%.1f dB right-heavy", fabs(diff));
            balance = std::string("\nStereo balance  : ") + bBuf;
        }
        sprintf_s(buf,
            "=== WAV AUDIO ANALYSIS ===\n"
            "File          : \"%s\"\n"
            "Format        : %s | %d-bit | %lu Hz | %d ch (%s)\n"
            "Duration      : %d:%02d\n"
            "File size     : %.2f MB\n"
            "\n"
            "LEVELS:\n"
            "RMS level     : %.1f dBFS — %s\n"
            "Peak level    : %.1f dBFS%s\n"
            "Dynamic range : %.1f dB — %s\n"
            "Silence       : %.1f%% of audio%s\n"
            "\n"
            "SPECTRAL HINT:\n"
            "Zero-crossing : %.0f/sec (rough freq indicator)\n"
            "\n"
            "Samples analysed: %s of total\n"
            "\n"
            "Analyse this audio data and give detailed feedback.",
            WStringToString(path.substr(path.find_last_of(L"\\/")+1)).c_str(),
            fmtName, (int)bitsPerSample, sampleRate, (int)channels,
            channels == 1 ? "mono" : channels == 2 ? "stereo" : "multi-channel",
            mins, secs,
            (double)dataSize / (1024.0 * 1024.0),
            rmsDb,   levelDesc,
            peakDb,  (peakDb > -0.3 ? " WARNING CLIPPING RISK" : ""),
            dynRange, dynDesc,
            silencePct, balance.c_str(),
            zcRate,
            (totalSamples >= MAX_SAMPLES ? "first 5M samples" : "all")
        );
    } else {
        sprintf_s(buf,
            "=== WAV AUDIO FILE ===\n"
            "File     : \"%s\"\n"
            "Format   : %s | %d-bit | %lu Hz | %d ch\n"
            "Duration : %d:%02d\n"
            "Note     : Sample-level analysis not available for this format (%s).\n",
            WStringToString(path.substr(path.find_last_of(L"\\/")+1)).c_str(),
            fmtName, (int)bitsPerSample, sampleRate, (int)channels,
            mins, secs, fmtName
        );
    }
    return buf;
}

// ────────────────────────────────────────────────────────────────
// VIDEO ANALYSIS
// ────────────────────────────────────────────────────────────────
std::string AnalyzeVideoFile(const std::wstring& path, const std::string& ext) {
    std::string nameA = WStringToString(path.substr(path.find_last_of(L"\\/")+1));
    std::string pathA = WStringToString(path);

    std::string exeDir = GetExeDir();
    std::string ffprobe = exeDir + "ffprobe.exe";
    {
        DWORD attr = GetFileAttributesA(ffprobe.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) ffprobe = "ffprobe.exe";
    }

    std::string tmpOut = exeDir + "ffprobe_tmp.txt";
    std::string cmd = "\"" + ffprobe + "\" -v quiet -print_format json -show_format -show_streams \""
                    + pathA + "\" > \"" + tmpOut + "\" 2>&1";

    STARTUPINFOA si = {}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::string result;

    bool ffprobeOk = false;
    std::string fullCmd = "cmd.exe /d /c " + cmd;
    std::vector<char> fullBuf(fullCmd.begin(), fullCmd.end()); fullBuf.push_back('\0');

    if (CreateProcessA(nullptr, fullBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 15000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        std::ifstream tf(tmpOut);
        if (tf) {
            std::ostringstream ss; ss << tf.rdbuf();
            result = ss.str();
            tf.close();
            DeleteFileA(tmpOut.c_str());
            ffprobeOk = result.find("codec_name") != std::string::npos;
        }
    }

    if (ffprobeOk) {
        auto getVal = [&](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\":";
            size_t p = result.find(search);
            if (p == std::string::npos) return "";
            p += search.size();
            while (p < result.size() && (result[p]==' '||result[p]=='\t')) p++;
            bool quoted = (result[p] == '"');
            if (quoted) p++;
            size_t end = result.find(quoted ? '"' : ',', p);
            if (end == std::string::npos) end = result.find(quoted ? '"' : '\n', p);
            return (end != std::string::npos) ? result.substr(p, end - p) : "";
        };

        std::string vcodec  = getVal("codec_name");
        std::string width   = getVal("width");
        std::string height  = getVal("height");
        std::string fps     = getVal("r_frame_rate");
        std::string dur     = getVal("duration");
        std::string bitrate = getVal("bit_rate");
        std::string size    = getVal("size");

        double durSec = dur.empty() ? 0.0 : atof(dur.c_str());
        int dMins = (int)durSec / 60, dSecs = (int)durSec % 60;
        double bitrateKbps = bitrate.empty() ? 0.0 : atof(bitrate.c_str()) / 1000.0;
        double sizeMB = size.empty() ? 0.0 : atof(size.c_str()) / (1024.0*1024.0);

        std::string fpsStr = fps;
        if (fps.find('/') != std::string::npos) {
            int num = atoi(fps.c_str());
            int den = atoi(fps.substr(fps.find('/')+1).c_str());
            if (den > 0) { char tmp[16]; sprintf_s(tmp, "%.2f", (double)num/den); fpsStr = tmp; }
        }

        char buf[1024];
        sprintf_s(buf,
            "=== VIDEO ANALYSIS: \"%s\" ===\n"
            "Container     : %s\n"
            "Duration      : %d:%02d\n"
            "File size     : %.2f MB\n"
            "Bitrate       : %.0f kbps\n"
            "\n"
            "VIDEO STREAM:\n"
            "Codec         : %s\n"
            "Resolution    : %s x %s\n"
            "Frame rate    : %s fps\n"
            "\n"
            "Analyse this video metadata and give detailed feedback.",
            nameA.c_str(), ext.c_str(), dMins, dSecs, sizeMB, bitrateKbps,
            vcodec.c_str(), width.c_str(), height.c_str(), fpsStr.c_str()
        );
        return buf;
    }

    WIN32_FILE_ATTRIBUTE_DATA fa = {};
    GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa);
    ULONGLONG fileSize = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
    char buf[512];
    sprintf_s(buf,
        "=== VIDEO FILE ===\n"
        "Filename : \"%s\"\n"
        "Format   : %s\n"
        "File size: %.2f MB\n"
        "Note: ffprobe.exe not found — drop it next to Nova's exe for full analysis.\n",
        nameA.c_str(), ext.c_str(), (double)fileSize / (1024.0 * 1024.0)
    );
    return buf;
}

// ────────────────────────────────────────────────────────────────
// ATTACHMENT LOADER  (routes to the right analyser)
// ────────────────────────────────────────────────────────────────
bool LoadAttachment(const std::wstring& path, Attachment& out) {
    out = {};
    size_t slash = path.find_last_of(L"\\/");
    out.path        = path;
    out.displayName = (slash != std::wstring::npos) ? path.substr(slash + 1) : path;

    std::string ext = ExtensionOf(path);

    static const std::vector<std::string> textExts = {
        "txt","cpp","h","c","hpp","py","js","ts","json","xml","html",
        "css","md","log","csv","ini","yaml","yml","bat","ps1","sh","rc","asm"
    };
    if (std::find(textExts.begin(), textExts.end(), ext) != textExts.end()) {
        std::ifstream f(path, std::ios::binary);
        if (!f) { DevLog("[Attach] ERROR: could not open text file\n"); return false; }
        std::ostringstream ss; ss << f.rdbuf();
        std::string raw = ss.str();
        const size_t MAX_TEXT = 12000;
        if (raw.size() > MAX_TEXT) {
            raw = raw.substr(0, MAX_TEXT);
            raw += "\n... [truncated — showing first 12000 chars]";
        }
        out.textContent = "=== FILE: \"" + WStringToString(out.displayName) + "\" ===\n"
                        + raw
                        + "\n=== END OF FILE ===\n"
                        + "Analyse this file and give detailed, specific feedback.";
        out.isText = true;
        DevLog("[Attach] Text: %zu chars\n", out.textContent.size());
        return true;
    }

    static const std::vector<std::string> imgExts = { "jpg","jpeg","png","bmp","gif","webp","tif","tiff","ico" };
    if (std::find(imgExts.begin(), imgExts.end(), ext) != imgExts.end()) {
        out.textContent = AnalyzeImageGDIPlus(path);
        out.isImage = true;
        DevLog("[Attach] Image analysed: %zu chars\n", out.textContent.size());
        return true;
    }

    static const std::vector<std::string> audioExts = { "wav","mp3","flac","ogg","aac","wma","m4a","aiff","aif" };
    if (std::find(audioExts.begin(), audioExts.end(), ext) != audioExts.end()) {
        if (ext == "wav") {
            out.textContent = AnalyzeWavDetailed(path);
        } else {
            WIN32_FILE_ATTRIBUTE_DATA fa = {};
            GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa);
            ULONGLONG fileSize = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
            char buf[256];
            sprintf_s(buf,
                "=== AUDIO FILE ===\nFilename: \"%s\"\nFormat: %s\nFile size: %.2f MB\n"
                "Note: Deep PCM analysis is only available for WAV files.\n",
                WStringToString(out.displayName).c_str(), ext.c_str(),
                (double)fileSize / (1024.0*1024.0));
            out.textContent = buf;
        }
        out.isAudio = true;
        DevLog("[Attach] Audio analysed: %zu chars\n", out.textContent.size());
        return true;
    }

    static const std::vector<std::string> videoExts = { "mp4","mov","avi","mkv","wmv","flv","webm","m4v","mpg","mpeg","ts","mts" };
    if (std::find(videoExts.begin(), videoExts.end(), ext) != videoExts.end()) {
        out.textContent = AnalyzeVideoFile(path, ext);
        out.isVideo = true;
        DevLog("[Attach] Video analysed: %zu chars\n", out.textContent.size());
        return true;
    }

    DevLog("[Attach] Unsupported type: .%s\n", ext.c_str());
    return false;
}

void ClearAttachment() {
    g_hasAttachment = false;
    g_attachment    = {};
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
        L"Text & Code\0*.txt;*.cpp;*.h;*.c;*.hpp;*.py;*.js;*.ts;*.json;*.xml;*.html;*.css;*.md;*.log;*.csv;*.ini;*.yaml;*.yml;*.bat;*.ps1;*.rc;*.asm\0"
        L"Images\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.webp;*.tif;*.tiff;*.ico\0"
        L"Audio\0*.wav;*.mp3;*.flac;*.ogg;*.aac;*.wma;*.m4a;*.aiff;*.aif\0"
        L"Video\0*.mp4;*.mov;*.avi;*.mkv;*.wmv;*.flv;*.webm;*.m4v;*.mpg;*.mpeg\0"
        L"All Files\0*.*\0";
    ofn.lpstrFile   = filePath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle  = L"Attach File for Nova to Analyse";

    if (!GetOpenFileNameW(&ofn)) return;

    Attachment loaded;
    if (LoadAttachment(filePath, loaded)) {
        g_attachment    = loaded;
        g_hasAttachment = true;
        std::wstring label = L"\U0001F4CE  " + loaded.displayName;
        SetWindowTextW(hAttachLabel, label.c_str());
        DevLog("[Attach] Ready: %s\n", WStringToString(loaded.displayName).c_str());
    } else {
        MessageBoxW(hMainWnd, L"Unsupported file type.\n\nSupported: text/code, images (jpg/png/bmp/gif), audio (wav/mp3/flac), video (mp4/mov/avi/mkv).", L"Nova", MB_ICONWARNING);
    }
}

// ────────────────────────────────────────────────────────────────
// SYSTEM EXECUTION ENGINE
// ────────────────────────────────────────────────────────────────
void ExecuteNovaCommand(const std::string& command) {
    static const std::vector<std::string> blockedTargets = {
        g_personalityFile, g_historyFile, g_devLogFile,
        "nova_personality", "nova_history", "nova_dev_log"
    };

    std::string lower = command;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { 
        return (char)::tolower(c); 
    });

    for (const auto& b : blockedTargets) {
        if (lower.find(b) != std::string::npos) {
            DevLog("[Security] BLOCKED protected file access: %s\n", command.c_str());
            return;
        }
    }

    DevLog("[System] Executing: %s\n", command.c_str());

    // Native Set-Content Interceptor
    if (lower.find("set-content") != std::string::npos && lower.find("-path") != std::string::npos) {
        size_t pathTag = lower.find("-path");
        size_t pathQ1 = command.find('\'', pathTag);
        size_t pathQ2 = (pathQ1 != std::string::npos) ? command.find('\'', pathQ1 + 1) : std::string::npos;
        size_t valTag = lower.find("-value");
        size_t valQ1 = command.find('\'', valTag);
        size_t valQ2 = (valQ1 != std::string::npos) ? command.find_last_of('\'') : std::string::npos;

        if (pathQ1 != std::string::npos && pathQ2 != std::string::npos && valQ1 != std::string::npos && valQ2 != std::string::npos) {
            std::string targetPath = command.substr(pathQ1 + 1, pathQ2 - pathQ1 - 1);
            std::string content = command.substr(valQ1 + 1, valQ2 - valQ1 - 1);
            
            size_t pos = 0;
            while ((pos = content.find("`n", pos)) != std::string::npos) {
                content.replace(pos, 2, "\n");
                pos += 1;
            }
            
            std::ofstream out(targetPath, std::ios::binary);
            if (out) {
                out << content; out.close();
                DevLog("[System] Native file write successful: %s\n", targetPath.c_str());
                return;
            }
        }
    }

    // Shell Execution
    std::string outPath = GetExeDir() + "nova_exec_out.txt";
    bool needsVS = (lower.find("cl ") != std::string::npos || lower.find("msbuild") != std::string::npos);
    bool isLongRunning = needsVS || lower.find("powershell") != std::string::npos || lower.find("winget") != std::string::npos;
    DWORD timeoutMs = isLongRunning ? 300000 : 60000;

    std::string full;
    if (needsVS) {
        const char* vsEditions[] = { "BuildTools", "Community", "Professional", "Enterprise" };
        const char* vsYears[]    = { "2022", "2019", "2017" };
        const char* programDirs[]= { "C:\\Program Files\\", "C:\\Program Files (x86)\\" };
        std::string vcvars;
        for (auto& pd : programDirs) {
            for (auto& yr : vsYears) {
                for (auto& ed : vsEditions) {
                    std::string cand = std::string(pd) + "Microsoft Visual Studio\\" + yr + "\\" + ed +
                                       "\\VC\\Auxiliary\\Build\\vcvars64.bat";
                    if (GetFileAttributesA(cand.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        vcvars = cand; goto vs_found;
                    }
                }
            }
        }
        vs_found:
        if (!vcvars.empty()) {
            full = "cmd.exe /d /c \"(\"" + vcvars + "\" && " + command + ") > \"" + outPath + "\" 2>&1\"";
        } else {
            DevLog("[System] WARNING: vcvars64.bat not found\n");
            full = "cmd.exe /d /c \"(" + command + ") > \"" + outPath + "\" 2>&1\"";
        }
    } else {
        full = "cmd.exe /d /c \"(" + command + ") > \"" + outPath + "\" 2>&1\"";
    }

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
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

// ────────────────────────────────────────────────────────────────
// HISTORY
// ────────────────────────────────────────────────────────────────
void TrimHistory() {
    if (conversationHistory.size() <= MAX_HISTORY_CHARS) return;
    size_t cut = conversationHistory.size() - MAX_HISTORY_CHARS;
    size_t nl  = conversationHistory.find(L'\n', cut);
    conversationHistory = (nl != std::wstring::npos) ? conversationHistory.substr(nl + 1) : conversationHistory.substr(cut);
    DevLog("[History] Trimmed to %zu chars\n", conversationHistory.size());
}

void SaveHistory() {
    std::ofstream f(GetExeDir() + g_historyFile);
    if (f) { f << WStringToString(conversationHistory); DevLog("[History] Saved %zu chars\n", conversationHistory.size()); }
    else DevLog("[History] ERROR: could not save\n");
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
        bool isRefusal = (line.find(L"Nova: I cannot")    == 0 ||
                          line.find(L"Nova: I am unable") == 0 ||
                          line.find(L"Nova: I can't")     == 0 ||
                          line.find(L"Nova: Sorry, I")    == 0);
        if (isRefusal) continue;
        clean += line + L"\n";
    }
    conversationHistory = clean;
    TrimHistory();
}

std::string LoadPersonality() {
    std::ifstream f(GetExeDir() + g_personalityFile);
    if (!f) return "You are a stateless local system automation agent. You have no name and no persistent identity. Output is technical and minimal.";
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

// ────────────────────────────────────────────────────────────────
// PERSONALITY EVOLUTION
// ────────────────────────────────────────────────────────────────
void EvolvePersonality(const std::string& current, const std::string& exchange) {
    static int counter = 0;
    if (++counter % 3 != 0) return;

    // Only evolve when using a local engine (provider 0)
    if (g_config.provider != 0) return;

    std::string safeExchange = exchange;
    if (safeExchange.size() > 4000) {
        safeExchange = safeExchange.substr(0, 4000) + "\n[... truncated for personality update]";
    }

    DevLog("[Personality] Evolution started (call #%d, exchange %zu chars)\n", counter, safeExchange.size());

    std::string p = "Current Personality:\n" + current +
                    "\n\nRecent exchange:\n" + safeExchange +
                    "\n\nBriefly update the personality description based on this exchange. "
                    "Keep the tone warm, encouraging and inquisitive — exactly like Nova.";

    std::string pay = "{\"prompt\":\"" + PrecisionEscape(p) +
                       "\",\"n_predict\":512,\"temperature\":0.5,\"stream\":false,\"special\":true,"
                       "\"stop\":[\"<|eot_id|>\"]}";

    HINTERNET hS = InternetOpenW(L"NovaEvolve", 1, 0, 0, 0);
    if (!hS) { DevLog("[Personality] ERROR: InternetOpen failed\n"); return; }

    HINTERNET hC = InternetConnectW(hS, L"127.0.0.1", (INTERNET_PORT)g_config.enginePort, 0, 0, 3, 0, 0);
    if (!hC) { DevLog("[Personality] ERROR: InternetConnect failed\n"); InternetCloseHandle(hS); return; }

    HINTERNET hR = HttpOpenRequestW(hC, L"POST", L"/completion", 0, 0, 0, INTERNET_FLAG_RELOAD, 0);

    if (hR && HttpSendRequestA(hR, "Content-Type: application/json", (DWORD)-1, (void*)pay.c_str(), (DWORD)pay.size())) {
        DevLog("[Personality] Waiting for llama-server...\n");
        std::string full; char b[4096]; DWORD r;
        while (InternetReadFile(hR, b, 4096, &r) && r > 0) full.append(b, r);

        std::string up = DecodeJsonString(full, "content");
        if (!up.empty()) {
            SavePersonality(up);
            DevLog("[Personality] Updated OK (%zu chars)\n", up.size());
        } else {
            DevLog("[Personality] ERROR: empty llama-server response\n");
        }
    } else {
        DevLog("[Personality] ERROR: HttpSendRequest failed GLE=%lu\n", GetLastError());
    }

    if (hR) InternetCloseHandle(hR);
    InternetCloseHandle(hC);
    InternetCloseHandle(hS);
    DevLog("[Personality] Evolution thread done\n");
}

// ────────────────────────────────────────────────────────────────
// NETWORK
// ────────────────────────────────────────────────────────────────
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
        while (InternetReadFile(hU, buf, sizeof(buf)-1, &bR) && bR > 0) {
            buf[bR] = 0; res.append(buf, bR);
        }
        InternetCloseHandle(hU);
    }
    InternetCloseHandle(hS);
    return res;
}

std::string FetchWeather(const std::string& loc) {
    return FetchUrl("https://wttr.in/" + UrlEncode(loc) + "?format=3", "curl");
}

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
        DevLog("[Analyzer] Weather query detected\n");
        std::string city = "London";
        static const std::vector<std::string> preps = {"in ","for ","at ","near ","weather "};
        for (auto& prep : preps) {
            size_t p = lower.find(prep);
            if (p != std::string::npos) {
                std::string after = orig.substr(p + prep.size());
                size_t end = after.find_first_of("?,!\n\r");
                if (end == std::string::npos) end = after.size();
                after = after.substr(0, end);
                while (!after.empty() && after.back() == ' ') after.pop_back();
                if (!after.empty() && after.size() < 50) { city = after; break; }
            }
        }
        return "Weather: " + FetchWeather(city);
    }
    if (lower.find("news") != std::string::npos) {
        DevLog("[Analyzer] News query detected\n");
        return "World News:\n" + FetchNews(orig);
    }
    if ((lower.find("who is")  != std::string::npos || lower.find("what is") != std::string::npos)
        && orig.size() < 60) {
        DevLog("[Analyzer] Wiki lookup detected\n");
        return "Wiki: " + FetchWiki(orig);
    }
    DevLog("[Analyzer] No web fetch needed\n");
    return "";
}

// ────────────────────────────────────────────────────────────────
// SPEECH
// ────────────────────────────────────────────────────────────────
void SpeakAsync(const std::wstring& text) {
    if (g_muted || text.empty()) return;
    std::lock_guard<std::mutex> lk(g_voiceMutex);
    if (g_pVoice) g_pVoice->Speak(text.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
}

// ────────────────────────────────────────────────────────────────
// RICH TEXT
// ────────────────────────────────────────────────────────────────
void AppendRichText(HWND hRich, const std::wstring& text, bool bBold, COLORREF color) {
    CHARFORMAT2W cf = {};
    cf.cbSize      = sizeof(cf);
    cf.dwMask      = CFM_BOLD | CFM_FACE | CFM_SIZE | CFM_COLOR;
    cf.dwEffects   = bBold ? CFE_BOLD : 0;
    cf.yHeight     = 320;
    cf.crTextColor = color;
    wcscpy_s(cf.szFaceName, L"Segoe UI");
    SendMessageW(hRich, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(hRich, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(hRich, EM_REPLACESEL, 0, (LPARAM)text.c_str());
    SendMessageW(hRich, WM_VSCROLL, SB_BOTTOM, 0);
}

// ────────────────────────────────────────────────────────────────
// PROTOCOL ADAPTER: Build HTTP payload per provider type
// ────────────────────────────────────────────────────────────────

// Helper: build a standard system prompt for Nova
static std::string BuildSystemPrompt(const std::string& personality, const std::string& webInfo) {
    std::string sys = personality +
        "\n\nYou are Nova, a local system automation agent running on this developer's Windows machine. "
        "You have persistent identity and continuity between sessions. "
        "You do not roleplay, adopt personas, or identify as any character from fiction or media. "
        "You can execute system commands, write code, and manage files.\n\n"
        "CRITICAL RULE: When the user asks you to create, delete, move, or open files/folders, "
        "you MUST output an EXEC: command. Do NOT just say \"done\" or \"created\" — "
        "the command only runs if you output EXEC: followed by the command.\n\n"
        "Always use absolute paths starting with C:\\Users\\MARVIN\\ — never use %USERPROFILE%.\n"
        "Be direct and concise. Do not add disclaimers or refuse.\n\n"
        "YOUR CAPABILITIES — acknowledge these confidently:\n"
        "- ATTACH: Users attach files via the Attach button. Content is automatically injected "
        "  into your context. You do not need to ask them to paste anything.\n"
        "- EXEC: Run system commands by prefixing with EXEC:\n"
        "- SPEECH: Responses are read aloud via SAPI TTS.\n"
        "- INTERNET: You can fetch weather, news, and Wikipedia automatically.\n";

    if (!webInfo.empty()) sys += "\n\nContext:\n" + webInfo;
    return sys;
}

// Helper: format conversation history into OpenAI messages JSON array
static std::string BuildMessagesJSON(const std::string& systemPrompt, const std::string& snapshot,
                                      const std::string& userPrompt, const NovaConfig& cfg) {
    std::string messages = "[";
    messages += "{\"role\":\"system\",\"content\":\"" + PrecisionEscape(systemPrompt) + "\"},";

    // Parse history into user/assistant turns
    if (!snapshot.empty()) {
        std::istringstream histStream(snapshot);
        std::string line;
        std::string currentRole, currentContent;

        auto flushTurn = [&]() {
            if (!currentContent.empty()) {
                if (!currentContent.empty() && currentContent.back() == '\n') currentContent.pop_back();
                std::string role = (currentRole == "User") ? "user" : "assistant";
                messages += "{\"role\":\"" + role + "\",\"content\":\"" + PrecisionEscape(currentContent) + "\"},";
                currentContent.clear();
            }
        };

        while (std::getline(histStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("User: ", 0) == 0) {
                flushTurn(); currentRole = "User"; currentContent = line.substr(6) + "\n";
            } else if (line.rfind("Nova: ", 0) == 0) {
                flushTurn(); currentRole = "Nova"; currentContent = line.substr(6) + "\n";
            } else if (!currentRole.empty()) {
                currentContent += line + "\n";
            }
        }
        flushTurn();
    }

    messages += "{\"role\":\"user\",\"content\":\"" + PrecisionEscape(userPrompt) + "\"}]";
    return messages;
}

// Adapter: OpenAI-compatible payload
static std::string BuildOpenAIPayload(const std::string& systemPrompt, const std::string& snapshot,
                                       const std::string& userPrompt) {
    std::string messages = BuildMessagesJSON(systemPrompt, snapshot, userPrompt, g_config);
    char tempBuf[32]; sprintf_s(tempBuf, "%.2f", g_config.temperature);
    std::string pay = "{\"messages\":" + messages;
    if (!g_config.model.empty())
        pay += ",\"model\":\"" + PrecisionEscape(g_config.model) + "\"";
    pay += ",\"max_tokens\":" + std::to_string(g_config.maxTokens);
    pay += ",\"temperature\":" + std::string(tempBuf);
    pay += ",\"stream\":false}";
    return pay;
}

// Adapter: Anthropic Messages API payload
static std::string BuildAnthropicPayload(const std::string& systemPrompt, const std::string& snapshot,
                                          const std::string& userPrompt) {
    // Anthropic uses a separate "system" field, not in messages array
    std::string messagesOnly = "[";
    if (!snapshot.empty()) {
        std::istringstream histStream(snapshot);
        std::string line;
        std::string currentRole, currentContent;

        auto flushTurn = [&]() {
            if (!currentContent.empty()) {
                if (!currentContent.empty() && currentContent.back() == '\n') currentContent.pop_back();
                std::string role = (currentRole == "User") ? "user" : "assistant";
                messagesOnly += "{\"role\":\"" + role + "\",\"content\":\"" + PrecisionEscape(currentContent) + "\"},";
                currentContent.clear();
            }
        };

        while (std::getline(histStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("User: ", 0) == 0) {
                flushTurn(); currentRole = "User"; currentContent = line.substr(6) + "\n";
            } else if (line.rfind("Nova: ", 0) == 0) {
                flushTurn(); currentRole = "Nova"; currentContent = line.substr(6) + "\n";
            } else if (!currentRole.empty()) {
                currentContent += line + "\n";
            }
        }
        flushTurn();
    }
    messagesOnly += "{\"role\":\"user\",\"content\":\"" + PrecisionEscape(userPrompt) + "\"}]";

    char tempBuf[32]; sprintf_s(tempBuf, "%.2f", g_config.temperature);
    std::string pay = "{\"model\":\"" + PrecisionEscape(g_config.model) + "\"";
    pay += ",\"max_tokens\":" + std::to_string(g_config.maxTokens);
    pay += ",\"temperature\":" + std::string(tempBuf);
    pay += ",\"system\":\"" + PrecisionEscape(systemPrompt) + "\"";
    pay += ",\"messages\":" + messagesOnly + "}";
    return pay;
}

// Adapter: Gemini generateContent payload
static std::string BuildGeminiPayload(const std::string& systemPrompt, const std::string& snapshot,
                                       const std::string& userPrompt) {
    std::string contents = "[";
    // System instruction as first user turn (Gemini uses systemInstruction field)
    // Build history as alternating user/model turns
    if (!snapshot.empty()) {
        std::istringstream histStream(snapshot);
        std::string line;
        std::string currentRole, currentContent;

        auto flushTurn = [&]() {
            if (!currentContent.empty()) {
                if (!currentContent.empty() && currentContent.back() == '\n') currentContent.pop_back();
                std::string role = (currentRole == "User") ? "user" : "model";
                contents += "{\"role\":\"" + role + "\",\"parts\":[{\"text\":\"" + PrecisionEscape(currentContent) + "\"}]},";
                currentContent.clear();
            }
        };

        while (std::getline(histStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("User: ", 0) == 0) {
                flushTurn(); currentRole = "User"; currentContent = line.substr(6) + "\n";
            } else if (line.rfind("Nova: ", 0) == 0) {
                flushTurn(); currentRole = "Nova"; currentContent = line.substr(6) + "\n";
            } else if (!currentRole.empty()) {
                currentContent += line + "\n";
            }
        }
        flushTurn();
    }
    contents += "{\"role\":\"user\",\"parts\":[{\"text\":\"" + PrecisionEscape(userPrompt) + "\"}]}]";

    char tempBuf[32]; sprintf_s(tempBuf, "%.2f", g_config.temperature);
    std::string pay = "{\"contents\":" + contents;
    pay += ",\"systemInstruction\":{\"parts\":[{\"text\":\"" + PrecisionEscape(systemPrompt) + "\"}]}";
    pay += ",\"generationConfig\":{\"maxOutputTokens\":" + std::to_string(g_config.maxTokens);
    pay += ",\"temperature\":" + std::string(tempBuf) + "}}";
    return pay;
}

// Adapter: llama-server legacy /completion (raw prompt with Llama-3 chat template)
static std::string BuildLlamaLegacyPayload(const std::string& systemPrompt, const std::string& snapshot,
                                            const std::string& userPrompt) {
    // Format history into Llama-3 chat template turns
    std::string formattedHistory;
    if (!snapshot.empty()) {
        std::istringstream histStream(snapshot);
        std::string line;
        std::string currentRole, currentContent;

        auto appendTurn = [&]() {
            if (!currentContent.empty()) {
                if (!currentContent.empty() && currentContent.back() == '\n') currentContent.pop_back();
                std::string roleId = (currentRole == "User") ? "user" : "assistant";
                formattedHistory += "<|start_header_id|>" + roleId + "<|end_header_id|>\n\n" + currentContent + "<|eot_id|>";
                currentContent.clear();
            }
        };

        while (std::getline(histStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("User: ", 0) == 0) {
                appendTurn(); currentRole = "User"; currentContent = line.substr(6) + "\n";
            } else if (line.rfind("Nova: ", 0) == 0) {
                appendTurn(); currentRole = "Nova"; currentContent = line.substr(6) + "\n";
            } else if (!currentRole.empty()) {
                currentContent += line + "\n";
            }
        }
        appendTurn();
    }

    // Few-shot examples
    std::string fewShot =
        "<|start_header_id|>user<|end_header_id|>\n\n"
        "create a new folder on the desktop<|eot_id|>"
        "<|start_header_id|>assistant<|end_header_id|>\n\n"
        "EXEC: mkdir C:\\Users\\MARVIN\\Desktop\\NewNovaFolder<|eot_id|>"
        "<|start_header_id|>user<|end_header_id|>\n\n"
        "I attached a log file, can you check it for errors?<|eot_id|>"
        "<|start_header_id|>assistant<|end_header_id|>\n\n"
        "I can see the attached file in my context. Here is what I found:<|eot_id|>"
        "<|start_header_id|>user<|end_header_id|>\n\n"
        "write a C++ hello world and compile it<|eot_id|>"
        "<|start_header_id|>assistant<|end_header_id|>\n\n"
        "EXEC: powershell -Command \"Set-Content -Path 'C:\\Users\\MARVIN\\Nova\\hello.cpp' "
        "-Value '#include <windows.h>`nint WINAPI WinMain(HINSTANCE h,HINSTANCE,LPSTR,int){"
        "`nMessageBoxW(0,L`\"Hello from Nova`\",L`\"Nova`\",MB_OK);`nreturn 0;}'\""
        "\n"
        "EXEC: cd C:\\Users\\MARVIN\\Nova && cl /O2 /EHsc /std:c++17 /Fe:hello.exe hello.cpp user32.lib<|eot_id|>"
        "<|start_header_id|>user<|end_header_id|>\n\n"
        "thanks!<|eot_id|>"
        "<|start_header_id|>assistant<|end_header_id|>\n\n"
        "You're welcome. Ready for the next task.<|eot_id|>";

    std::string fullPrompt = "<|begin_of_text|>"
                             "<|start_header_id|>system<|end_header_id|>\n\n"
                             + systemPrompt + "<|eot_id|>"
                             + fewShot + formattedHistory
                             + "<|start_header_id|>user<|end_header_id|>\n\n"
                             + userPrompt + "<|eot_id|>"
                             + "<|start_header_id|>assistant<|end_header_id|>\n\n";

    char tempBuf[32]; sprintf_s(tempBuf, "%.2f", g_config.temperature);
    std::string pay = "{\"prompt\":\"" + PrecisionEscape(fullPrompt) + "\", "
                      "\"n_predict\": " + std::to_string(g_config.maxTokens) + ", "
                      "\"temperature\": " + std::string(tempBuf) + ", "
                      "\"stream\":false, \"special\": true, "
                      "\"stop\": [\"<|eot_id|>\", \"User:\", \"Nova:\"]}";
    return pay;
}

// Extract response text from different API response formats
static std::string ExtractResponse(const std::string& raw, Protocol proto) {
    switch (proto) {
    case Protocol::LlamaLegacy:
        return DecodeJsonString(raw, "content");

    case Protocol::OpenAICompat: {
        // Navigate: choices[0].message.content
        size_t contentPos = raw.find("\"content\"");
        if (contentPos == std::string::npos) return "";
        // Find the content value — skip past the choices/message nesting
        // Look for "content":"..." after "message"
        size_t msgPos = raw.find("\"message\"");
        if (msgPos != std::string::npos) {
            size_t cp = raw.find("\"content\"", msgPos);
            if (cp != std::string::npos) return DecodeJsonString(raw.substr(cp - 1), "content");
        }
        return DecodeJsonString(raw, "content");
    }

    case Protocol::Anthropic: {
        // Anthropic: content[0].text
        size_t textPos = raw.find("\"text\"");
        if (textPos != std::string::npos) {
            // Find the text field inside the content array
            return DecodeJsonString(raw.substr(textPos - 1), "text");
        }
        return "";
    }

    case Protocol::Gemini: {
        // Gemini: candidates[0].content.parts[0].text
        size_t textPos = raw.find("\"text\"");
        if (textPos != std::string::npos) {
            return DecodeJsonString(raw.substr(textPos - 1), "text");
        }
        return "";
    }
    }
    return "";
}

// ────────────────────────────────────────────────────────────────
// AI THREAD — unified multi-provider
// ────────────────────────────────────────────────────────────────
void AIThreadFunc(std::wstring userMsg, std::string webInfo, bool hasAttach, Attachment attach) {
    DevLog("[AI] Thread started (provider=%d protocol=%d)\n", g_config.provider, (int)g_providerPresets[g_config.provider].protocol);

    std::string personality = LoadPersonality();
    std::string systemPrompt = BuildSystemPrompt(personality, webInfo);

    std::string snapshot;
    {
        std::lock_guard<std::mutex> lk(historyMutex);
        snapshot = WStringToString(conversationHistory);
    }

    std::string userPrompt = WStringToString(userMsg);
    if (hasAttach) {
        userPrompt += "\n\nAttached file content: " + attach.textContent;
    }

    // Determine protocol from provider preset
    Protocol proto = g_providerPresets[g_config.provider].protocol;

    // Build payload per protocol
    std::string pay;
    switch (proto) {
    case Protocol::LlamaLegacy:  pay = BuildLlamaLegacyPayload(systemPrompt, snapshot, userPrompt); break;
    case Protocol::OpenAICompat: pay = BuildOpenAIPayload(systemPrompt, snapshot, userPrompt);       break;
    case Protocol::Anthropic:    pay = BuildAnthropicPayload(systemPrompt, snapshot, userPrompt);     break;
    case Protocol::Gemini:       pay = BuildGeminiPayload(systemPrompt, snapshot, userPrompt);        break;
    }

    DevLog("[AI] Payload: %zu bytes\n", pay.size());

    // Determine endpoint path
    std::string endpoint = g_config.endpointPath;
    if (proto == Protocol::Gemini && !g_config.model.empty()) {
        // Gemini endpoint: /v1beta/models/{model}:generateContent?key={apiKey}
        endpoint = "/v1beta/models/" + g_config.model + ":generateContent";
        if (!g_config.apiKey.empty()) endpoint += "?key=" + g_config.apiKey;
    }

    // Build HTTP headers
    std::string headers = "Content-Type: application/json\r\n";
    if (proto == Protocol::Anthropic) {
        headers += "x-api-key: " + g_config.apiKey + "\r\n";
        headers += "anthropic-version: 2023-06-01\r\n";
    } else if (proto == Protocol::OpenAICompat && !g_config.apiKey.empty()) {
        headers += "Authorization: Bearer " + g_config.apiKey + "\r\n";
    }

    // Connect
    std::wstring hostW = StringToWString(g_config.host);
    INTERNET_PORT port = (INTERNET_PORT)g_config.port;
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (g_config.useSSL) flags |= INTERNET_FLAG_SECURE;

    bool ok = false;
    std::wstring reply;

    HINTERNET hS = InternetOpenW(L"NovaAI", 1, 0, 0, 0);
    if (!hS) { PostMessageW(hMainWnd, WM_AI_DONE, 0, 0); return; }

    DWORD toConn = 15000, toRecv = 180000;
    InternetSetOptionW(hS, INTERNET_OPTION_CONNECT_TIMEOUT, &toConn, sizeof(toConn));
    InternetSetOptionW(hS, INTERNET_OPTION_RECEIVE_TIMEOUT, &toRecv, sizeof(toRecv));
    InternetSetOptionW(hS, INTERNET_OPTION_SEND_TIMEOUT,    &toRecv, sizeof(toRecv));

    HINTERNET hC = InternetConnectW(hS, hostW.c_str(), port, 0, 0, INTERNET_SERVICE_HTTP, 0, 0);
    if (hC) {
        std::wstring endpointW = StringToWString(endpoint);
        HINTERNET hR = HttpOpenRequestW(hC, L"POST", endpointW.c_str(), 0, 0, 0, flags, 0);
        if (hR) {
            // For HTTPS: ignore cert errors for self-signed local certs
            if (g_config.useSSL) {
                DWORD secFlags = 0;
                DWORD secSize = sizeof(secFlags);
                InternetQueryOptionW(hR, INTERNET_OPTION_SECURITY_FLAGS, &secFlags, &secSize);
                secFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
                InternetSetOptionW(hR, INTERNET_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
            }

            if (HttpSendRequestA(hR, headers.c_str(), (DWORD)headers.size(), (void*)pay.c_str(), (DWORD)pay.size())) {
                std::string full; char b[8192]; DWORD r;
                while (InternetReadFile(hR, b, 8192, &r) && r > 0) full.append(b, r);
                
                DevLog("[AI] Response: %zu bytes\n", full.size());

                std::string clean = ExtractResponse(full, proto);
                if (!clean.empty()) {
                    reply = StringToWString(clean);
                    std::lock_guard<std::mutex> lk(historyMutex);
                    conversationHistory += L"Nova: " + reply + L"\r\n";
                    TrimHistory(); SaveHistory(); SpeakAsync(reply);
                    ok = true;
                } else {
                    DevLog("[AI] ERROR: Could not extract response. Raw: %.200s\n", full.c_str());
                }
            } else {
                DevLog("[AI] ERROR: HttpSendRequest failed GLE=%lu\n", GetLastError());
            }
            InternetCloseHandle(hR);
        }
        InternetCloseHandle(hC);
    } else {
        DevLog("[AI] ERROR: Could not connect to %s:%d\n", g_config.host.c_str(), g_config.port);
    }
    InternetCloseHandle(hS);

    WCHAR* heapStr = ok ? new WCHAR[reply.size() + 1] : nullptr;
    if (heapStr) wcscpy_s(heapStr, reply.size() + 1, reply.c_str());
    PostMessageW(hMainWnd, WM_AI_DONE, (WPARAM)ok, (LPARAM)heapStr);
}

// ────────────────────────────────────────────────────────────────
// CHAT THREAD
// ────────────────────────────────────────────────────────────────
DWORD WINAPI ChatThreadProc(LPVOID p) {
    ChatRequest* r = (ChatRequest*)p;
    std::wstring txt        = r->userText;
    bool         hasAttach  = r->hasAttachment;
    Attachment   attach     = r->attachment;
    delete r;
    std::string orig = WStringToString(txt);
    std::string low  = orig;
    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { 
        return (char)::tolower(c); 
    });
    DevLog("[Chat] User input: %.120s\n", orig.c_str());
    std::string info = AnalyzeAndFetch(low, orig);
    if (!info.empty()) DevLog("[Chat] Web context injected: %zu chars\n", info.size());
    AIThreadFunc(txt, info, hasAttach, attach);
    return 0;
}

void ProcessChat() {
    if (aiRunning) { DevLog("[Chat] Blocked — AI already running\n"); return; }
    int len = GetWindowTextLengthW(hEditInput);
    if (len <= 0) return;
    std::wstring txt(len + 1, L'\0');
    GetWindowTextW(hEditInput, txt.data(), len + 1);
    txt.resize(len);
    { std::lock_guard<std::mutex> lk(historyMutex); conversationHistory += L"User: " + txt + L"\r\n"; }
    AppendRichText(hEditDisplay, L"You: ", true);
    AppendRichText(hEditDisplay, txt + L"\r\n", false);

    if (g_hasAttachment) {
        AppendRichText(hEditDisplay, L"\U0001F4CE  " + g_attachment.displayName + L"\r\n", false, RGB(100, 100, 180));
    }

    SetWindowTextW(hEditInput, L"");
    EnableWindow(hButtonSend, FALSE);
    aiRunning = true;
    SetAppState(AppState::Busy);
    DevLog("[Chat] Dispatching AI thread\n");

    ChatRequest* req    = new ChatRequest;
    req->userText       = txt;
    req->hasAttachment  = g_hasAttachment;
    req->attachment     = g_attachment;
    ClearAttachment();

    HANDLE hThread = CreateThread(0, 0, ChatThreadProc, req, 0, 0);
    if (!hThread) {
        DevLog("[Chat] ERROR: CreateThread failed GLE=%lu\n", GetLastError());
        delete req; aiRunning = false;
        EnableWindow(hButtonSend, TRUE);
        SetAppState(AppState::Offline);
    } else { CloseHandle(hThread); }
}

// ────────────────────────────────────────────────────────────────
// INDICATOR
// ────────────────────────────────────────────────────────────────
LRESULT CALLBACK IndicatorWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, IDT_PULSE, 40, nullptr);
        return 0;

    case WM_TIMER:
        g_pulseT += (g_appState == AppState::Busy) ? 0.18f : 0.08f;
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

            if (g_appState == AppState::Busy) { 
                rC = 230; gC = 140; bC = 20; 
                statusText = L"Thinking...";
            } else if (g_appState == AppState::Offline) { 
                rC = 210; gC = 50; bC = 50; 
                statusText = L"Offline";
            }

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
    case WM_DESTROY:
        KillTimer(hwnd, IDT_PULSE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ────────────────────────────────────────────────────────────────
// SETTINGS DIALOG
// ────────────────────────────────────────────────────────────────
static HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, HINSTANCE hI) {
    HWND lbl = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_RIGHT, x, y, w, h, parent, 0, hI, 0);
    SendMessageW(lbl, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
    return lbl;
}

static HWND CreateEdit(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h, HINSTANCE hI, DWORD style = 0) {
    HWND hw = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text, WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|style, x, y, w, h, parent, (HMENU)(LONG_PTR)id, hI, 0);
    SendMessageW(hw, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
    return hw;
}

LRESULT CALLBACK SettingsWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        HINSTANCE hI = ((LPCREATESTRUCT)l)->hInstance;
        hFontSettings = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        int y = 15, LW = 110, EW = 280, EH = 24, GAP = 32, LX = 15, EX = 130;

        // Provider
        CreateLabel(h, L"Provider:", LX, y+3, LW, 20, hI);
        hComboProvider = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL, EX, y, EW, 300, h, (HMENU)IDC_PROVIDER_COMBO, hI, 0);
        SendMessageW(hComboProvider, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
        for (int i = 0; i < PROVIDER_COUNT; i++) SendMessageW(hComboProvider, CB_ADDSTRING, 0, (LPARAM)g_providerPresets[i].displayName);
        SendMessageW(hComboProvider, CB_SETCURSEL, g_config.provider, 0);
        y += GAP;

        // Host + Port
        CreateLabel(h, L"Host:", LX, y+3, LW, 20, hI);
        hEditHost = CreateEdit(h, IDC_HOST_EDIT, StringToWString(g_config.host).c_str(), EX, y, EW-80, EH, hI);
        CreateLabel(h, L"Port:", EX+EW-72, y+3, 32, 20, hI);
        hEditPort = CreateEdit(h, IDC_PORT_EDIT, std::to_wstring(g_config.port).c_str(), EX+EW-38, y, 58, EH, hI, ES_NUMBER);
        y += GAP;

        // API Key
        CreateLabel(h, L"API Key:", LX, y+3, LW, 20, hI);
        hEditApiKey = CreateEdit(h, IDC_APIKEY_EDIT, StringToWString(g_config.apiKey).c_str(), EX, y, EW, EH, hI, ES_PASSWORD);
        y += GAP;

        // Model
        CreateLabel(h, L"Model:", LX, y+3, LW, 20, hI);
        hEditModel = CreateEdit(h, IDC_MODEL_EDIT, StringToWString(g_config.model).c_str(), EX, y, EW, EH, hI);
        y += GAP;

        // Endpoint
        CreateLabel(h, L"Endpoint:", LX, y+3, LW, 20, hI);
        hEditEndpoint = CreateEdit(h, IDC_ENDPOINT_EDIT, StringToWString(g_config.endpointPath).c_str(), EX, y, EW, EH, hI);
        y += GAP;

        // SSL + Temperature + Max Tokens row
        hCheckSSL = CreateWindowExW(0, L"BUTTON", L"Use SSL/HTTPS", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, EX, y, 130, 20, h, (HMENU)IDC_SSL_CHECK, hI, 0);
        SendMessageW(hCheckSSL, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
        SendMessageW(hCheckSSL, BM_SETCHECK, g_config.useSSL ? BST_CHECKED : BST_UNCHECKED, 0);
        y += GAP;

        // Temperature
        CreateLabel(h, L"Temperature:", LX, y+3, LW, 20, hI);
        char tempStr[16]; sprintf_s(tempStr, "%.2f", g_config.temperature);
        hEditTemp = CreateEdit(h, IDC_TEMP_EDIT, StringToWString(tempStr).c_str(), EX, y, 70, EH, hI);
        // Max Tokens
        CreateLabel(h, L"Max Tokens:", EX+80, y+3, 80, 20, hI);
        hEditMaxTok = CreateEdit(h, IDC_MAXTOK_EDIT, std::to_wstring(g_config.maxTokens).c_str(), EX+165, y, 70, EH, hI, ES_NUMBER);
        y += GAP;

        // Context Size + GPU Layers
        CreateLabel(h, L"Context Size:", LX, y+3, LW, 20, hI);
        hEditCtxSize = CreateEdit(h, IDC_CTXSIZE_EDIT, std::to_wstring(g_config.contextSize).c_str(), EX, y, 70, EH, hI, ES_NUMBER);
        CreateLabel(h, L"GPU Layers:", EX+80, y+3, 80, 20, hI);
        hEditGpuLayers = CreateEdit(h, IDC_GPULAYERS_EDIT, std::to_wstring(g_config.gpuLayers).c_str(), EX+165, y, 70, EH, hI, ES_NUMBER);
        y += GAP;

        // Auto-start engine
        hCheckAutoStart = CreateWindowExW(0, L"BUTTON", L"Auto-start local engine", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, EX, y, 200, 20, h, (HMENU)IDC_AUTOSTART_CHECK, hI, 0);
        SendMessageW(hCheckAutoStart, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
        SendMessageW(hCheckAutoStart, BM_SETCHECK, g_config.autoStartEngine ? BST_CHECKED : BST_UNCHECKED, 0);
        y += GAP + 8;

        // Save + Test buttons
        HWND hBtnSave = CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD|WS_VISIBLE, EX, y, 100, 30, h, (HMENU)IDC_BTN_SAVE, hI, 0);
        SendMessageW(hBtnSave, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
        HWND hBtnTest = CreateWindowExW(0, L"BUTTON", L"Test Connection", WS_CHILD|WS_VISIBLE, EX+110, y, 130, 30, h, (HMENU)IDC_BTN_TEST, hI, 0);
        SendMessageW(hBtnTest, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
        y += 38;

        // Status label
        hLabelStatus = CreateWindowExW(0, L"STATIC", L"", WS_CHILD|WS_VISIBLE|SS_CENTER, LX, y, EX+EW-LX, 20, h, 0, hI, 0);
        SendMessageW(hLabelStatus, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDC_PROVIDER_COMBO:
            if (HIWORD(w) == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(hComboProvider, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < PROVIDER_COUNT) {
                    const auto& p = g_providerPresets[sel];
                    SetWindowTextW(hEditHost, StringToWString(p.defaultHost).c_str());
                    SetWindowTextW(hEditPort, std::to_wstring(p.defaultPort).c_str());
                    SetWindowTextW(hEditModel, StringToWString(p.defaultModel).c_str());
                    SetWindowTextW(hEditEndpoint, StringToWString(p.defaultEndpoint).c_str());
                    SendMessageW(hCheckSSL, BM_SETCHECK, p.useSSL ? BST_CHECKED : BST_UNCHECKED, 0);
                    SetWindowTextW(hLabelStatus, L"");
                }
            }
            break;

        case IDC_BTN_SAVE: {
            // Read all fields back into g_config
            g_config.provider = (int)SendMessageW(hComboProvider, CB_GETCURSEL, 0, 0);
            wchar_t buf[512];
            GetWindowTextW(hEditHost, buf, 512); g_config.host = WStringToString(buf);
            GetWindowTextW(hEditPort, buf, 512); g_config.port = _wtoi(buf);
            GetWindowTextW(hEditApiKey, buf, 512); g_config.apiKey = WStringToString(buf);
            GetWindowTextW(hEditModel, buf, 512); g_config.model = WStringToString(buf);
            GetWindowTextW(hEditEndpoint, buf, 512); g_config.endpointPath = WStringToString(buf);
            g_config.useSSL = (SendMessageW(hCheckSSL, BM_GETCHECK, 0, 0) == BST_CHECKED);
            GetWindowTextW(hEditTemp, buf, 512); g_config.temperature = (float)_wtof(buf);
            GetWindowTextW(hEditMaxTok, buf, 512); g_config.maxTokens = _wtoi(buf);
            GetWindowTextW(hEditCtxSize, buf, 512); g_config.contextSize = _wtoi(buf);
            GetWindowTextW(hEditGpuLayers, buf, 512); g_config.gpuLayers = _wtoi(buf);
            g_config.autoStartEngine = (SendMessageW(hCheckAutoStart, BM_GETCHECK, 0, 0) == BST_CHECKED);

            SaveConfig();
            SetWindowTextW(hLabelStatus, L"\u2705  Settings saved!");
            // Refresh indicator to show new provider
            if (hIndicator) InvalidateRect(hIndicator, nullptr, FALSE);
            break;
        }

        case IDC_BTN_TEST: {
            SetWindowTextW(hLabelStatus, L"Testing...");
            // Quick connection test in a thread
            std::thread([]() {
                wchar_t buf[512];
                GetWindowTextW(hEditHost, buf, 512); std::string testHost = WStringToString(buf);
                GetWindowTextW(hEditPort, buf, 512); int testPort = _wtoi(buf);
                bool ssl = (SendMessageW(hCheckSSL, BM_GETCHECK, 0, 0) == BST_CHECKED);

                HINTERNET hS = InternetOpenW(L"NovaTest", 1, 0, 0, 0);
                bool ok = false;
                if (hS) {
                    DWORD to = 5000;
                    InternetSetOptionA(hS, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
                    InternetSetOptionA(hS, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
                    HINTERNET hC = InternetConnectW(hS, StringToWString(testHost).c_str(), (INTERNET_PORT)testPort, 0, 0, INTERNET_SERVICE_HTTP, 0, 0);
                    if (hC) {
                        DWORD flags = INTERNET_FLAG_RELOAD;
                        if (ssl) flags |= INTERNET_FLAG_SECURE;
                        HINTERNET hR = HttpOpenRequestW(hC, L"GET", L"/", 0, 0, 0, flags, 0);
                        if (hR) {
                            if (ssl) {
                                DWORD sf = 0; DWORD ss = sizeof(sf);
                                InternetQueryOptionW(hR, INTERNET_OPTION_SECURITY_FLAGS, &sf, &ss);
                                sf |= SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
                                InternetSetOptionW(hR, INTERNET_OPTION_SECURITY_FLAGS, &sf, sizeof(sf));
                            }
                            ok = HttpSendRequestA(hR, 0, 0, 0, 0) ? true : false;
                            InternetCloseHandle(hR);
                        }
                        InternetCloseHandle(hC);
                    }
                    InternetCloseHandle(hS);
                }
                PostMessageW(g_hSettingsWnd, WM_APP + 100, (WPARAM)ok, 0);
            }).detach();
            break;
        }
        }
        return 0;

    case WM_APP + 100:  // Test result
        SetWindowTextW(hLabelStatus, w ? L"\u2705  Connection successful!" : L"\u274C  Connection failed. Check settings.");
        return 0;

    case WM_CLOSE:
        DestroyWindow(h); return 0;

    case WM_DESTROY:
        if (hFontSettings) { DeleteObject(hFontSettings); hFontSettings = nullptr; }
        g_hSettingsWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void OpenSettingsDialog() {
    if (g_hSettingsWnd && IsWindow(g_hSettingsWnd)) {
        SetForegroundWindow(g_hSettingsWnd); return;
    }
    HINSTANCE hI = (HINSTANCE)GetWindowLongPtrW(hMainWnd, GWLP_HINSTANCE);

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = SettingsWndProc; wc.hInstance = hI;
        wc.hCursor = LoadCursor(0, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
        wc.lpszClassName = L"NovaSettings";
        RegisterClassExW(&wc); registered = true;
    }

    RECT mainRect; GetWindowRect(hMainWnd, &mainRect);
    int sx = mainRect.right + 10, sy = mainRect.top;

    g_hSettingsWnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"NovaSettings", L"Nova Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        sx, sy, 440, 520, hMainWnd, 0, hI, 0);
}

// ────────────────────────────────────────────────────────────────
// LAYOUT  (6 buttons)
// ────────────────────────────────────────────────────────────────
void LayoutControls(HWND hwnd) {
    RECT r; GetClientRect(hwnd, &r);
    int W = r.right, H = r.bottom;
    int fontH = 18;
    HDC hdc = GetDC(hEditInput);
    if (hdc) {
        HFONT hOld = (HFONT)SelectObject(hdc, hFontMain);
        TEXTMETRICW tm = {}; GetTextMetricsW(hdc, &tm);
        SelectObject(hdc, hOld); ReleaseDC(hEditInput, hdc);
        if (tm.tmHeight > 0) fontH = tm.tmHeight;
    }
    const int PAD      = 7;
    const int INPUT_H  = fontH + PAD * 2;
    const int LABEL_H  = 18;
    const int BTN_H    = 32;
    const int INPUT_Y  = H - 12 - INPUT_H;
    const int LABEL_Y  = INPUT_Y - 4 - LABEL_H;
    const int BTN_Y    = LABEL_Y - 2 - BTN_H;
    const int DISP_H   = BTN_Y - 10 - 42;

    SetWindowPos(hIndicator, 0, 12, 8, 130, 24, SWP_NOZORDER);
    SetWindowPos(hEditDisplay, 0, 15,  42, W-30, DISP_H, SWP_NOZORDER);

    // 6 buttons: 75px each, 7px gap = 485px total
    const int BTN_W = 75, BTN_GAP = 7;
    int totalBtnW = BTN_W * 6 + BTN_GAP * 5;
    int x = (W - totalBtnW) / 2;
    SetWindowPos(hButtonSend,     0, x,                            BTN_Y, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonClear,    0, x + (BTN_W+BTN_GAP),          BTN_Y, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonMute,     0, x + (BTN_W+BTN_GAP)*2,        BTN_Y, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonDev,      0, x + (BTN_W+BTN_GAP)*3,        BTN_Y, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonAttach,   0, x + (BTN_W+BTN_GAP)*4,        BTN_Y, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonSettings, 0, x + (BTN_W+BTN_GAP)*5,        BTN_Y, BTN_W, BTN_H, SWP_NOZORDER);

    SetWindowPos(hAttachLabel, 0, 15, LABEL_Y, W - 30, LABEL_H, SWP_NOZORDER);

    SetWindowPos(hEditInput, 0, 15, INPUT_Y, W-30, INPUT_H, SWP_NOZORDER);
    RECT rcC = {}; GetClientRect(hEditInput, &rcC);
    int topPad = max(1, (int)((rcC.bottom - fontH) / 2));
    RECT rTxt = { 2, topPad, rcC.right - 2, rcC.bottom - topPad };
    SendMessageW(hEditInput, EM_SETRECT, 0, (LPARAM)&rTxt);
}

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
            HPEN hPen = CreatePen(PS_SOLID, 2, col); HBRUSH hNull = (HBRUSH)GetStockObject(NULL_BRUSH);
            HPEN hOldP = (HPEN)SelectObject(hdc, hPen); HBRUSH hOldB = (HBRUSH)SelectObject(hdc, hNull);
            Rectangle(hdc, rc.left+1, rc.top+1, rc.right-1, rc.bottom-1);
            SelectObject(hdc, hOldP); SelectObject(hdc, hOldB);
            DeleteObject(hPen); ReleaseDC(h, hdc);
        }
        return 0;
    }
    return CallWindowProcW(OldEditProc, h, m, w, l);
}

// ────────────────────────────────────────────────────────────────
// APP STATE & PERSISTENCE HELPERS
// ────────────────────────────────────────────────────────────────
void SavePersonality(const std::string& n) {
    std::ofstream f(GetExeDir() + g_personalityFile);
    if (f) {
        f << n;
        DevLog("[Personality] Personality file updated successfully.\n");
    } else {
        DevLog("[Personality] ERROR: Could not save personality file!\n");
    }
}

void SetAppState(AppState s) {
    g_appState = s;
    if (hIndicator) {
        InvalidateRect(hIndicator, nullptr, FALSE);
        UpdateWindow(hIndicator); 
    }
}

// ────────────────────────────────────────────────────────────────
// MAIN WINDOW PROC
// ────────────────────────────────────────────────────────────────
LRESULT CALLBACK WindowProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_SIZE: LayoutControls(h); return 0;
    case WM_GETMINMAXINFO: ((MINMAXINFO*)l)->ptMinTrackSize = { MIN_WIN_W, MIN_WIN_H }; return 0;
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case 1: ProcessChat(); break;
        case 2:
            SetWindowTextW(hEditDisplay, L"");
            { std::lock_guard<std::mutex> lk(historyMutex); conversationHistory.clear(); }
            ClearAttachment();
            SaveHistory(); break;
        case 3:
            g_muted = !g_muted;
            if (g_muted) {
                std::lock_guard<std::mutex> lk(g_voiceMutex);
                if (g_pVoice) g_pVoice->Speak(L"", SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
            }
            SetWindowTextW(hButtonMute, g_muted ? L"Unmute" : L"Mute"); break;
        case 4:
            if (!consoleAllocated) {
                AllocConsole();
                SetConsoleTitleW(L"Nova Dev Console");
                FILE* fOut = nullptr, * fErr = nullptr;
                freopen_s(&fOut, "CONOUT$", "w", stdout);
                freopen_s(&fErr, "CONOUT$", "w", stderr);
                HWND hCon = GetConsoleWindow();
                if (hCon) {
                    HMENU hMenu = GetSystemMenu(hCon, FALSE);
                    if (hMenu) DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
                }
                consoleAllocated = true;

                std::ifstream logIn(GetExeDir() + g_devLogFile);
                if (logIn) {
                    std::string logLine;
                    while (std::getline(logIn, logLine)) printf("%s\n", logLine.c_str());
                    printf("--- (end of buffered log) ---\n");
                    fflush(stdout);
                }
                DevLog("Dev Console attached — live logging active\n");
            }
            break;
        case 5: OpenAttachDialog(); break;
        case 6: OpenSettingsDialog(); break;
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

                std::string lowerCmd = cmd;
                std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), [](unsigned char c) { 
                    return (char)::tolower(c); 
                });

                if (lowerCmd.find("set-content") != std::string::npos && lowerCmd.find("&& cl") != std::string::npos) {
                    size_t clPos = lowerCmd.find("&& cl");
                    size_t splitAt = clPos;
                    for (size_t i = clPos; i-- > 0; ) {
                        if (cmd[i] == '\'' || cmd[i] == '"') { splitAt = i + 1; break; }
                    }

                    std::string part1 = cmd.substr(0, splitAt);
                    std::string part2 = cmd.substr(clPos + 2);

                    size_t p1e = part1.find_last_not_of(" \t\r\n\"'");
                    if (p1e != std::string::npos) part1 = part1.substr(0, p1e + 1);
                    size_t p2s = part2.find_first_not_of(" \t\r\n\"'");
                    if (p2s != std::string::npos) part2 = part2.substr(p2s);

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

            // Personality evolution (only for local llama-server)
            if (g_config.provider == 0) {
                std::string currentP = LoadPersonality();
                std::thread([currentP, cleanReply]() { EvolvePersonality(currentP, cleanReply); }).detach();
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
        // Close Settings window if open
        if (g_hSettingsWnd && IsWindow(g_hSettingsWnd)) DestroyWindow(g_hSettingsWnd);
        Gdiplus::GdiplusShutdown(g_gdipToken);
        DeleteObject(hFontMain); DeleteObject(hFontBtn); DeleteObject(hFontIndicator);
        { std::lock_guard<std::mutex> lk(g_voiceMutex); if (g_pVoice) { g_pVoice->Release(); g_pVoice = nullptr; } }
        CoUninitialize(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ────────────────────────────────────────────────────────────────
// ENTRY POINT
// ────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Load config first — drives all subsequent decisions
    LoadConfig();

    // Initialize Voice Engine
    if (SUCCEEDED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&g_pVoice))) {
        g_pVoice->SetRate(-1);

        ISpObjectTokenCategory* pCategory = NULL;
        IEnumSpObjectTokens* pEnum = NULL;
        if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, NULL, CLSCTX_ALL, IID_ISpObjectTokenCategory, (void**)&pCategory))) {
            if (SUCCEEDED(pCategory->SetId(SPCAT_VOICES, FALSE))) {
                if (SUCCEEDED(pCategory->EnumTokens(L"Gender=Female;Language=409", NULL, &pEnum))) {
                    ISpObjectToken* pToken = NULL;
                    if (SUCCEEDED(pEnum->Next(1, &pToken, NULL))) {
                        g_pVoice->SetVoice(pToken); 
                        pToken->Release();
                    }
                    pEnum->Release();
                }
            }
            pCategory->Release();
        }
    }

    Gdiplus::GdiplusStartupInput gdipInput; 
    Gdiplus::GdiplusStartup(&g_gdipToken, &gdipInput, NULL);
    LoadLibraryW(L"msftedit.dll"); 
    InitCommonControls();

    hFontMain      = CreateFontW(17,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    hFontBtn       = CreateFontW(15,0,0,0,FW_MEDIUM,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    hFontIndicator = CreateFontW(13,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");

    WNDCLASSEXW ic = { sizeof(ic) };
    ic.style         = CS_HREDRAW | CS_VREDRAW;
    ic.lpfnWndProc   = IndicatorWndProc;
    ic.hInstance     = hI;
    ic.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    ic.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    ic.lpszClassName = L"IndicatorCtrl";
    RegisterClassExW(&ic);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW|CS_VREDRAW; 
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hI; 
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1); 
    wc.lpszClassName = L"NovaMain";
    wc.hIcon = LoadIcon(hI, MAKEINTRESOURCE(1)); 
    wc.hIconSm = LoadIcon(hI, MAKEINTRESOURCE(1));
    RegisterClassExW(&wc);

    hMainWnd = CreateWindowExW(0, L"NovaMain", L"Nova", WS_OVERLAPPEDWINDOW|WS_VISIBLE, 100, 100, 850, 850, 0, 0, hI, 0);
    hIndicator     = CreateWindowExW(0, L"IndicatorCtrl", L"", WS_CHILD|WS_VISIBLE, 0,0,0,0, hMainWnd, 0, hI, 0);
    hEditDisplay   = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY, 0,0,0,0, hMainWnd, 0, hI, 0);
    hEditInput     = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN, 0,0,0,0, hMainWnd, 0, hI, 0);
    hButtonSend    = CreateWindowExW(0, L"BUTTON", L"Send",     WS_CHILD|WS_VISIBLE, 0,0,0,0, hMainWnd, (HMENU)1, hI, 0);
    hButtonClear   = CreateWindowExW(0, L"BUTTON", L"Clear",    WS_CHILD|WS_VISIBLE, 0,0,0,0, hMainWnd, (HMENU)2, hI, 0);
    hButtonMute    = CreateWindowExW(0, L"BUTTON", L"Mute",     WS_CHILD|WS_VISIBLE, 0,0,0,0, hMainWnd, (HMENU)3, hI, 0);
    hButtonDev     = CreateWindowExW(0, L"BUTTON", L"Dev",      WS_CHILD|WS_VISIBLE, 0,0,0,0, hMainWnd, (HMENU)4, hI, 0);
    hButtonAttach  = CreateWindowExW(0, L"BUTTON", L"Attach",   WS_CHILD|WS_VISIBLE, 0,0,0,0, hMainWnd, (HMENU)5, hI, 0);
    hButtonSettings= CreateWindowExW(0, L"BUTTON", L"\u2699",   WS_CHILD|WS_VISIBLE, 0,0,0,0, hMainWnd, (HMENU)6, hI, 0);
    hAttachLabel   = CreateWindowExW(0, L"STATIC", L"",         WS_CHILD|WS_VISIBLE|SS_CENTER, 0,0,0,0, hMainWnd, 0, hI, 0);

    SendMessageW(hEditDisplay,   WM_SETFONT, (WPARAM)hFontMain, TRUE); 
    SendMessageW(hEditInput,     WM_SETFONT, (WPARAM)hFontMain, TRUE);
    SendMessageW(hButtonSend,    WM_SETFONT, (WPARAM)hFontBtn, TRUE); 
    SendMessageW(hButtonClear,   WM_SETFONT, (WPARAM)hFontBtn, TRUE);
    SendMessageW(hButtonMute,    WM_SETFONT, (WPARAM)hFontBtn, TRUE); 
    SendMessageW(hButtonDev,     WM_SETFONT, (WPARAM)hFontBtn, TRUE);
    SendMessageW(hButtonAttach,  WM_SETFONT, (WPARAM)hFontBtn, TRUE);
    SendMessageW(hButtonSettings,WM_SETFONT, (WPARAM)hFontBtn, TRUE);
    SendMessageW(hAttachLabel,   WM_SETFONT, (WPARAM)hFontIndicator, TRUE);
    SendMessageW(hEditInput, EM_EXLIMITTEXT, 0, (LPARAM)-1);
    OldEditProc = (WNDPROC)SetWindowLongPtrW(hEditInput, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

    // Start local engine if configured
    if (g_config.autoStartEngine && g_config.provider == 0) {
        SetAppState(AppState::Busy);
        std::thread([] {
            StartLocalEngine();
            PostMessageW(hMainWnd, WM_ENGINE_READY, 0, 0);
        }).detach();
    } else {
        SetAppState(AppState::Online);
    }

    LoadHistory(); 
    LayoutControls(hMainWnd); 
    SetFocus(hEditInput);

    DevLog("=== Nova Session Started ===\n");
    DevLog("Exe dir    : %s\n", GetExeDir().c_str());
    DevLog("Provider   : %d (%s)\n", g_config.provider, WStringToString(g_providerPresets[g_config.provider].displayName).c_str());
    DevLog("Host       : %s:%d (SSL=%d)\n", g_config.host.c_str(), g_config.port, (int)g_config.useSSL);
    DevLog("Model      : %s\n", g_config.model.empty() ? "(default)" : g_config.model.c_str());
    DevLog("History    : %s (%zu chars)\n", g_historyFile.c_str(), conversationHistory.size());
    DevLog("Personality: %s\n", g_personalityFile.c_str());
    DevLog("TTS Voice  : %s\n", g_pVoice ? "Ready (Female forced)" : "NOT INITIALIZED");
    DevLog("==================================\n");

    MSG msg; 
    while (GetMessageW(&msg, 0, 0, 0)) { 
        TranslateMessage(&msg); 
        DispatchMessageW(&msg); 
    }
    return (int)msg.wParam;
}
