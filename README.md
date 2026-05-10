<h1 align="center" style="margin-top:0;margin-bottom:0;">
  <img src="https://94billy.com/wp-content/uploads/2026/03/Nova.png" alt="Nova Logo" width="100" /><br />
  <span style="display:inline-block;margin-top:2px;">Announcing NOVA v1.5.1 & Nova Pro </span>
</h1>

<p align="center"><strong>The Executive Desktop Assistant</strong></p>
<p align="center"><em>"The performance &amp; durability of the past with the intelligence of the future."</em></p>

Nova is the first native Win32 C++17 executive desktop AI assistant that connects to local or cloud AI providers and can automate real Windows tasks through PowerShell/CMD. No Electron. No bloat. No bullsh*t.

---

> **Nova Pro** — Dark mode, Dev Mode, and local image generation. Lifetime license, $19.99 Founding Member License.
> **[Get Nova Pro on Gumroad →](https://94billy.gumroad.com/l/NOVA)**

---

**What Nova does:**
* Connects to 17 local and cloud AI backends
* Automates Windows tasks through the `EXEC:` command pipeline
* Reads image, audio, video metadata, and source/text attachments
* Keeps local history and personality data on your machine
* Runs as a small native Windows executable (~3MB, ~14MB heap)

Try these first: see [`recipes/demo-recipes.md`](recipes/demo-recipes.md) for five safe demo prompts.

---

## What You Can Do With Nova 1.5.1

* "Create a folder on my Desktop called `NovaDemo` and add a `README.txt` explaining what Nova did."
* "Organize my Desktop into folders for images, documents, installers, and code."
* "Create a tiny C++ console app that prints `Hello from Nova`, compile it with MSVC, and tell me where the EXE is."
* "Make a simple Win32 C++ window with a button that says `Click me`."
* "Use Ollama as my provider and help me test whether it is responding."
* "Summarize this attached text or source file without sending it to the cloud."
* "Check today's weather or latest news and give me a three-bullet summary."
* "Update your personality based on how I corrected you."

**Things you can make with Nova:**
* Small C++ utilities and Win32 app prototypes
* Batch scripts for repetitive Windows tasks
* PowerShell file organization tools
* Desktop project scaffolds
* README, checklist, and project-plan generators
* Local document and source-code summaries
* Provider test and debug workflows
* Personal knowledge notes and reusable task recipes

---

## Key Improvements in v1.5.1

* **Real responses. Casual tone.** Nova talks like a person, not a status report.
* **EXEC confirmation.** Destructive commands require explicit user approval.
* **Auto-loop fixed.** Nova no longer self-executes without user input.
* **Bare-Metal Core:** Pure C++17, zero dependencies, no Qt, no web views.
* **Unified 17-Provider Backend:** llama-server, Ollama, LM Studio, vLLM, KoboldCpp, Jan, GPT4All, OpenAI, Anthropic, Groq, Mistral, Together AI, OpenRouter, xAI, Google Gemini, and custom.
* **Hardware Kill-Switch:** Thread-safe Stop button for immediate inference termination.
* **Automated Orchestration:** EXEC engine with single-line command chaining (`&&`).
* **Universal Pathing:** `%USERPROFILE%` environment variable support.
* **Multimodal Attachment Analysis:** GDI+ image analysis, WAV audio, video metadata, source file ingestion.
* **EvolvingPersonality®:** Persistent identity and memory growth between sessions, stored entirely on-device.

---

## Installation

1. Download this repository (or just `Install_Nova.bat`) to your Windows machine.
2. Double-click **`Install_Nova.bat`**.
3. The installer automatically:
   - downloads the latest `Nova.exe` from GitHub Releases,
   - downloads and extracts the local `llama-server` engine,
   - downloads the default `llama3.gguf` model (~4.66 GB, resumable),
   - creates a desktop shortcut.

**Requirements**
- Windows 10 version 1803 or later
- `curl` and PowerShell available on system PATH
- Stable internet connection
- ~6-8 GB free disk space

**If installation is interrupted** — Re-run `Install_Nova.bat`; existing files are reused and the model download resumes.

**If you need a clean re-download** — Delete `Nova.exe` and run `Install_Nova.bat` again.

---

## Manual Source Build (Advanced)

1. Install Microsoft Visual Studio Build Tools (MSVC 2019 or 2022).
2. Build `nova.cpp` into `Nova.exe` using `Step_2_Compile_Nova.bat`.
3. Run `Nova.exe` from the project directory.

---

## Nova Pro

Nova Pro is the paid tier — a lifetime license with dark mode, Dev Mode, image generation via local Stable Diffusion, and a native installer.

**[Get Nova Pro → 94billy.gumroad.com/l/NOVA](https://94billy.gumroad.com/l/NOVA)**

---

## Security, Privacy & Control

Nova has no telemetry. API keys are saved in `nova_config.ini` — treat it as private. The `EXEC:` pipeline can run real Windows shell commands; review commands before using Nova on sensitive files or systems. Local providers keep all data on your machine. Cloud providers send requests per their own terms.

---

<div align="center">

[github.com/94BILLY/NOVA](https://github.com/94BILLY/NOVA) · [94billy.com/NOVA](https://www.94billy.com/NOVA)

**94BILLY**

*"Anything is possible."*

© 2026 94BILLY · All rights reserved

</div>
