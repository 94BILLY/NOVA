# Nova Demo Recipes

Use these prompts after Nova is installed and connected to a provider. Start with a harmless recipe so you can see the EXEC feedback loop before giving Nova broader tasks.

## 1. Create a desktop workspace

Prompt:
> Create a folder on my Desktop named NovaDemo and add a short README.txt inside it that says this folder was created by Nova.

What it demonstrates:
- Desktop path detection
- Safe filesystem automation
- EXEC feedback loop

## 2. Summarize a local file

Prompt:
> I attached a text or source file. Summarize what it does, list risks, and suggest one improvement.

What it demonstrates:
- Attachment ingestion
- Local-first review workflow
- Practical developer assistance

## 3. Build a tiny C++ app

Prompt:
> Create a small C++ console app on my Desktop that prints "Hello from Nova", compile it with MSVC, and tell me where the EXE is.

What it demonstrates:
- PowerShell file writing
- MSVC compile workflow
- Self-correction from compiler output

## 4. Fetch current web context

Prompt:
> Check the latest weather or news for my area and summarize it in three bullet points.

What it demonstrates:
- Real-time internet fetches
- Concise desktop-assistant output
- Cloud/local provider flexibility

## 5. Switch providers safely

Prompt:
> Help me test whether my current provider settings work, then explain what I should change if the connection fails.

What it demonstrates:
- Multi-provider setup
- Troubleshooting flow
- Clear user-visible status
