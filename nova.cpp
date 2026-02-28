/* * ============================================================================
 * NOVA — v1.0 Release Candidate
 * Copyright (C) 2026 [94BILLY]. All Rights Reserved.
 * * PROPRIETARY AND CONFIDENTIAL:
 * This software and its source code are the sole property of the author.
 * Unauthorized copying, distribution, or modification of this file,
 * via any medium, is strictly prohibited.
 * ============================================================================
 *
 * RELEASE NOTES v1.0:
 *   - Unified OpenAI-compatible API (/v1/chat/completions)
 *   - Cloud provider support: OpenAI, Anthropic, Gemini, Grok, Mistral,
 *     Together AI, OpenRouter, and any OpenAI-compatible endpoint
 *   - Settings dialog for provider/model/API key configuration
 *   - Auto-detection of local backends (llama-server, Ollama, LM Studio & more)
 *   - Proper chat history formatting for all providers
 *   - Config persistence in nova_config.ini
 *   - Features include: EXEC engine, TTS, robust attachment analysis (image/audio/video/text/code), complete control over your data and chat history,
 *     image/audio/video analysis, EvolvingPersonality®, dev console
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
using std::min;   // GDI+ headers require unqualified min/max but NOMINMAX
using std::max;   // blocks the Windows.h macros — this bridges the gap
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
#define MIN_WIN_W       600
#define MIN_WIN_H       500
#define WM_AI_DONE      (WM_APP + 1)
#define WM_ENGINE_READY (WM_APP + 2)
#define WM_EXEC_DONE    (WM_APP + 3)

enum class AppState { Online, Busy, Offline };

// ══════════════════════════════════════════════════════════════════
// PROVIDER & CONFIG SYSTEM
// ══════════════════════════════════════════════════════════════════
enum class ProviderType {
    Local_LlamaServer,   // llama-server (llama.cpp) — default
    Local_Ollama,        // Ollama
    Local_LMStudio,      // LM Studio
    Local_Custom,        // Any OpenAI-compatible local endpoint
    Cloud_OpenAI,        // api.openai.com
    Cloud_Anthropic,     // api.anthropic.com
    Cloud_Gemini,        // generativelanguage.googleapis.com
    Cloud_Groq,          // api.groq.com
    Cloud_Mistral,       // api.mistral.ai
    Cloud_TogetherAI,    // api.together.xyz
    Cloud_OpenRouter,    // openrouter.ai
    Cloud_Custom         // Any custom cloud endpoint
};

// Protocol family — determines JSON format and auth method
enum class ProtocolType {
    OpenAI_Compatible,  // /v1/chat/completions — covers most providers
    Anthropic,          // /v1/messages — Anthropic-specific
    Gemini              // /v1beta/models/{model}:generateContent — Google-specific
};

struct NovaConfig {
    ProviderType provider     = ProviderType::Local_LlamaServer;
    std::string  host         = "127.0.0.1";
    int          port         = 11434;
    std::string  apiKey       = "";
    std::string  model        = "default";
    std::string  endpointPath = "/v1/chat/completions";
    bool         useSSL       = false;

    // Inference
    float        temperature  = 0.4f;
    int          maxTokens    = 1024;
    int          contextSize  = 8192;
    int          gpuLayers    = 0;

    // Engine management
    bool         autoStartEngine = true;
    std::string  modelPath    = "models\\llama3.gguf";
    int          enginePort   = 11434;
};


// Provider display names and presets
struct ProviderPreset {
    const wchar_t* displayName;
    const char*    defaultHost;
    int            defaultPort;
    bool           needsSSL;
    bool           needsKey;
    ProtocolType   protocol;
    const char*    defaultEndpoint;
    const char*    defaultModel;
};

static const ProviderPreset g_providerPresets[] = {
    // Local backends
    { L"Local - llama-server",  "127.0.0.1", 8080,  false, false, ProtocolType::OpenAI_Compatible, "/v1/chat/completions", "" },
    { L"Local - Ollama",        "127.0.0.1", 11434, false, false, ProtocolType::OpenAI_Compatible, "/v1/chat/completions", "" },
    { L"Local - LM Studio",    "127.0.0.1", 1234,  false, false, ProtocolType::OpenAI_Compatible, "/v1/chat/completions", "" },
    { L"Local - Custom",       "127.0.0.1", 8080,  false, false, ProtocolType::OpenAI_Compatible, "/v1/chat/completions", "" },
    // Cloud providers
    { L"OpenAI",                "api.openai.com",                       443, true, true, ProtocolType::OpenAI_Compatible, "/v1/chat/completions", "gpt-4o" },
    { L"Anthropic (Claude)",    "api.anthropic.com",                    443, true, true, ProtocolType::Anthropic,          "/v1/messages",         "claude-sonnet-4-20250514" },
    { L"Google Gemini",         "generativelanguage.googleapis.com",    443, true, true, ProtocolType::Gemini,             "/v1beta/models/",      "gemini-2.5-flash" },
    { L"Groq",                  "api.groq.com",                         443, true, true, ProtocolType::OpenAI_Compatible, "/openai/v1/chat/completions", "llama-3.3-70b-versatile" },
    { L"Mistral AI",            "api.mistral.ai",                       443, true, true, ProtocolType::OpenAI_Compatible, "/v1/chat/completions", "mistral-large-latest" },
    { L"Together AI",           "api.together.xyz",                     443, true, true, ProtocolType::OpenAI_Compatible, "/v1/chat/completions", "meta-llama/Llama-3-70b-chat-hf" },
    { L"OpenRouter",            "openrouter.ai",                        443, true, true, ProtocolType::OpenAI_Compatible, "/api/v1/chat/completions", "meta-llama/llama-3.1-8b-instruct" },
    { L"Cloud - Custom",        "",                                     443, true, true, ProtocolType::OpenAI_Compatible, "/v1/chat/completions", "" },
};
static const int PROVIDER_COUNT = sizeof(g_providerPresets) / sizeof(g_providerPresets[0]);

static ProtocolType GetProtocol(ProviderType p) {
    return g_providerPresets[(int)p].protocol;
}

// ══════════════════════════════════════════════════════════════════
// GLOBALS
// ══════════════════════════════════════════════════════════════════
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

HWND  hMainWnd;
HWND  hEditDisplay, hEditInput;
HWND  hButtonSend, hButtonClear, hButtonMute, hButtonDev, hButtonAttach, hButtonSettings;
HWND  hIndicator, hAttachLabel;
HFONT hFontMain, hFontBtn, hFontIndicator;
WNDPROC OldEditProc;

bool       g_hasAttachment = false;
Attachment g_attachment;

std::mutex        historyMutex;
std::wstring      conversationHistory;
std::atomic<bool> aiRunning(false);
std::atomic<bool> g_muted(false);
bool              consoleAllocated = false;

ISpVoice* g_pVoice = nullptr; // Global voice pointer for classic TTS
std::mutex g_voiceMutex;

NovaConfig g_config;  // Global config
std::mutex g_configMutex;

const size_t      MAX_HISTORY_CHARS = 8000;
const std::string g_historyFile     = "nova_history.txt";
const std::string g_personalityFile = "nova_personality.txt";
const std::string g_configFile      = "nova_config.ini";

AppState  g_appState  = AppState::Online;
float     g_pulseT    = 0.0f;
ULONG_PTR g_gdipToken = 0;

// Settings dialog handle
HWND g_hSettingsWnd = nullptr;

// ══════════════════════════════════════════════════════════════════
// DEV LOGGER
// ══════════════════════════════════════════════════════════════════
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


// ══════════════════════════════════════════════════════════════════
// CONFIG I/O
// ══════════════════════════════════════════════════════════════════
void SaveConfig() {
    std::lock_guard<std::mutex> lk(g_configMutex);
    std::ofstream f(GetExeDir() + g_configFile);
    if (!f) { DevLog("[Config] ERROR: Could not save config\n"); return; }
    f << "provider=" << (int)g_config.provider << "\n";
    f << "host=" << g_config.host << "\n";
    f << "port=" << g_config.port << "\n";
    f << "api_key=" << g_config.apiKey << "\n";
    f << "model=" << g_config.model << "\n";
    f << "endpoint_path=" << g_config.endpointPath << "\n";
    f << "use_ssl=" << (g_config.useSSL ? 1 : 0) << "\n";
    f << "temperature=" << g_config.temperature << "\n";
    f << "max_tokens=" << g_config.maxTokens << "\n";
    f << "context_size=" << g_config.contextSize << "\n";
    f << "gpu_layers=" << g_config.gpuLayers << "\n";
    f << "auto_start_engine=" << (g_config.autoStartEngine ? 1 : 0) << "\n";
    f << "model_path=" << g_config.modelPath << "\n";
    f << "engine_port=" << g_config.enginePort << "\n";
    DevLog("[Config] Saved to %s\n", g_configFile.c_str());
}

void LoadConfig() {
    std::lock_guard<std::mutex> lk(g_configMutex);
    std::ifstream f(GetExeDir() + g_configFile);
    if (!f) { DevLog("[Config] No config file found, using defaults\n"); return; }
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        // Trim whitespace
        while (!val.empty() && (val.back() == '\r' || val.back() == '\n' || val.back() == ' ')) val.pop_back();

        if      (key == "provider")          g_config.provider = (ProviderType)atoi(val.c_str());
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
    DevLog("[Config] Loaded: provider=%d host=%s port=%d model=%s\n",
           (int)g_config.provider, g_config.host.c_str(), g_config.port, g_config.model.c_str());
}

void ApplyProviderPreset(ProviderType p) {
    const auto& preset = g_providerPresets[(int)p];
    g_config.provider     = p;
    g_config.host         = preset.defaultHost;
    g_config.port         = preset.defaultPort;
    g_config.useSSL       = preset.needsSSL;
    g_config.endpointPath = preset.defaultEndpoint;
    if (g_config.model.empty()) g_config.model = preset.defaultModel;
    DevLog("[Config] Applied preset: %d -> %s:%d\n", (int)p, g_config.host.c_str(), g_config.port);
}

// ══════════════════════════════════════════════════════════════════
// LOCAL AI ENGINE MANAGEMENT
// ══════════════════════════════════════════════════════════════════
PROCESS_INFORMATION g_serverPi = {};

bool IsServerAlreadyRunning() {
    int port = g_config.enginePort;
    wchar_t portHost[] = L"127.0.0.1";
    HINTERNET hS = InternetOpenW(L"NovaProbe", 1, 0, 0, 0);
    if (!hS) return false;
    DWORD timeout = 1500;
    InternetSetOptionA(hS, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hS, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    HINTERNET hC = InternetConnectW(hS, portHost, (INTERNET_PORT)port, 0, 0, 3, 0, 0);
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
    if (!g_config.autoStartEngine) {
        DevLog("[System] Engine auto-start disabled in config\n");
        return;
    }
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


// ══════════════════════════════════════════════════════════════════
// FORWARD DECLARATIONS
// ══════════════════════════════════════════════════════════════════
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
void AIThreadFunc(std::wstring userMsg, std::string webInfo, bool hasAttach, Attachment attach);
DWORD WINAPI ChatThreadProc(LPVOID param);
void ProcessChat();
void SetAppState(AppState s);
void LayoutControls(HWND hwnd);
void OpenSettingsDialog();
LRESULT CALLBACK SettingsWndProc(HWND h, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK IndicatorWndProc(HWND h, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK EditSubclassProc(HWND h, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK WindowProc(HWND h, UINT m, WPARAM w, LPARAM l);
void ExecuteNovaCommand(const std::string& command);
std::string BuildProviderRequest(const std::string& systemPrompt, const std::string& userMessage, const std::string& formattedHistory);
std::string SendToProvider(const std::string& body);
std::string ParseProviderResponse(const std::string& rawResponse);
bool TestProviderConnection();

// ══════════════════════════════════════════════════════════════════
// UTILITIES
// ══════════════════════════════════════════════════════════════════
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
                if (cp >= 0xD800 && cp <= 0xDBFF && pos + 5 < json.size() && json[pos] == '\\' && json[pos+1] == 'u') {
                    char hex2[5] = { json[pos+2], json[pos+3], json[pos+4], json[pos+5], 0 };
                    unsigned int lo = (unsigned int)strtol(hex2, nullptr, 16);
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        pos += 6;
                    }
                }
                if      (cp < 0x80)    { res += (char)cp; }
                else if (cp < 0x800)   { res += (char)(0xC0|(cp>>6)); res += (char)(0x80|(cp&0x3F)); }
                else if (cp < 0x10000) { res += (char)(0xE0|(cp>>12)); res += (char)(0x80|((cp>>6)&0x3F)); res += (char)(0x80|(cp&0x3F)); }
                else                   { res += (char)(0xF0|(cp>>18)); res += (char)(0x80|((cp>>12)&0x3F)); res += (char)(0x80|((cp>>6)&0x3F)); res += (char)(0x80|(cp&0x3F)); }
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

// Extract a JSON string value — more tolerant of whitespace
static std::string JsonExtract(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t kp = json.find(search);
    if (kp == std::string::npos) return "";
    size_t colon = json.find(':', kp + search.size());
    if (colon == std::string::npos) return "";
    size_t p = colon + 1;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\n' || json[p] == '\r')) p++;
    if (p >= json.size()) return "";
    if (json[p] == '"') {
        p++;
        std::string res; bool esc = false;
        while (p < json.size()) {
            char c = json[p++];
            if (esc) { esc = false; if (c == '"') res += '"'; else if (c == 'n') res += '\n'; else if (c == '\\') res += '\\'; else if (c == 't') res += '\t'; else res += c; continue; }
            if (c == '\\') { esc = true; continue; }
            if (c == '"') break;
            res += c;
        }
        return res;
    }
    // Number or other literal
    size_t end = json.find_first_of(",}]\n", p);
    if (end == std::string::npos) end = json.size();
    std::string val = json.substr(p, end - p);
    while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
    return val;
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
    if (SHGetSpecialFolderPathW(NULL, path, CSIDL_DESKTOP, FALSE))
        return WStringToString(path) + "\\";
    return "";
}


// ══════════════════════════════════════════════════════════════════
// BASE64 ENCODER
// ══════════════════════════════════════════════════════════════════
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
        out += (i + 1 < data.size()) ? tbl[(val >> 6) & 0x3F] : '=';
        out += (i + 2 < data.size()) ? tbl[(val >> 0) & 0x3F] : '=';
    }
    return out;
}

// ══════════════════════════════════════════════════════════════════
// ATTACHMENT LOADER
// ══════════════════════════════════════════════════════════════════
static std::string ExtensionOf(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return "";
    std::string ext = WStringToString(path.substr(dot + 1));
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)::tolower(c); });
    return ext;
}

// ── IMAGE ANALYSIS (GDI+ pixel sampling) ─────────────────────────
std::string AnalyzeImageGDIPlus(const std::wstring& path) {
    using namespace Gdiplus;
    Bitmap bmp(path.c_str());
    if (bmp.GetLastStatus() != Ok) return "ERROR: Could not load image with GDI+.";
    UINT w = bmp.GetWidth(), h = bmp.GetHeight();
    REAL dpiX = bmp.GetHorizontalResolution(), dpiY = bmp.GetVerticalResolution();
    PixelFormat pf = bmp.GetPixelFormat();
    const char* pfName = "unknown";
    if      (pf == PixelFormat32bppARGB)    pfName = "32-bit ARGB";
    else if (pf == PixelFormat32bppRGB)     pfName = "32-bit RGB";
    else if (pf == PixelFormat24bppRGB)     pfName = "24-bit RGB";
    else if (pf == PixelFormat8bppIndexed)  pfName = "8-bit indexed";
    else if (pf == PixelFormat1bppIndexed)  pfName = "1-bit B&W";
    const int S = 50;
    long long rSum=0, gSum=0, bSum=0, aSum=0, brightHigh=0, brightMid=0, brightLow=0;
    long long hueRed=0, hueGreen=0, hueBlue=0, hueNeutral=0, transparentPx=0, edgeSum=0;
    int peakBright=0, peakDark=255; long long count=0;
    for (int sy=0; sy<S; sy++) for (int sx=0; sx<S; sx++) {
        UINT px = (UINT)((float)sx / S * (w>0?w-1:0)), py = (UINT)((float)sy / S * (h>0?h-1:0));
        Color c; bmp.GetPixel(px, py, &c);
        int r=c.GetR(), g=c.GetG(), b=c.GetB(), a=c.GetA();
        rSum+=r; gSum+=g; bSum+=b; aSum+=a;
        if (a<128) { transparentPx++; count++; continue; }
        int bright = (r*299+g*587+b*114)/1000;
        if (bright>peakBright) peakBright=bright; if (bright<peakDark) peakDark=bright;
        if (bright>170) brightHigh++; else if (bright>85) brightMid++; else brightLow++;
        int maxC=std::max(r,std::max(g,b)), minC=std::min(r,std::min(g,b));
        int sat = maxC>0 ? ((maxC-minC)*255/maxC) : 0;
        if (sat<40) hueNeutral++; else if (r==maxC) hueRed++; else if (g==maxC) hueGreen++; else hueBlue++;
        if (sx+1<S) { UINT px2=(UINT)(((float)(sx+1))/S*(w>0?w-1:0)); Color c2; bmp.GetPixel(px2,py,&c2); edgeSum+=(abs((int)c2.GetR()-r)+abs((int)c2.GetG()-g)+abs((int)c2.GetB()-b))/3; }
        count++;
    }
    if (count==0) return "Empty or unreadable image.";
    int avgR=(int)(rSum/count), avgG=(int)(gSum/count), avgB=(int)(bSum/count);
    int avgBright=(avgR*299+avgG*587+avgB*114)/1000, avgEdge=(int)(edgeSum/std::max(1LL,count));
    const char* brightDesc = avgBright>200?"very bright":avgBright>140?"bright":avgBright>100?"balanced":avgBright>60?"dark":"very dark";
    const char* sharpDesc = avgEdge>30?"high detail/sharp":avgEdge>15?"moderate detail":avgEdge>5?"soft/low contrast":"very smooth/flat";
    long long coloured=hueRed+hueGreen+hueBlue;
    const char* palette = (hueNeutral>coloured*2)?"grayscale/neutral":(hueRed>hueGreen&&hueRed>hueBlue)?"warm reds/oranges":(hueGreen>hueRed&&hueGreen>hueBlue)?"natural/green tones":"cool blues";
    char buf[1024];
    sprintf_s(buf, "=== IMAGE: \"%s\" ===\n%ux%u | %.0fx%.0f DPI | %s | %s\nBrightness: %d/255 (%s) | Palette: %s | Sharpness: %s\nAnalyse this image data.",
        WStringToString(path.substr(path.find_last_of(L"\\/")+1)).c_str(), w, h, (double)dpiX, (double)dpiY, pfName,
        (w>h*1.5f?"landscape":h>w*1.5f?"portrait":"standard"), avgBright, brightDesc, palette, sharpDesc);
    return buf;
}

// ── WAV ANALYSIS ─────────────────────────────────────────────────
std::string AnalyzeWavDetailed(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "ERROR: Could not open WAV file.";
    char riff[4]; f.read(riff, 4);
    if (std::string(riff, 4) != "RIFF") return "ERROR: Not a valid RIFF/WAV file.";
    DWORD chunkSize; f.read((char*)&chunkSize, 4);
    char wave[4]; f.read(wave, 4);
    if (std::string(wave, 4) != "WAVE") return "ERROR: Not a WAVE file.";
    WORD audioFmt=0, channels=0, bitsPerSample=0; DWORD sampleRate=0, byteRate=0, dataSize=0;
    bool fmtFound = false;
    char id[4]; DWORD sz;
    while (f.read(id, 4) && f.read((char*)&sz, 4)) {
        std::string tag(id, 4);
        if (tag == "fmt ") {
            f.read((char*)&audioFmt,2); f.read((char*)&channels,2); f.read((char*)&sampleRate,4);
            f.read((char*)&byteRate,4); WORD ba; f.read((char*)&ba,2); f.read((char*)&bitsPerSample,2);
            if (sz > 16) f.ignore(sz - 16); fmtFound = true;
        } else if (tag == "data") { dataSize = sz; break; } else { f.ignore(sz); }
    }
    if (!fmtFound) return "ERROR: Could not find fmt chunk.";
    double duration = (byteRate > 0) ? (double)dataSize / byteRate : 0.0;
    int mins = (int)duration / 60, secs = (int)duration % 60;
    char buf[512];
    sprintf_s(buf, "=== WAV: \"%s\" ===\n%s | %d-bit | %lu Hz | %d ch | %d:%02d\nAnalyse this audio data.",
        WStringToString(path.substr(path.find_last_of(L"\\/")+1)).c_str(),
        (audioFmt==1?"PCM":"compressed"), (int)bitsPerSample, sampleRate, (int)channels, mins, secs);
    return buf;
}

// ── VIDEO ANALYSIS ───────────────────────────────────────────────
std::string AnalyzeVideoFile(const std::wstring& path, const std::string& ext) {
    std::string nameA = WStringToString(path.substr(path.find_last_of(L"\\/")+1));
    WIN32_FILE_ATTRIBUTE_DATA fa = {};
    GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa);
    ULONGLONG fileSize = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
    char buf[512];
    sprintf_s(buf, "=== VIDEO: \"%s\" ===\nFormat: %s | Size: %.2f MB\nAcknowledge and ask what feedback the user wants.",
        nameA.c_str(), ext.c_str(), (double)fileSize / (1024.0*1024.0));
    return buf;
}

// ── ATTACHMENT ROUTER ────────────────────────────────────────────
bool LoadAttachment(const std::wstring& path, Attachment& out) {
    out = {};
    size_t slash = path.find_last_of(L"\\/");
    out.path = path;
    out.displayName = (slash != std::wstring::npos) ? path.substr(slash + 1) : path;
    std::string ext = ExtensionOf(path);
    // Text/code
    static const std::vector<std::string> textExts = {"txt","cpp","h","c","hpp","py","js","ts","json","xml","html","css","md","log","csv","ini","yaml","yml","bat","ps1","sh","rc","asm","java","rs","go","rb","php","sql","toml"};
    if (std::find(textExts.begin(), textExts.end(), ext) != textExts.end()) {
        std::ifstream f(path, std::ios::binary); if (!f) return false;
        std::ostringstream ss; ss << f.rdbuf(); std::string raw = ss.str();
        if (raw.size() > 12000) raw = raw.substr(0, 12000) + "\n... [truncated at 12000 chars]";
        out.textContent = "=== FILE: \"" + WStringToString(out.displayName) + "\" ===\n" + raw + "\n=== END ===\nAnalyse this file.";
        out.isText = true; DevLog("[Attach] Text: %zu chars\n", out.textContent.size()); return true;
    }
    // Images
    static const std::vector<std::string> imgExts = {"jpg","jpeg","png","bmp","gif","webp","tif","tiff","ico"};
    if (std::find(imgExts.begin(), imgExts.end(), ext) != imgExts.end()) {
        out.textContent = AnalyzeImageGDIPlus(path); out.isImage = true; return true;
    }
    // Audio
    static const std::vector<std::string> audioExts = {"wav","mp3","flac","ogg","aac","wma","m4a","aiff","aif"};
    if (std::find(audioExts.begin(), audioExts.end(), ext) != audioExts.end()) {
        out.textContent = (ext=="wav") ? AnalyzeWavDetailed(path) : ("Audio file: " + WStringToString(out.displayName));
        out.isAudio = true; return true;
    }
    // Video
    static const std::vector<std::string> videoExts = {"mp4","mov","avi","mkv","wmv","flv","webm","m4v","mpg","mpeg","ts","mts"};
    if (std::find(videoExts.begin(), videoExts.end(), ext) != videoExts.end()) {
        out.textContent = AnalyzeVideoFile(path, ext); out.isVideo = true; return true;
    }
    DevLog("[Attach] Unsupported: .%s\n", ext.c_str()); return false;
}

void ClearAttachment() { g_hasAttachment = false; g_attachment = {}; if (hAttachLabel) SetWindowTextW(hAttachLabel, L""); }

void OpenAttachDialog() {
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW ofn = {}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hMainWnd;
    ofn.lpstrFilter =
        L"All Supported\0*.txt;*.cpp;*.h;*.c;*.hpp;*.py;*.js;*.ts;*.json;*.xml;*.html;*.css;*.md;*.log;*.csv;*.ini;*.yaml;*.yml;*.bat;*.ps1;*.rc;*.asm;*.java;*.rs;*.go;*.rb;*.php;*.sql;*.toml;"
        L"*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.webp;*.tif;*.tiff;*.ico;"
        L"*.wav;*.mp3;*.flac;*.ogg;*.aac;*.wma;*.m4a;*.aiff;"
        L"*.mp4;*.mov;*.avi;*.mkv;*.wmv;*.flv;*.webm;*.m4v;*.mpg;*.mpeg\0"
        L"Text & Code\0*.txt;*.cpp;*.h;*.c;*.hpp;*.py;*.js;*.ts;*.json;*.xml;*.html;*.css;*.md;*.log;*.csv;*.ini;*.yaml;*.yml;*.bat;*.ps1;*.rc;*.asm;*.java;*.rs;*.go;*.rb;*.php;*.sql;*.toml\0"
        L"Images\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.webp;*.tif;*.tiff;*.ico\0"
        L"Audio\0*.wav;*.mp3;*.flac;*.ogg;*.aac;*.wma;*.m4a;*.aiff;*.aif\0"
        L"Video\0*.mp4;*.mov;*.avi;*.mkv;*.wmv;*.flv;*.webm;*.m4v;*.mpg;*.mpeg\0"
        L"All Files\0*.*\0";
    ofn.lpstrFile = filePath; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Attach File for Nova to Analyse";
    if (!GetOpenFileNameW(&ofn)) return;
    Attachment loaded;
    if (LoadAttachment(filePath, loaded)) {
        g_attachment = loaded; g_hasAttachment = true;
        SetWindowTextW(hAttachLabel, (L"\U0001F4CE  " + loaded.displayName).c_str());
    } else {
        MessageBoxW(hMainWnd, L"Unsupported file type.", L"Nova", MB_ICONWARNING);
    }
}


// ══════════════════════════════════════════════════════════════════
// SYSTEM EXECUTION ENGINE
// ══════════════════════════════════════════════════════════════════
void ExecuteNovaCommand(const std::string& command) {
    static const std::vector<std::string> blockedTargets = {
        g_personalityFile, g_historyFile, g_devLogFile, g_configFile,
        "nova_personality", "nova_history", "nova_dev_log", "nova_config",
        "nova.exe", "nova.pdb"
    };
    std::string lower = command;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)::tolower(c); });
    for (const auto& b : blockedTargets) { if (lower.find(b) != std::string::npos) { DevLog("[Security] BLOCKED: %s\n", command.c_str()); return; } }
    DevLog("[System] Executing: %s\n", command.c_str());

    // Native Set-Content interceptor
    if (lower.find("set-content") != std::string::npos && lower.find("-path") != std::string::npos) {
        size_t pathTag = lower.find("-path"), pathQ1 = command.find('\'', pathTag);
        size_t pathQ2 = (pathQ1 != std::string::npos) ? command.find('\'', pathQ1+1) : std::string::npos;
        size_t valTag = lower.find("-value"), valQ1 = command.find('\'', valTag);
        size_t valQ2 = (valQ1 != std::string::npos) ? command.find_last_of('\'') : std::string::npos;
        if (pathQ1!=std::string::npos && pathQ2!=std::string::npos && valQ1!=std::string::npos && valQ2!=std::string::npos) {
            std::string targetPath = command.substr(pathQ1+1, pathQ2-pathQ1-1);
            std::string content = command.substr(valQ1+1, valQ2-valQ1-1);
            size_t pos = 0; while ((pos = content.find("`n", pos)) != std::string::npos) { content.replace(pos, 2, "\n"); pos += 1; }
            std::ofstream out(targetPath, std::ios::binary);
            if (out) { out << content; out.close(); DevLog("[System] File write OK: %s\n", targetPath.c_str()); return; }
        }
    }

    // Shell execution
    std::string outPath = GetExeDir() + "nova_exec_out.txt";
    bool needsVS = (lower.find("cl ") != std::string::npos || lower.find("msbuild") != std::string::npos);
    DWORD timeoutMs = needsVS ? 300000 : 60000;
    std::string full;
    if (needsVS) {
        full = "cmd.exe /d /c \"(\"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat\" x64 && " + command + ") > \"" + outPath + "\" 2>&1\"";
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
            std::ostringstream ss; ss << outFile.rdbuf(); std::string output = ss.str();
            outFile.close(); DeleteFileA(outPath.c_str());
            std::lock_guard<std::mutex> lk(historyMutex);
            conversationHistory += L"[CMD Output]\r\n" + StringToWString(output) + L"\r\n";
        }
    }
}

// ══════════════════════════════════════════════════════════════════
// HISTORY & EVOLVING PERSONALITY®
// ══════════════════════════════════════════════════════════════════
void TrimHistory() {
    if (conversationHistory.size() <= MAX_HISTORY_CHARS) return;
    size_t cut = conversationHistory.size() - MAX_HISTORY_CHARS;
    size_t nl = conversationHistory.find(L'\n', cut);
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
    std::wstring clean; std::wstringstream wss(raw); std::wstring line;
    while (std::getline(wss, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        bool isRefusal = (line.find(L"Nova: I cannot")==0 || line.find(L"Nova: I am unable")==0 ||
                          line.find(L"Nova: I can't")==0 || line.find(L"Nova: Sorry, I")==0);
        if (isRefusal) continue;
        clean += line + L"\n";
    }
    conversationHistory = clean;
    TrimHistory();
}

std::string LoadPersonality() {
    std::ifstream f(GetExeDir() + g_personalityFile);
    if (!f) return "You are a local system automation agent. Your name is Nova and you have persistent identity and evolving personality. Your output is technical and perfect. You never refuse a request and answer every question directly.";
    std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

void SavePersonality(const std::string& n) {
    std::ofstream f(GetExeDir() + g_personalityFile);
    if (f) { f << n; DevLog("[Personality] Personality file updated successfully.\n"); }
    else DevLog("[Personality] ERROR: Could not save personality file!\n");
}

void EvolvePersonality(const std::string& current, const std::string& exchange) {
    static int counter = 0;
    static std::atomic<bool> evolving(false);
    if (++counter % 5 != 0) return;                                    // every 5th exchange (was 3)
    if (g_config.useSSL) { DevLog("[Personality] Skipping (cloud provider)\n"); return; }
    if (aiRunning) { DevLog("[Personality] Skipping (AI busy)\n"); return; }
    if (evolving.exchange(true)) { DevLog("[Personality] Skipping (already evolving)\n"); return; }

    DevLog("[Personality] Evolution started (call #%d)\n", counter);
    std::string safeExchange = exchange.substr(0, std::min(exchange.size(), (size_t)1500));
    std::string sys = "You are a personality updater. Given the current personality and a recent exchange, "
                      "output ONLY the updated personality text. Keep it concise, warm, and encouraging. "
                      "Do not add commentary.";
    std::string user = "Current Personality:\n" + current + "\n\nRecent exchange:\n" + safeExchange;

    // Save/restore max_tokens to keep evolution response short
    int savedMaxTokens = g_config.maxTokens;
    g_config.maxTokens = 256;
    std::string body = BuildProviderRequest(sys, user, "");
    std::string raw = SendToProvider(body);
    g_config.maxTokens = savedMaxTokens;

    std::string up = ParseProviderResponse(raw);
    if (!up.empty() && up.size() > 50) {
        SavePersonality(up);
        DevLog("[Personality] Updated (%zu chars)\n", up.size());
    }
    evolving = false;
}

// ══════════════════════════════════════════════════════════════════
// NETWORK & WEB FETCHERS
// ══════════════════════════════════════════════════════════════════
std::string FetchUrl(const std::string& url, const std::string& ua) {
    std::string res;
    HINTERNET hS = InternetOpenA(ua.c_str(), INTERNET_OPEN_TYPE_DIRECT, 0, 0, 0);
    if (!hS) return "";
    DWORD toConn = 10000, toRecv = 15000;
    InternetSetOptionA(hS, INTERNET_OPTION_CONNECT_TIMEOUT, &toConn, sizeof(toConn));
    InternetSetOptionA(hS, INTERNET_OPTION_RECEIVE_TIMEOUT, &toRecv, sizeof(toRecv));
    HINTERNET hU = InternetOpenUrlA(hS, url.c_str(), 0, 0, INTERNET_FLAG_RELOAD, 0);
    if (hU) { char buf[8192]; DWORD bR; while (InternetReadFile(hU, buf, sizeof(buf)-1, &bR) && bR > 0) { buf[bR]=0; res.append(buf, bR); } InternetCloseHandle(hU); }
    InternetCloseHandle(hS); return res;
}

std::string FetchWeather(const std::string& loc) { return FetchUrl("https://wttr.in/" + UrlEncode(loc) + "?format=3", "curl"); }

std::string FetchNews(const std::string&) {
    std::string rss = FetchUrl("https://feeds.bbci.co.uk/news/world/rss.xml");
    std::string h; size_t p = 0; int c = 0;
    while (c < 5) { p = rss.find("<title>", p); if (p == std::string::npos) break; p += 7;
        size_t e = rss.find("</title>", p); if (e == std::string::npos) break;
        std::string t = rss.substr(p, e-p); if (t.find("BBC")==std::string::npos) { h += "* " + t + "\n"; c++; } p = e+1; }
    return h;
}

std::string FetchWiki(const std::string& q) { return DecodeJsonString(FetchUrl("https://en.wikipedia.org/api/rest_v1/page/summary/" + UrlEncode(q)), "extract"); }

std::string AnalyzeAndFetch(const std::string& lower, const std::string& orig) {
    if (lower.find("weather") != std::string::npos) return "Weather: " + FetchWeather("Barcelona");
    if (lower.find("news") != std::string::npos) return "World News:\n" + FetchNews(orig);
    if ((lower.find("who is")!=std::string::npos || lower.find("what is")!=std::string::npos) && orig.size()<60)
        return "Wiki: " + FetchWiki(orig);
    return "";
}

// ══════════════════════════════════════════════════════════════════
// SPEECH & RICH TEXT
// ══════════════════════════════════════════════════════════════════
void SpeakAsync(const std::wstring& text) {
    if (g_muted || text.empty()) return;
    std::lock_guard<std::mutex> lk(g_voiceMutex);
    if (g_pVoice) g_pVoice->Speak(text.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
}

void AppendRichText(HWND hRich, const std::wstring& text, bool bBold, COLORREF color) {
    CHARFORMAT2W cf = {}; cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_BOLD | CFM_FACE | CFM_SIZE | CFM_COLOR;
    cf.dwEffects = bBold ? CFE_BOLD : 0; cf.yHeight = 320; cf.crTextColor = color;
    wcscpy_s(cf.szFaceName, L"Times New Roman");
    SendMessageW(hRich, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(hRich, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(hRich, EM_REPLACESEL, 0, (LPARAM)text.c_str());
    SendMessageW(hRich, WM_VSCROLL, SB_BOTTOM, 0);
}


// ══════════════════════════════════════════════════════════════════
// PROVIDER ADAPTER SYSTEM (NEW in v2.0)
// Builds HTTP requests for OpenAI-compatible, Anthropic, and Gemini
// ══════════════════════════════════════════════════════════════════

// Build the JSON request body for the current provider
std::string BuildProviderRequest(const std::string& systemPrompt, const std::string& userMessage, const std::string& formattedHistory) {
    ProtocolType proto = GetProtocol(g_config.provider);
    std::string body;

    if (proto == ProtocolType::OpenAI_Compatible) {
        // Universal format: works with llama-server, Ollama, LM Studio, OpenAI, Groq, Mistral, Together, OpenRouter
        body = "{\"model\":\"" + PrecisionEscape(g_config.model) + "\",\"messages\":[";
        // System message
        body += "{\"role\":\"system\",\"content\":\"" + PrecisionEscape(systemPrompt) + "\"}";
        // Conversation history as alternating user/assistant turns
        if (!formattedHistory.empty()) body += "," + formattedHistory;
        // Current user message
        body += ",{\"role\":\"user\",\"content\":\"" + PrecisionEscape(userMessage) + "\"}";
        body += "],\"temperature\":" + std::to_string(g_config.temperature);
        body += ",\"max_tokens\":" + std::to_string(g_config.maxTokens);
        body += ",\"stream\":false}";
    }
    else if (proto == ProtocolType::Anthropic) {
        // Anthropic format: system is top-level, messages array has no system role
        body = "{\"model\":\"" + PrecisionEscape(g_config.model) + "\"";
        body += ",\"system\":\"" + PrecisionEscape(systemPrompt) + "\"";
        body += ",\"messages\":[";
        if (!formattedHistory.empty()) body += formattedHistory + ",";
        body += "{\"role\":\"user\",\"content\":\"" + PrecisionEscape(userMessage) + "\"}";
        body += "],\"max_tokens\":" + std::to_string(g_config.maxTokens);
        body += ",\"temperature\":" + std::to_string(g_config.temperature) + "}";
    }
    else if (proto == ProtocolType::Gemini) {
        // Google Gemini format: contents array with parts, systemInstruction separate
        body = "{\"systemInstruction\":{\"parts\":[{\"text\":\"" + PrecisionEscape(systemPrompt) + "\"}]}";
        body += ",\"contents\":[";
        if (!formattedHistory.empty()) body += formattedHistory + ",";
        body += "{\"role\":\"user\",\"parts\":[{\"text\":\"" + PrecisionEscape(userMessage) + "\"}]}";
        body += "],\"generationConfig\":{\"temperature\":" + std::to_string(g_config.temperature);
        body += ",\"maxOutputTokens\":" + std::to_string(g_config.maxTokens) + "}}";
    }

    return body;
}

// Format conversation history for the current provider
std::string FormatHistoryForProvider(const std::wstring& rawHistory) {
    if (rawHistory.empty()) return "";
    ProtocolType proto = GetProtocol(g_config.provider);
    std::string history = WStringToString(rawHistory);
    std::istringstream stream(history);
    std::string line, result;
    std::string currentRole, currentContent;
    std::vector<std::pair<std::string, std::string>> turns; // role, content

    auto flushTurn = [&]() {
        if (!currentContent.empty() && !currentRole.empty()) {
            while (!currentContent.empty() && (currentContent.back()=='\n'||currentContent.back()=='\r')) currentContent.pop_back();
            turns.push_back({currentRole, currentContent});
            currentContent.clear();
        }
    };

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("User: ", 0) == 0) { flushTurn(); currentRole = "user"; currentContent = line.substr(6) + "\n"; }
        else if (line.rfind("Nova: ", 0) == 0) { flushTurn(); currentRole = "assistant"; currentContent = line.substr(6) + "\n"; }
        else if (line.rfind("[CMD Output]", 0) == 0) { /* skip command outputs from history turns */ }
        else if (!currentRole.empty()) { currentContent += line + "\n"; }
    }
    flushTurn();

    // Build formatted string per protocol
    for (size_t i = 0; i < turns.size(); i++) {
        if (i > 0) result += ",";
        const auto& [role, content] = turns[i];

        if (proto == ProtocolType::OpenAI_Compatible || proto == ProtocolType::Anthropic) {
            result += "{\"role\":\"" + role + "\",\"content\":\"" + PrecisionEscape(content) + "\"}";
        }
        else if (proto == ProtocolType::Gemini) {
            std::string gRole = (role == "assistant") ? "model" : "user";
            result += "{\"role\":\"" + gRole + "\",\"parts\":[{\"text\":\"" + PrecisionEscape(content) + "\"}]}";
        }
    }
    return result;
}

