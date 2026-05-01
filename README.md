# NOVA v1.5 🌎
### Native Windows Executive Assistant
*"The performance & durability of the past with the intelligence of the future."*

Nova is a native Win32 C++17 desktop assistant that can chat with local or cloud AI providers and execute approved Windows tasks through PowerShell/CMD. It is built for users who want a fast, local-first AI operator without an Electron wrapper.

What Nova does today:
* Connects to 17 local and cloud AI backends.
* Automates Windows tasks through the `EXEC:` command pipeline.
* Reads image, audio, video metadata, and source/text attachments.
* Keeps local history and personality data on your machine.
* Runs as a small native Windows executable.

Try these first: see [`recipes/demo-recipes.md`](recipes/demo-recipes.md) for five safe demo prompts that show folder creation, file summarization, C++ app generation, weather/news lookup, and provider switching.

#### ✨ What You Can Do With Nova 1.5

Use prompts like these to test Nova's core Windows-operator loop:

* "Create a folder on my Desktop called `NovaDemo` and add a `README.txt` explaining what Nova did."
* "Organize my Desktop into folders for images, documents, installers, and code."
* "Create a tiny C++ console app that prints `Hello from Nova`, compile it with MSVC, and tell me where the EXE is."
* "Make a simple Win32 C++ window with a button that says `Click me`."
* "Use Ollama as my provider and help me test whether it is responding."
* "Summarize this attached text or source file without sending it to the cloud."
* "Check today's weather or latest news and give me a three-bullet summary."
* "Update your personality based on how I corrected you."

Things you can make with Nova 1.5:

* Small C++ utilities and Win32 app prototypes.
* Batch scripts for repetitive Windows tasks.
* PowerShell file organization tools.
* Desktop project scaffolds.
* README, checklist, and project-plan generators.
* Local document and source-code summaries.
* Provider test and debug workflows.
* Personal knowledge notes and reusable task recipes.

#### 🚀 Key Improvements in v1.5

* **Bare-Metal Core:** 100% Native C++ execution with zero bloat, native DPI Awareness for crisp 4K displays, and a sleek centered 600px 7-button dashboard layout.
* **Unified 17-Provider Backend:** Seamless switching between local (llama-server, Ollama, LM Studio, vLLM, KoboldCpp, Jan, GPT4All) and cloud endpoints (OpenAI, Anthropic, Groq, Mistral, Together AI, OpenRouter, xAI, Google Gemini, and custom).
* **Hardware Kill-Switch:** Dedicated **Stop** button with thread-safe inference termination to safely abort execution loops.
* **Automated Orchestration:** Custom EXEC engine with single-line command chaining and strict anti-hallucination safeguards for PowerShell/CMD task management.
* **Universal Pathing:** Native support for `%USERPROFILE%` environment variables for reliable cross-machine execution.
* **Multimodal Attachment Analysis:** Native GDI+ image analysis, WAV audio parsing, video metadata via ffprobe, and full source file ingestion.
* **EvolvingPersonality®:** Persistent identity and memory growth between sessions, stored entirely on-device.

#### 🛠 Installation (One Step)
NOVA uses a single installer flow.
1. Download this repository (or just `Install_Nova.bat`) to your Windows machine.
2. Double-click **`Install_Nova.bat`**.
3. The installer automatically:
   - downloads the latest `Nova.exe` from GitHub Releases,
   - downloads and extracts the local `llama-server` engine,
   - downloads the default `llama3.gguf` model (~4.66 GB, resumable),
   - creates a desktop shortcut, and
   - optionally launches NOVA.
**Requirements**
- Windows 10 version 1803 or later
- `curl` and PowerShell available on system PATH
- Stable internet connection
- ~6-8 GB free disk space
**If installation is interrupted**
- Re-run `Install_Nova.bat`; existing files are reused and the model download resumes.
**If you need a clean re-download**
- Delete `Nova.exe` and run `Install_Nova.bat` again.
#### 🔧 Manual Source Build (Advanced)
If you want to build from source instead of using the prebuilt release binary:
1. Install Microsoft Visual Studio Build Tools (MSVC 2019 or 2022).
2. Build `nova.cpp` into `Nova.exe` with your preferred MSVC workflow.
3. Run `Nova.exe` from the project directory.

#### 🔒 Security, Privacy & Control

Nova has no telemetry and stores local history/personality data beside the app. API keys are saved in `nova_config.ini`, so treat that file as private. The `EXEC:` pipeline can run real Windows shell commands; review commands before using Nova on sensitive files or systems.

When using local providers, prompts and attachments stay on your machine. When using cloud providers, requests are sent to the selected provider according to that provider's terms.

**Technical Requirement:** No compiler is required for the standard install flow.  
For manual source compilation, use Microsoft Visual Studio Build Tools (MSVC, 2019 or 2022).

---

**Official Repository:** https://github.com/94BILLY/NOVA  
**Author:** [94BILLY](https://github.com/94BILLY)

*"Anything is possible."*
