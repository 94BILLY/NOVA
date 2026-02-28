# NOVA — AI Desktop System

A single-file C++17 Win32 desktop AI assistant with multi-provider support, system command execution, file analysis, and text-to-speech.

![Windows](https://img.shields.io/badge/platform-Windows%2010%2F11-blue) ![C++17](https://img.shields.io/badge/language-C%2B%2B17-orange) ![License](https://img.shields.io/badge/license-MIT-green)

## Features

- **Multi-Provider AI** — 12 backends out of the box: local (llama-server, Ollama, LM Studio) and cloud (OpenAI, Anthropic, Google Gemini, Groq, Mistral, Together AI, OpenRouter, and custom endpoints)
- **System Command Execution** — Run PowerShell/CMD commands directly from chat via `EXEC:` prefix
- **File Analysis** — Drag-and-drop or attach images, audio, video, and 25+ text/code formats
- **Text-to-Speech** — Built-in Windows SAPI voice with mute toggle
- **Evolving Personality** — Nova's personality updates naturally over time based on your conversations
- **Settings UI** — Configure provider, model, API key, and tuning parameters from the in-app dialog
- **Persistent Config** — All settings and conversation history saved between sessions
- **Dev Console** — Built-in developer console with log replay for debugging
- **Single File** — The entire application is one `.cpp` file with zero external dependencies beyond the Windows SDK

## Requirements

- **Windows 10 or 11** (x64)
- **Visual Studio Build Tools 2019 or 2022** with the "Desktop development with C++" workload
  - Download: https://visualstudio.microsoft.com/downloads/

### For Local AI (run models on your machine)

You need one of the following inference servers running locally:

| Server | Default Port | Install |
|--------|-------------|---------|
| [llama.cpp / llama-server](https://github.com/ggerganov/llama.cpp) | 8080 | Build from source or download release |
| [Ollama](https://ollama.ai) | 11434 | One-click installer |
| [LM Studio](https://lmstudio.ai) | 1234 | Desktop app with model browser |

For best performance on laptops, use a **Q4_K_M** quantized model (7B-8B parameters) and launch your server with `--flash-attn` if your GPU supports it.

### For Cloud AI (use hosted models)

Just need an API key from any supported provider — configure it in Nova's Settings dialog.

## Installation

1. **Clone the repo:**
   ```
   git clone https://github.com/94BILLY/NOVA.git
   cd NOVA
   ```

2. **Build:**

   Double-click **`Save_Changes.bat`** — it auto-detects your Visual Studio installation, compiles the icon resource, and builds `nova.exe`.

   Or build manually from a VS Developer Command Prompt:
   ```
   rc /nologo resources.rc
   cl /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /utf-8 /W3 nova.cpp resources.res /Fe:nova.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib kernel32.lib ole32.lib comctl32.lib gdiplus.lib comdlg32.lib wininet.lib winmm.lib shell32.lib shlwapi.lib advapi32.lib /ENTRY:wWinMainCRTStartup
   ```

3. **Run:**
   ```
   nova.exe
   ```
   Or double-click **`Run_Nova.bat`**.

4. **Configure:**
   Click the **Settings** button in the app to select your AI backend, enter API keys, and adjust parameters.

## Usage

| Action | How |
|--------|-----|
| Send a message | Type in the input box, press **Enter** or click **Send** |
| Run a command | Start your message with `EXEC:` followed by any PowerShell/CMD command |
| Attach a file | Click **Attach** or drag-and-drop a file onto the window |
| Mute voice | Click **Mute** to toggle text-to-speech |
| Dev console | Click **Dev** to open the debug log window |
| Change provider | Click **Settings** to switch between local and cloud AI |

## File Structure

```
NOVA/
├── nova.cpp                      # Complete application source
├── resources.rc                  # Icon resource script
├── Nova.ico                      # Application icon
├── Save_Changes.bat              # Build script (auto-detects VS, kills running Nova)
├── Run_Nova.bat                  # Launcher
├── nova_personality_default.txt  # Default personality template
├── .gitignore
├── LICENSE
└── README.md
```

**Generated at runtime** (not tracked in git):

| File | Purpose |
|------|---------|
| `nova_config.ini` | Your provider, model, and tuning settings |
| `nova_history.txt` | Conversation history |
| `nova_personality.txt` | Nova's evolving personality |
| `nova_dev_log.txt` | Debug log |

## Supported Providers

| Provider | Type | Protocol |
|----------|------|----------|
| llama-server | Local | OpenAI-compatible |
| Ollama | Local | OpenAI-compatible |
| LM Studio | Local | OpenAI-compatible |
| Custom Local | Local | OpenAI-compatible |
| OpenAI | Cloud | OpenAI |
| Anthropic (Claude) | Cloud | Anthropic |
| Google Gemini | Cloud | Gemini |
| Groq | Cloud | OpenAI-compatible |
| Mistral AI | Cloud | OpenAI-compatible |
| Together AI | Cloud | OpenAI-compatible |
| OpenRouter | Cloud | OpenAI-compatible |
| Custom Cloud | Cloud | OpenAI-compatible |

## Performance Tips

- **Use a Q4_K_M quantized model** — best speed/quality tradeoff for laptops
- **Enable flash attention** — add `--flash-attn` to your llama-server launch flags
- **Smaller models are faster** — 7B/8B models respond in seconds on modern GPUs
- **GPU offloading** — set GPU Layers to 99 in Settings to offload everything to your GPU
- **Context size** — lower values (2048-4096) are faster; increase only if you need long conversations

## License

MIT License — see [LICENSE](LICENSE) for details.