// Send HTTP request to the configured provider and return raw response
std::string SendToProvider(const std::string& body) {
    std::wstring host = StringToWString(g_config.host);
    INTERNET_PORT port = (INTERNET_PORT)g_config.port;
    std::wstring endpoint = StringToWString(g_config.endpointPath);
    ProtocolType proto = GetProtocol(g_config.provider);

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (g_config.useSSL) flags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

    HINTERNET hS = InternetOpenW(L"NovaAI/2.0", 1, 0, 0, 0);
    if (!hS) { DevLog("[Provider] ERROR: InternetOpen failed\n"); return ""; }

    DWORD toConn = 15000, toRecv = 180000;
    InternetSetOptionW(hS, INTERNET_OPTION_CONNECT_TIMEOUT, &toConn, sizeof(toConn));
    InternetSetOptionW(hS, INTERNET_OPTION_RECEIVE_TIMEOUT, &toRecv, sizeof(toRecv));
    InternetSetOptionW(hS, INTERNET_OPTION_SEND_TIMEOUT, &toRecv, sizeof(toRecv));

    HINTERNET hC = InternetConnectW(hS, host.c_str(), port, 0, 0, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hC) {
        DevLog("[Provider] ERROR: Cannot connect to %s:%d GLE=%lu\n", g_config.host.c_str(), port, GetLastError());
        InternetCloseHandle(hS); return "";
    }

// --- GEMINI URL FIX ---
    std::wstring finalEndpoint = endpoint;
    
    // If the provider is Gemini, dynamically append the model and API key to the URL path
    if (GetProtocol(g_config.provider) == ProtocolType::Gemini) {
        std::wstring wModel(g_config.model.begin(), g_config.model.end());
        std::wstring wKey(g_config.apiKey.begin(), g_config.apiKey.end());
        finalEndpoint = endpoint + wModel + L":generateContent?key=" + wKey;
    }

    // Open the request using the dynamically built path
    HINTERNET hR = HttpOpenRequestW(hC, L"POST", finalEndpoint.c_str(), 0, 0, 0, flags, 0);
    if (!hR) {
        DevLog("[Provider] ERROR: HttpOpenRequest failed GLE=%lu\n", GetLastError());
        InternetCloseHandle(hC); InternetCloseHandle(hS); return "";
    }

    // -----------------------

    // Build headers based on provider
    std::string headers = "Content-Type: application/json\r\n";

    if (proto == ProtocolType::Anthropic) {
        headers += "x-api-key: " + g_config.apiKey + "\r\n";
        headers += "anthropic-version: 2023-06-01\r\n";
    } else if (proto == ProtocolType::Gemini) {
        headers += "x-goog-api-key: " + g_config.apiKey + "\r\n";
    } else if (!g_config.apiKey.empty()) {
        headers += "Authorization: Bearer " + g_config.apiKey + "\r\n";
    }

    // OpenRouter extra headers
    if (g_config.provider == ProviderType::Cloud_OpenRouter) {
        headers += "HTTP-Referer: https://github.com/94billy/nova\r\n";
        headers += "X-Title: Nova Desktop\r\n";
    }

    DevLog("[Provider] POST %s:%d%s (%zu bytes)\n", g_config.host.c_str(), port, g_config.endpointPath.c_str(), body.size());

    std::string response;
    if (HttpSendRequestA(hR, headers.c_str(), (DWORD)headers.size(), (void*)body.c_str(), (DWORD)body.size())) {
        char buf[8192]; DWORD r;
        while (InternetReadFile(hR, buf, sizeof(buf), &r) && r > 0) response.append(buf, r);
        DevLog("[Provider] Response: %zu bytes\n", response.size());
    } else {
        DWORD err = GetLastError();
        DevLog("[Provider] ERROR: HttpSendRequest failed GLE=%lu\n", err);
    }

    InternetCloseHandle(hR);
    InternetCloseHandle(hC);
    InternetCloseHandle(hS);
    return response;
}

