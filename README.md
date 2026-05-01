# NOVA v1.5 🌎
### Native Windows AI Operator
*"The performance & durability of the past with the intelligence of the future."*

Nova is a native Win32 C++17 desktop assistant that can chat with local or cloud AI providers and execute approved Windows tasks through PowerShell/CMD. It is built for users who want a fast, local-first AI operator without an Electron wrapper.

What Nova does today:
* Connects to 17 local and cloud AI backends.
* Automates Windows tasks through the `EXEC:` command pipeline.
* Reads image, audio, video metadata, and source/text attachments.
* Keeps local history and personality data on your machine.
* Runs as a small native Windows executable.

Try these first: see [`recipes/demo-recipes.md`](recipes/demo-recipes.md) for five safe demo prompts that show folder creation, file summarization, C++ app generation, weather/news lookup, and provider switching.

#### 🚀 Key Improvements in v1.5

* **Bare-Metal Core:** 100% Native C++ execution with zero bloat, native DPI Awareness for crisp 4K displays, and a sleek centered 600px 7-button dashboard layout.
* **Unified 17-Provider Backend:** Seamless switching between local (llama-server, Ollama, LM Studio, vLLM, KoboldCpp, Jan, GPT4All) and cloud endpoints (OpenAI, Anthropic, Groq, Mistral, Together AI, OpenRouter, xAI, Google Gemini, and custom).
* **Hardware Kill-Switch:** Dedicated **Stop** button with thread-safe inference termination to safely abort execution loops.
* **Automated Orchestration:** Custom EXEC engine with single-line command chaining and strict anti-hallucination safeguards for PowerShell/CMD task management.
* **Universal Pathing:** Native support for `%USERPROFILE%` environment variables for reliable cross-machine execution.
* **Multimodal Attachment Analysis:** Native GDI+ image analysis, WAV audio parsing, video metadata via ffprobe, and full source file ingestion.
* **EvolvingPersonality®:** Persistent identity and memory growth between sessions, stored entirely on-device.

#### 🛠 Installation Details

This release follows a strictly sequenced process. Choose your path:

**Option A — Pre-built Binary (Recommended)**  
1. Download and run **`Install_Nova.bat`** — it handles everything (engine + model + shortcut).

**Option B — Build from Source**  
1. **`Step 1 - Setup_Nova.bat`** — Downloads the local engine and model weights (~4.66 GB).  
2. **`Step 2 - Compile_Nova.bat`** — Locates MSVC (auto-detects VS2019/VS2022) and compiles `nova.cpp` into `Nova.exe`.  
3. **`Step 3 - Run_Nova.bat`** — Launches Nova.  
4. **`Step 4 - Create_Shortcut.bat`** — Creates the desktop shortcut (optional).

#### 🔒 Security, Privacy & Control

Nova has no telemetry and stores local history/personality data beside the app. API keys are saved in `nova_config.ini`, so treat that file as private. The `EXEC:` pipeline can run real Windows shell commands; review commands before using Nova on sensitive files or systems.

When using local providers, prompts and attachments stay on your machine. When using cloud providers, requests are sent to the selected provider according to that provider's terms.

**Technical Requirement:** Requires Microsoft Visual Studio Build Tools (MSVC) for source compilation (2019 or 2022).

---

**Official Repository:** https://github.com/94BILLY/NOVA  
**Author:** [94BILLY](https://github.com/94BILLY)

*"Anything is possible."*
