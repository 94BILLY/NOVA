# Nova 🌎
### The Executive Desktop Assistant
*"The performance & durability of the past with the intelligence of the future."*

Nova is a high-performance, bare metal desktop orchestrator built in native Win32 C++17—utilizing the same architecture that powered the most legendary applications of the Windows XP era. Engineered for zero-bloat execution, it features a custom execution engine for automated Windows task management, deep attachment analysis, real-time internet access, and a proprietary **EvolvingPersonality®** system.

---

### 1. Core Philosophy
* **Bare-Metal Performance:** Zero Electron, zero frameworks, and zero bloat. Direct Win32 API access ensures maximum system efficiency and legendary reliability.
* **Absolute Privacy:** Built on a foundation of total data sovereignty. All chat history and operational logs remain strictly on-device and are never uploaded to the internet.
* **Proprietary Identity:** Powered by the **EvolvingPersonality®** system, Nova maintains persistent memory and identity growth between sessions.
* **Unrestricted Power:** The model operates without restrictive external guardrails. Users have full authority to refine Nova’s behavior to match professional requirements.

### 2. Key Features
* **Universal Pathing:** Native detection of $DESKTOP and OneDrive redirects to ensure stability across varying hardware configurations.
* **EXEC Engine:** Automated PowerShell and CMD orchestration for system tasks, code generation, and automated compilation.
* **Multimodal Analysis:** Native GDI+ processing for attachments, including high-fidelity images, WAV audio, and video logic.
* **Synchronous Boot:** Specialized loading sequence ensuring the engine is 100% VRAM-ready before the interface is initialized.

### 3. Operational Protocol: Software Architect
Nova operates as an automated software architect following a rigid dual-execution constraint to ensure kernel-level precision:

1. **File Creation:** EXEC: powershell -Command "Set-Content -Path '$DESKTOP\hello.cpp' -Value '#include <iostream>', '', 'int main() {', '    std::cout << \"Hello World!\" << std::endl;', '    return 0;', '}'"
2. **Compilation:** EXEC: cmd /c "cd /d $DESKTOP && cl /O2 /EHsc /std:c++17 /Fe:hello.exe hello.cpp"

### 4. Technical Specifications
* **Operating System:** Windows 10 / 11 (x64)
* **Hardware:** 8GB VRAM (RTX 3060) Minimum | 12GB+ VRAM Recommended
* **Compiler:** MSVC (Visual Studio 2019 / 2022)

### 5. Installation
1. **Provisioning:** Run **Step 1 - Setup_Nova.bat** to initialize the engine and fetch model weights.
2. **Verification:** Run **Step 2 - Save_Changes.bat** to verify the environment and compile the binary.
3. **Initialization:** Run **Step 3 - Run_Nova.bat** to start the engine and launch the Nova UI.
4. **Integration:** Run **Step 4 - Create_Shortcut.bat** to generate your executive desktop icon.

---
**Official Site:** [94billy.com/nova](https://94BILLY.COM/NOVA)  
**Author:** [94BILLY](https://github.com/94BILLY)  

**"Anything is possible."**