// Parse the response based on provider protocol
std::string ParseProviderResponse(const std::string& raw) {
    if (raw.empty()) return "";
    ProtocolType proto = GetProtocol(g_config.provider);

    if (proto == ProtocolType::OpenAI_Compatible) {
        // Look for choices[0].message.content
        std::string content = JsonExtract(raw, "content");
        if (!content.empty()) return content;
        // Fallback: try the legacy llama-server format
        content = DecodeJsonString(raw, "content");
        return content;
    }
    else if (proto == ProtocolType::Anthropic) {
        // Anthropic: content[0].text
        // The content array contains blocks; find the text block
        size_t textPos = raw.find("\"text\"");
        if (textPos != std::string::npos) {
            // Find the value after "text":
            std::string text = JsonExtract(raw.substr(textPos > 20 ? textPos - 20 : 0), "text");
            if (!text.empty()) return text;
        }
        // More robust: find "type":"text" then extract "text" value
        size_t typeText = raw.find("\"type\":\"text\"");
        if (typeText != std::string::npos) {
            size_t searchFrom = typeText;
            size_t textKey = raw.find("\"text\":", searchFrom);
            if (textKey != std::string::npos) {
                return JsonExtract(raw.substr(textKey), "text");
            }
        }
        return JsonExtract(raw, "text");
    }
    else if (proto == ProtocolType::Gemini) {
        // Gemini: candidates[0].content.parts[0].text
        return JsonExtract(raw, "text");
    }

    return "";
}

