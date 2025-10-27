# 🧠 Git & Firmware Versioning Command Reference

> Quick reference for managing **ESP32 / DPM firmware versions** with  
> **Git + PlatformIO + GitHub Actions**  
> Works in Visual Studio Code or any terminal (PowerShell / Git Bash / Linux Shell)

---

## 📁 Project Structure

| Folder / File | Purpose |
|----------------|---------|
| `scripts/git_versioning.py` | Reads latest Git tag → injects firmware version into build |
| `scripts/rename_firmware.py` | Renames, archives, exports `.bin`, creates Git tag |
| `scripts/get_git_version.py`  |✅ helper for git_versioning.py|
| `.github/workflows/release.yml` | (Optional) Builds & uploads firmware release on tag push |
| `/firmware_builds/` | Local firmware archive |
| `/apache/firmware/` | OTA export folder served by Apache |
| `firmware_info.h` | C++ header printing version & build time |

---

## 🧭 Daily Git Basics

| Command | Description |
|----------|-------------|
| `git status` | Show changed files |
| `git add .` | Stage all modified files |
| `git commit -m "Message"` | Save current snapshot |
| `git log --oneline --graph` | Compact history overview |
| `git diff` | Show code differences |
| `git restore <file>` | Discard local changes |
| `git branch` | List or show current branch |

---

## 🧩 Version Tags – Core of Firmware Versioning

| Command | Description |
|----------|-------------|
| `git tag` | List all firmware versions |
| `git tag v1.4.4 -m "Stable OTA release"` | Create new firmware version tag |
| `git tag -d v1.4.3` | Delete local tag |
| `git push origin :refs/tags/v1.4.3` | Delete remote tag |
| `git show v1.4.4` | Inspect tagged commit |
| `git describe --tags` | Show latest tag (used by build scripts) |

🧠 **Tip:** Each tag = new firmware version number (read automatically by `git_versioning.py`).

---

## ⚙️ Building & Releasing

| Command | Description |
|----------|-------------|
| `pio run` | Build firmware (auto-injects version from Git tag) |
| `pio run -t clean && pio run` | Clean rebuild |
| `git push && git push --tags` | Push code + version tags to GitHub |
| `pio device monitor` | View boot log & firmware version on serial |
| `git describe --tags` | Verify build version being used |

📦 After build you’ll see files like  
`firmware_v1.4.4_20251025_1420.bin` in  
`.pio/build/...`, `firmware_builds/`, and `/apache/firmware/`

---

## 🚀 Full Release Workflow

```bash
# 1️⃣  Commit code changes
git add .
git commit -m "Improve watchdog timing"

# 2️⃣  Tag new version
git tag v1.4.5 -m "Release v1.4.5"

# 3️⃣  Build firmware
pio run

# 4️⃣  Push code + tags to GitHub
git push
git push --tags
