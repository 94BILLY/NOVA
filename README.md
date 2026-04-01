# NOVA v1.5 🌎
### The Executive Desktop Assistant
*"The performance & durability of the past with the intelligence of the future."*

This is the official **v1.5** release of Nova, a high-performance, bare-metal desktop orchestrator built in native Win32 C++17—utilizing the same architecture that powered the most legendary applications of the Windows XP era. Engineered for zero-bloat execution, Nova eliminates the massive overhead of modern web wrappers to reserve 100% of system resources for local intelligence and hardware-accelerated performance.

Nova features a unified 17-provider AI backend, a custom EXEC engine for automated Windows task management, deep multimodal attachment analysis, real-time internet access, and the proprietary **EvolvingPersonality®** system that grows with you entirely on-device.

Due to the strong popularity of v1.0.0 (over **400+ clones** since its initial release), I'm shipping v1.5 with significant stability and usability upgrades while staying true to the original bare-metal philosophy.

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

#### 🔒 Privacy & Sovereignty

Nova operates with **Total Data Sovereignty**. No telemetry, no cloud logging, and no external guardrails. Your intelligence remains entirely on your hardware.

**Technical Requirement:** Requires Microsoft Visual Studio Build Tools (MSVC) for source compilation (2019 or 2022).

---

**Official Repository:** https://github.com/94BILLY/NOVA  
**Author:** [94BILLY](https://github.com/94BILLY)

*"Anything is possible."*