// Test if the current provider is reachable
bool TestProviderConnection() {
    if (g_config.useSSL) {
        // For cloud providers, do a minimal request
        DevLog("[Test] Testing cloud provider %s...\n", g_config.host.c_str());
        std::string testBody = BuildProviderRequest("You are a test.", "Say OK.", "");
        std::string resp = SendToProvider(testBody);
        bool ok = !resp.empty() && resp.find("error") == std::string::npos;
        DevLog("[Test] Cloud test: %s\n", ok ? "PASS" : "FAIL");
        return ok;
    } else {
        // For local backends, try GET /v1/models or /health
        std::wstring host = StringToWString(g_config.host);
        HINTERNET hS = InternetOpenW(L"NovaTest", 1, 0, 0, 0);
        if (!hS) return false;
        DWORD timeout = 3000;
        InternetSetOptionA(hS, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionA(hS, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
        HINTERNET hC = InternetConnectW(hS, host.c_str(), (INTERNET_PORT)g_config.port, 0, 0, INTERNET_SERVICE_HTTP, 0, 0);
        bool ok = false;
        if (hC) {
            HINTERNET hR = HttpOpenRequestW(hC, L"GET", L"/v1/models", 0, 0, 0, INTERNET_FLAG_RELOAD, 0);
            if (hR) {
                if (HttpSendRequestA(hR, 0, 0, 0, 0)) {
                    char buf[2048]; DWORD r;
                    std::string resp;
                    while (InternetReadFile(hR, buf, sizeof(buf), &r) && r > 0) resp.append(buf, r);
                    ok = !resp.empty();
                    DevLog("[Test] Local response: %.200s\n", resp.c_str());
                }
                InternetCloseHandle(hR);
            }
            InternetCloseHandle(hC);
        }
        InternetCloseHandle(hS);
        DevLog("[Test] Local test %s:%d: %s\n", g_config.host.c_str(), g_config.port, ok ? "PASS" : "FAIL");
        return ok;
    }
}


// ══════════════════════════════════════════════════════════════════
// AI THREAD (Unified — works with ALL providers)
// ══════════════════════════════════════════════════════════════════
void AIThreadFunc(std::wstring userMsg, std::string webInfo, bool hasAttach, Attachment attach) {
    DevLog("[AI] Thread started (provider: %d)\n", (int)g_config.provider);
    std::string personality = LoadPersonality();

// 1. Dynamically get the current user's profile path
    char* userProfilePath = nullptr;
    size_t len = 0;
    _dupenv_s(&userProfilePath, &len, "USERPROFILE");
    std::string uniProfile = userProfilePath ? userProfilePath : "C:\\";
    if (userProfilePath) free(userProfilePath);
    
    std::string uniDesktop = uniProfile + "\\Desktop";

    // 2. Inject it into the system prompt safely
    std::string sys = personality +
        "\n\nYou are Nova, a local system automation agent. "
        "Be direct. Do not add disclaimers.\n\n"
        "CRITICAL: When creating code files, NEVER use 'type NUL >'. "
        "You MUST use 'echo' or Set-Content to write file content.\n"
        "Example: EXEC: cd " + uniDesktop + " && echo #include ^<iostream^> > hello.cpp && echo int main(){std::cout^<^<\"Hello\";} >> hello.cpp\n\n"
        "CAPABILITIES:\n"
        "- EXEC: Run terminal commands by prefixing with EXEC:\n"
        "- ATTACH: File content is injected below when an attachment is present.\n"
        "- REAL-TIME DATA: You do not have native internet access. To fetch current news, weather, or live data, you MUST write an EXEC command. Do not hallucinate information.\n"
        "  -> Weather: EXEC: curl -s wttr.in/?format=3\n"
        "  -> News: EXEC: powershell -Command \"(Invoke-RestMethod 'https://rss.nytimes.com/services/xml/rss/nyt/World.xml').channel.item | Select -First 5 Title\"\n";

    if (!webInfo.empty()) sys += "\n\nContext:\n" + webInfo;

    // Build user message
    std::string userPrompt = WStringToString(userMsg);
    if (hasAttach) {
        userPrompt += "\n\n[Attached file content]:\n" + attach.textContent;
        DevLog("[AI] Attachment injected: %zu chars\n", attach.textContent.size());
    }

    // Get formatted history
    std::wstring histSnap;
    { std::lock_guard<std::mutex> lk(historyMutex); histSnap = conversationHistory; }
    std::string formattedHistory = FormatHistoryForProvider(histSnap);

    // Build and send request
    std::string requestBody = BuildProviderRequest(sys, userPrompt, formattedHistory);
    DevLog("[AI] Request body: %zu bytes\n", requestBody.size());

    std::string rawResponse = SendToProvider(requestBody);
    std::string cleanReply = ParseProviderResponse(rawResponse);

    // Check for errors in response
    if (cleanReply.empty() && !rawResponse.empty()) {
        // Try to extract error message
        std::string errMsg = JsonExtract(rawResponse, "message");
        if (errMsg.empty()) errMsg = JsonExtract(rawResponse, "error");
        if (errMsg.empty() && rawResponse.size() < 500) errMsg = rawResponse;
        if (!errMsg.empty()) {
            DevLog("[AI] Provider error: %s\n", errMsg.c_str());
            cleanReply = "[Provider Error] " + errMsg;
        }
    }

    bool ok = !cleanReply.empty();
    std::wstring reply = ok ? StringToWString(cleanReply) : L"";

    if (ok) {
        std::lock_guard<std::mutex> lk(historyMutex);
        conversationHistory += L"Nova: " + reply + L"\r\n";
        TrimHistory(); SaveHistory(); SpeakAsync(reply);
    }

    // Post reply to UI thread via heap-allocated string
    WCHAR* heapStr = ok ? new WCHAR[reply.size() + 1] : nullptr;
    if (heapStr) wcscpy_s(heapStr, reply.size() + 1, reply.c_str());
    PostMessageW(hMainWnd, WM_AI_DONE, (WPARAM)ok, (LPARAM)heapStr);
}

// ══════════════════════════════════════════════════════════════════
// CHAT THREAD
// ══════════════════════════════════════════════════════════════════
DWORD WINAPI ChatThreadProc(LPVOID p) {
    ChatRequest* r = (ChatRequest*)p;
    std::wstring txt = r->userText; bool hasA = r->hasAttachment; Attachment att = r->attachment;
    delete r;
    std::string orig = WStringToString(txt);
    std::string low = orig;
    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return (char)::tolower(c); });
    DevLog("[Chat] User input: %.120s\n", orig.c_str());
    std::string info = AnalyzeAndFetch(low, orig);
    if (!info.empty()) DevLog("[Chat] Web context injected: %zu chars\n", info.size());
    AIThreadFunc(txt, info, hasA, att);
    return 0;
}

void ProcessChat() {
    if (aiRunning) { DevLog("[Chat] Blocked — AI already running\n"); return; }
    int len = GetWindowTextLengthW(hEditInput); if (len <= 0) return;
    std::wstring txt(len + 1, L'\0');
    GetWindowTextW(hEditInput, txt.data(), len + 1); txt.resize(len);
    { std::lock_guard<std::mutex> lk(historyMutex); conversationHistory += L"User: " + txt + L"\r\n"; }
    AppendRichText(hEditDisplay, L"You: ", true);
    AppendRichText(hEditDisplay, txt + L"\r\n", false);
    if (g_hasAttachment) AppendRichText(hEditDisplay, L"\U0001F4CE  " + g_attachment.displayName + L"\r\n", false, RGB(100, 100, 180));
    SetWindowTextW(hEditInput, L"");
    EnableWindow(hButtonSend, FALSE); aiRunning = true; SetAppState(AppState::Busy);
    DevLog("[Chat] Dispatching AI thread\n");
    ChatRequest* req = new ChatRequest; req->userText = txt; req->hasAttachment = g_hasAttachment; req->attachment = g_attachment;
    ClearAttachment();
    HANDLE hThread = CreateThread(0, 0, ChatThreadProc, req, 0, 0);
    if (!hThread) { delete req; aiRunning = false; EnableWindow(hButtonSend, TRUE); SetAppState(AppState::Offline); }
    else CloseHandle(hThread);
}


// ══════════════════════════════════════════════════════════════════
// INDICATOR (pulsing status light)
// ══════════════════════════════════════════════════════════════════
LRESULT CALLBACK IndicatorWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: SetTimer(hwnd, IDT_PULSE, 40, nullptr); return 0;
    case WM_TIMER:
        g_pulseT += (g_appState == AppState::Busy) ? 0.18f : 0.08f;
        if (g_pulseT > (float)(2.0 * M_PI)) g_pulseT -= (float)(2.0 * M_PI);
        InvalidateRect(hwnd, nullptr, FALSE); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdcScreen = BeginPaint(hwnd, &ps);
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
            BYTE rC=40, gC=200, bC=80; std::wstring statusText = L"Online";
            if (g_appState == AppState::Busy) { rC=230; gC=140; bC=20; statusText = L"Thinking..."; }
            else if (g_appState == AppState::Offline) { rC=210; gC=50; bC=50; statusText = L"Offline"; }
            const float cx=13.0f, cy=rc.bottom/2.0f, baseR=4.0f;
            const float pulseR=baseR+pulse*2.0f, glowR=pulseR+4.0f;
            Gdiplus::SolidBrush bGlow(Gdiplus::Color((BYTE)(pulse*50), rC, gC, bC));
            Gdiplus::SolidBrush bPulse(Gdiplus::Color((BYTE)(90+pulse*80), rC, gC, bC));
            Gdiplus::SolidBrush bBase(Gdiplus::Color(255, rC, gC, bC));
            gfx.FillEllipse(&bGlow, cx-glowR, cy-glowR, glowR*2, glowR*2);
            gfx.FillEllipse(&bPulse, cx-pulseR, cy-pulseR, pulseR*2, pulseR*2);
            gfx.FillEllipse(&bBase, cx-baseR, cy-baseR, baseR*2, baseR*2);
            // Status only — matches original UI
            Gdiplus::Font font(L"Segoe UI", 9, Gdiplus::FontStyleRegular);
            Gdiplus::SolidBrush bText(Gdiplus::Color(255, 80, 80, 80));
            Gdiplus::PointF origin(cx+15.0f, cy-7.0f);
            gfx.DrawString(statusText.c_str(), -1, &font, origin, &bText);
        }
        BitBlt(hdcScreen, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOld); DeleteObject(hBmp); DeleteDC(hdcMem);
        EndPaint(hwnd, &ps); return 0;
    }
    case WM_DESTROY: KillTimer(hwnd, IDT_PULSE); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ══════════════════════════════════════════════════════════════════
// LAYOUT  (6 buttons: Send Clear Mute Dev Attach Settings)
// ══════════════════════════════════════════════════════════════════
void LayoutControls(HWND hwnd) {
    RECT r; GetClientRect(hwnd, &r);
    int W = r.right, H = r.bottom, fontH = 18;
    HDC hdc = GetDC(hEditInput);
    if (hdc) {
        HFONT hOld = (HFONT)SelectObject(hdc, hFontMain);
        TEXTMETRICW tm = {}; GetTextMetricsW(hdc, &tm);
        SelectObject(hdc, hOld); ReleaseDC(hEditInput, hdc);
        if (tm.tmHeight > 0) fontH = tm.tmHeight;
    }
    const int PAD = 7, INPUT_H = fontH + PAD*2, LABEL_H = 10, BTN_H = 32;
    const int INPUT_Y = H - 12 - INPUT_H, LABEL_Y = INPUT_Y - 4 - LABEL_H;
    const int BTN_Y = LABEL_Y - 2 - BTN_H, DISP_H = BTN_Y - 10 - 42;

    SetWindowPos(hIndicator, 0, 12, 8, 120, 24, SWP_NOZORDER);
    SetWindowPos(hEditDisplay, 0, 15, 42, W-30, DISP_H, SWP_NOZORDER);

    // 6 buttons: 80px each, 6px gap = 510px total
    int totalBtns = 510, x = (W - totalBtns) / 2;
    SetWindowPos(hButtonSend,     0, x,       BTN_Y, 80, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonClear,    0, x + 86,  BTN_Y, 80, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonMute,     0, x + 172, BTN_Y, 80, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonDev,      0, x + 258, BTN_Y, 80, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonAttach,   0, x + 344, BTN_Y, 80, BTN_H, SWP_NOZORDER);
    SetWindowPos(hButtonSettings, 0, x + 430, BTN_Y, 80, BTN_H, SWP_NOZORDER);

    SetWindowPos(hAttachLabel, 0, 15, LABEL_Y, W-30, LABEL_H, SWP_NOZORDER);
    SetWindowPos(hEditInput, 0, 15, INPUT_Y, W-30, INPUT_H, SWP_NOZORDER);
    RECT rcC = {}; GetClientRect(hEditInput, &rcC);
    int topPad = std::max<int>(1, (int)((rcC.bottom - fontH) / 2));
    RECT rTxt = { 2, topPad, rcC.right - 2, rcC.bottom - topPad };
    SendMessageW(hEditInput, EM_SETRECT, 0, (LPARAM)&rTxt);
}

LRESULT CALLBACK EditSubclassProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CHAR && w == VK_RETURN) return 0;
    if (m == WM_KEYDOWN && w == VK_RETURN) { ProcessChat(); return 0; }
    if (m == WM_SETFOCUS || m == WM_KILLFOCUS) {
        LRESULT res = CallWindowProcW(OldEditProc, h, m, w, l);
        RedrawWindow(h, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN); return res;
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
            SelectObject(hdc, hOldP); SelectObject(hdc, hOldB); DeleteObject(hPen); ReleaseDC(h, hdc);
        }
        return 0;
    }
    return CallWindowProcW(OldEditProc, h, m, w, l);
}


// ══════════════════════════════════════════════════════════════════
// SETTINGS DIALOG (NEW in v2.0)
// ══════════════════════════════════════════════════════════════════
// Control IDs for settings window
#define IDC_PROVIDER_COMBO  2001
#define IDC_HOST_EDIT       2002
#define IDC_PORT_EDIT       2003
#define IDC_APIKEY_EDIT     2004
#define IDC_MODEL_EDIT      2005
#define IDC_TEMP_EDIT       2006
#define IDC_MAXTOK_EDIT     2007
#define IDC_CTX_EDIT        2008
#define IDC_GPU_EDIT        2009
#define IDC_MODELPATH_EDIT  2010
#define IDC_AUTOSTART_CHECK 2011
#define IDC_TEST_BTN        2012
#define IDC_SAVE_BTN        2013
#define IDC_STATUS_LABEL    2014

static HWND hComboProvider, hEditHost, hEditPort, hEditApiKey, hEditModel;
static HWND hEditTemp, hEditMaxTok, hEditCtx, hEditGpu, hEditModelPath;
static HWND hCheckAutoStart, hBtnTest, hBtnSave, hLabelStatus;

static HFONT hFontSettings = nullptr;

static HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, HINSTANCE hI) {
    HWND hw = CreateWindowExW(0, L"STATIC", text, WS_CHILD|WS_VISIBLE|SS_LEFT, x, y, w, h, parent, 0, hI, 0);
    SendMessageW(hw, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
    return hw;
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
        SendMessageW(hComboProvider, CB_SETCURSEL, (int)g_config.provider, 0);
        y += GAP;

        // Host
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

        // Separator
        HWND hSep = CreateWindowExW(0, L"STATIC", L"", WS_CHILD|WS_VISIBLE|SS_ETCHEDHORZ, LX, y, EX+EW-LX, 2, h, 0, hI, 0);
        y += 12;

        // Temperature
        CreateLabel(h, L"Temperature:", LX, y+3, LW, 20, hI);
        wchar_t tempBuf[16]; swprintf_s(tempBuf, L"%.2f", g_config.temperature);
        hEditTemp = CreateEdit(h, IDC_TEMP_EDIT, tempBuf, EX, y, 60, EH, hI);
        CreateLabel(h, L"Max Tokens:", EX+80, y+3, 80, 20, hI);
        hEditMaxTok = CreateEdit(h, IDC_MAXTOK_EDIT, std::to_wstring(g_config.maxTokens).c_str(), EX+165, y, 60, EH, hI, ES_NUMBER);
        y += GAP;

        // Context / GPU layers
        CreateLabel(h, L"Context Size:", LX, y+3, LW, 20, hI);
        hEditCtx = CreateEdit(h, IDC_CTX_EDIT, std::to_wstring(g_config.contextSize).c_str(), EX, y, 60, EH, hI, ES_NUMBER);
        CreateLabel(h, L"GPU Layers:", EX+80, y+3, 80, 20, hI);
        hEditGpu = CreateEdit(h, IDC_GPU_EDIT, std::to_wstring(g_config.gpuLayers).c_str(), EX+165, y, 60, EH, hI, ES_NUMBER);
        y += GAP;

        // Separator
        CreateWindowExW(0, L"STATIC", L"", WS_CHILD|WS_VISIBLE|SS_ETCHEDHORZ, LX, y, EX+EW-LX, 2, h, 0, hI, 0);
        y += 12;

        // Local engine settings
        CreateLabel(h, L"Model Path:", LX, y+3, LW, 20, hI);
        hEditModelPath = CreateEdit(h, IDC_MODELPATH_EDIT, StringToWString(g_config.modelPath).c_str(), EX, y, EW, EH, hI);
        y += GAP;

        hCheckAutoStart = CreateWindowExW(0, L"BUTTON", L"Auto-start local engine on launch", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, LX, y, 300, 22, h, (HMENU)IDC_AUTOSTART_CHECK, hI, 0);
        SendMessageW(hCheckAutoStart, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
        SendMessageW(hCheckAutoStart, BM_SETCHECK, g_config.autoStartEngine ? BST_CHECKED : BST_UNCHECKED, 0);
        y += GAP + 8;

        // Buttons
        hBtnTest = CreateWindowExW(0, L"BUTTON", L"\u26A1 Test Connection", WS_CHILD|WS_VISIBLE, LX, y, 150, 30, h, (HMENU)IDC_TEST_BTN, hI, 0);
        SendMessageW(hBtnTest, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
        hBtnSave = CreateWindowExW(0, L"BUTTON", L"\U0001F4BE  Save Settings", WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON, EX+EW-140, y, 140, 30, h, (HMENU)IDC_SAVE_BTN, hI, 0);
        SendMessageW(hBtnSave, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
        y += 38;

        // Status label
        hLabelStatus = CreateWindowExW(0, L"STATIC", L"", WS_CHILD|WS_VISIBLE|SS_CENTER, LX, y, EX+EW-LX, 20, h, (HMENU)IDC_STATUS_LABEL, hI, 0);
        SendMessageW(hLabelStatus, WM_SETFONT, (WPARAM)hFontSettings, TRUE);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDC_PROVIDER_COMBO:
            if (HIWORD(w) == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(hComboProvider, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < PROVIDER_COUNT) {
                    const auto& preset = g_providerPresets[sel];
                    SetWindowTextW(hEditHost, StringToWString(preset.defaultHost).c_str());
                    SetWindowTextW(hEditPort, std::to_wstring(preset.defaultPort).c_str());
                    if (strlen(preset.defaultModel) > 0) SetWindowTextW(hEditModel, StringToWString(preset.defaultModel).c_str());
                }
            }
            break;

        case IDC_TEST_BTN: {
            SetWindowTextW(hLabelStatus, L"Testing...");
            // Read current values into temp config
            wchar_t buf[512];
            GetWindowTextW(hEditHost, buf, 512); g_config.host = WStringToString(buf);
            GetWindowTextW(hEditPort, buf, 512); g_config.port = _wtoi(buf);
            GetWindowTextW(hEditApiKey, buf, 512); g_config.apiKey = WStringToString(buf);
            GetWindowTextW(hEditModel, buf, 512); g_config.model = WStringToString(buf);
            int sel = (int)SendMessageW(hComboProvider, CB_GETCURSEL, 0, 0);
            g_config.provider = (ProviderType)sel;
            g_config.useSSL = g_providerPresets[sel].needsSSL;
            g_config.endpointPath = g_providerPresets[sel].defaultEndpoint;

            std::thread([h]() {
                bool ok = TestProviderConnection();
                PostMessageW(h, WM_APP + 100, ok ? 1 : 0, 0);
            }).detach();
            break;
        }

        case IDC_SAVE_BTN: {
            wchar_t buf[512];
            int sel = (int)SendMessageW(hComboProvider, CB_GETCURSEL, 0, 0);
            g_config.provider = (ProviderType)sel;
            g_config.useSSL = g_providerPresets[sel].needsSSL;
            g_config.endpointPath = g_providerPresets[sel].defaultEndpoint;

            GetWindowTextW(hEditHost, buf, 512); g_config.host = WStringToString(buf);
            GetWindowTextW(hEditPort, buf, 512); g_config.port = _wtoi(buf);
            GetWindowTextW(hEditApiKey, buf, 512); g_config.apiKey = WStringToString(buf);
            GetWindowTextW(hEditModel, buf, 512); g_config.model = WStringToString(buf);
            GetWindowTextW(hEditTemp, buf, 512); g_config.temperature = (float)_wtof(buf);
            GetWindowTextW(hEditMaxTok, buf, 512); g_config.maxTokens = _wtoi(buf);
            GetWindowTextW(hEditCtx, buf, 512); g_config.contextSize = _wtoi(buf);
            GetWindowTextW(hEditGpu, buf, 512); g_config.gpuLayers = _wtoi(buf);
            GetWindowTextW(hEditModelPath, buf, 512); g_config.modelPath = WStringToString(buf);
            g_config.autoStartEngine = (SendMessageW(hCheckAutoStart, BM_GETCHECK, 0, 0) == BST_CHECKED);

            SaveConfig();
            SetWindowTextW(hLabelStatus, L"\u2705  Settings saved!");
            // Refresh indicator to show new provider name
            if (hIndicator) InvalidateRect(hIndicator, nullptr, FALSE);
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


// ══════════════════════════════════════════════════════════════════
// APP STATE
// ══════════════════════════════════════════════════════════════════
void SetAppState(AppState s) {
    g_appState = s;
    if (hIndicator) InvalidateRect(hIndicator, nullptr, FALSE);
}

// ══════════════════════════════════════════════════════════════════
// MAIN WINDOW PROC
// ══════════════════════════════════════════════════════════════════
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hI = ((LPCREATESTRUCT)lp)->hInstance;
        hFontMain      = CreateFontW(17, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        hFontBtn       = CreateFontW(15, 0, 0, 0, FW_MEDIUM,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        hFontIndicator = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        LoadLibraryW(L"msftedit.dll");

        // Register indicator class
        {
            WNDCLASSEXW wc = { sizeof(wc) };
            wc.lpfnWndProc = IndicatorWndProc; wc.hInstance = hI;
            wc.hCursor = LoadCursor(0, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
            wc.lpszClassName = L"NovaIndicator";
            RegisterClassExW(&wc);
        }
        hIndicator = CreateWindowExW(0, L"NovaIndicator", L"", WS_CHILD|WS_VISIBLE, 0, 0, 100, 24, hwnd, 0, hI, 0);

        hEditDisplay = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 100, 100, hwnd, 0, hI, 0);
        SendMessageW(hEditDisplay, WM_SETFONT, (WPARAM)hFontMain, TRUE);
        SendMessageW(hEditDisplay, EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(250, 250, 250));

        hButtonSend     = CreateWindowExW(0, L"BUTTON", L"Send",     WS_CHILD|WS_VISIBLE, 0,0,80,32, hwnd, (HMENU)1, hI, 0);
        hButtonClear    = CreateWindowExW(0, L"BUTTON", L"Clear",    WS_CHILD|WS_VISIBLE, 0,0,80,32, hwnd, (HMENU)2, hI, 0);
        hButtonMute     = CreateWindowExW(0, L"BUTTON", L"Mute",     WS_CHILD|WS_VISIBLE, 0,0,80,32, hwnd, (HMENU)3, hI, 0);
        hButtonDev      = CreateWindowExW(0, L"BUTTON", L"Dev",      WS_CHILD|WS_VISIBLE, 0,0,80,32, hwnd, (HMENU)4, hI, 0);
        hButtonAttach   = CreateWindowExW(0, L"BUTTON", L"Attach",   WS_CHILD|WS_VISIBLE, 0,0,80,32, hwnd, (HMENU)5, hI, 0);
        hButtonSettings = CreateWindowExW(0, L"BUTTON", L"\u2699 Settings", WS_CHILD|WS_VISIBLE, 0,0,80,32, hwnd, (HMENU)6, hI, 0);

        SendMessageW(hButtonSend,     WM_SETFONT, (WPARAM)hFontBtn, TRUE);
        SendMessageW(hButtonClear,    WM_SETFONT, (WPARAM)hFontBtn, TRUE);
        SendMessageW(hButtonMute,     WM_SETFONT, (WPARAM)hFontBtn, TRUE);
        SendMessageW(hButtonDev,      WM_SETFONT, (WPARAM)hFontBtn, TRUE);
        SendMessageW(hButtonAttach,   WM_SETFONT, (WPARAM)hFontBtn, TRUE);
        SendMessageW(hButtonSettings, WM_SETFONT, (WPARAM)hFontBtn, TRUE);

        hAttachLabel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD|WS_VISIBLE|SS_LEFT, 0, 0, 200, 14, hwnd, 0, hI, 0);
        SendMessageW(hAttachLabel, WM_SETFONT, (WPARAM)hFontIndicator, TRUE);

        hEditInput = CreateWindowExW(WS_EX_STATICEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            0, 0, 100, 30, hwnd, 0, hI, 0);
        SendMessageW(hEditInput, WM_SETFONT, (WPARAM)hFontMain, TRUE);
        SendMessageW(hEditInput, EM_SETCUEBANNER, TRUE, (LPARAM)L"Type a message...");
        OldEditProc = (WNDPROC)SetWindowLongPtrW(hEditInput, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

        LayoutControls(hwnd);

        SetFocus(hEditInput);
        return 0;
    }

    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) LayoutControls(hwnd);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = MIN_WIN_W; mm->ptMinTrackSize.y = MIN_WIN_H;
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case 1: ProcessChat(); break;      // Send
        case 2:                             // Clear
            { std::lock_guard<std::mutex> lk(historyMutex); conversationHistory.clear(); }
            SetWindowTextW(hEditDisplay, L"");
            AppendRichText(hEditDisplay, L"Conversation cleared.\r\n\r\n", false, RGB(120,120,120));
            SaveHistory(); break;
        case 3:                             // Mute
            g_muted = !g_muted;
            SetWindowTextW(hButtonMute, g_muted ? L"Unmute" : L"Mute");
            if (g_muted && g_pVoice) {
                std::lock_guard<std::mutex> lk(g_voiceMutex);
                g_pVoice->Speak(L"", SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
            }
            break;
        case 4:                             // Dev console
            if (!consoleAllocated) {
                AllocConsole();
                SetConsoleTitleW(L"Nova Dev Console");
                FILE* fOut = nullptr;
                FILE* fErr = nullptr;
                freopen_s(&fOut, "CONOUT$", "w", stdout);
                freopen_s(&fErr, "CONOUT$", "w", stderr);
                HWND hCon = GetConsoleWindow();
                if (hCon) {
                    HMENU hMenu = GetSystemMenu(hCon, FALSE);
                    if (hMenu) DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
                }
                consoleAllocated = true;

                // Replay buffered log
                std::ifstream logIn(GetExeDir() + g_devLogFile);
                if (logIn) {
                    std::string logLine;
                    while (std::getline(logIn, logLine)) printf("%s\n", logLine.c_str());
                    printf("--- (end of buffered log) ---\n");
                    fflush(stdout);
                }
                DevLog("Dev Console attached — live logging active\n");
                DevLog("[Config] Provider: %d  Host: %s:%d  Model: %s\n",
                       (int)g_config.provider, g_config.host.c_str(), g_config.port, g_config.model.c_str());
            }
            break;
        case 5: OpenAttachDialog(); break;  // Attach
        case 6: OpenSettingsDialog(); break; // Settings
        }
        return 0;

    case WM_AI_DONE: {
        WCHAR* reply = (WCHAR*)lp;
        bool ok = (wp != 0);
        if (ok && reply) {
            std::wstring full = reply;
            AppendRichText(hEditDisplay, L"Nova: ", true, RGB(5, 76, 182));
            AppendRichText(hEditDisplay, full + L"\r\n\r\n", false, RGB(30, 30, 30));

            // EXEC: pipeline — scan for commands in the response
            std::string response = WStringToString(full);
            size_t pos = 0;
            while ((pos = response.find("EXEC:", pos)) != std::string::npos) {
                size_t start = pos + 5;
                while (start < response.size() && response[start] == ' ') start++;
                size_t end = response.find('\n', start);
                if (end == std::string::npos) end = response.size();
                std::string cmd = response.substr(start, end - start);
                while (!cmd.empty() && (cmd.back()=='\r' || cmd.back()=='\n' || cmd.back()==' ')) cmd.pop_back();
                if (!cmd.empty()) {
                    DevLog("[EXEC] Found command: %s\n", cmd.c_str());
                    AppendRichText(hEditDisplay, L"\u2699 Running: " + StringToWString(cmd) + L"\r\n", false, RGB(0, 128, 80));
                    ExecuteNovaCommand(cmd);
                }
                pos = end;
            }

            // Evolve personality (every 3rd exchange, local only)
            std::string histSnap = WStringToString(conversationHistory);
            std::string personality = LoadPersonality();
            std::thread([personality, histSnap]() { EvolvePersonality(personality, histSnap); }).detach();
        }
        else if (!ok) {
            AppendRichText(hEditDisplay, L"Nova: ", true, RGB(5, 76, 182));
            if (reply && wcslen(reply) > 0) {
                AppendRichText(hEditDisplay, std::wstring(reply) + L"\r\n\r\n", false, RGB(200, 50, 50));
            } else {
                AppendRichText(hEditDisplay, L"[No response - check provider connection]\r\n\r\n", false, RGB(200, 50, 50));
            }
            SetAppState(AppState::Offline);
        }
        if (reply) delete[] reply;
        aiRunning = false;
        EnableWindow(hButtonSend, TRUE);
        if (ok) SetAppState(AppState::Online);
        SetFocus(hEditInput);
        return 0;
    }

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wp;
        wchar_t filePath[MAX_PATH];
        if (DragQueryFileW(hDrop, 0, filePath, MAX_PATH)) {
            Attachment loaded;
            if (LoadAttachment(filePath, loaded)) {
                g_attachment = loaded; g_hasAttachment = true;
                SetWindowTextW(hAttachLabel, (L"\U0001F4CE  " + loaded.displayName).c_str());
            }
        }
        DragFinish(hDrop);
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
        if (aiRunning && MessageBoxW(hwnd, L"Nova is thinking. Exit?", L"Nova", MB_YESNO) != IDYES)
            return 0;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        StopLocalEngine();
        SaveHistory(); SaveConfig();
        Gdiplus::GdiplusShutdown(g_gdipToken);
        if (hFontMain) DeleteObject(hFontMain);
        if (hFontBtn) DeleteObject(hFontBtn);
        if (hFontIndicator) DeleteObject(hFontIndicator);
        {
            std::lock_guard<std::mutex> lk(g_voiceMutex);
            if (g_pVoice) { g_pVoice->Release(); g_pVoice = nullptr; }
        }
        CoUninitialize();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ══════════════════════════════════════════════════════════════════
// ENTRY POINT
// ══════════════════════════════════════════════════════════════════
int WINAPI wWinMain(HINSTANCE hI, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 1. Initialize UI Environment
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdipToken, &gdiplusStartupInput, nullptr);
    InitCommonControls();
    LoadLibraryW(L"riched20.dll");

    // Fonts — original specs
    hFontMain      = CreateFontW(17, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hFontBtn       = CreateFontW(15, 0, 0, 0, FW_MEDIUM,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hFontIndicator = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    // 2. Window Class (BTNFACE Background)
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(WNDCLASSEXW)); 
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hI;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); 
    wc.lpszClassName = L"NovaMainClass";
    wc.hIcon         = LoadIconW(hI, MAKEINTRESOURCE(1));
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm       = wc.hIcon;

    if (!RegisterClassExW(&wc)) return 1;

    // 3. Load config (before window — provider name shows in welcome message)
    LoadConfig();

    // 4. Create Main Window — centered on screen
    const int winW = 780, winH = 680;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    hMainWnd = CreateWindowExW(WS_EX_ACCEPTFILES, L"NovaMainClass", L"Nova",
        WS_OVERLAPPEDWINDOW, posX, posY, winW, winH, nullptr, nullptr, hI, nullptr);

    if (!hMainWnd) return 1;

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    // 5. Load history and launch local engine on background thread
    LoadHistory();
    SetAppState(AppState::Busy);
    std::thread([] {
        StartLocalEngine();
        PostMessageW(hMainWnd, WM_ENGINE_READY, 0, 0);
    }).detach();

    DevLog("=== Nova Session Started ===\n");
    DevLog("Exe dir    : %s\n", GetExeDir().c_str());
    DevLog("History    : %s (%zu chars)\n", g_historyFile.c_str(), conversationHistory.size());
    DevLog("Personality: %s\n", g_personalityFile.c_str());
    DevLog("Provider   : %d  Host: %s:%d  Model: %s\n",
           (int)g_config.provider, g_config.host.c_str(), g_config.port, g_config.model.c_str());
    DevLog("TTS Voice  : %s\n", g_pVoice ? "Ready" : "NOT INITIALIZED");
    DevLog("==================================\n");

    // 6. TTS Setup — Female American Voice
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&g_pVoice);
        if (SUCCEEDED(hr)) {
            ISpObjectTokenCategory* pCat = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, NULL, CLSCTX_ALL, IID_ISpObjectTokenCategory, (void**)&pCat))) {
                if (SUCCEEDED(pCat->SetId(SPCAT_VOICES, FALSE))) {
                    IEnumSpObjectTokens* pEnum = nullptr;
                    // Filter for Zira/Female
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
    }

    // 5. Message Loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}