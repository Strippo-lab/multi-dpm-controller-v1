# 🧭 Git & GitHub Setup Guide for ESP32 / PlatformIO Projects

This guide explains how to take an existing working project folder,
initialize Git, and publish it to a new GitHub repository — with a clean
and professional branch structure.

---

## 🚀 1. Prepare Project

1. Open your working project folder in **Visual Studio Code**.
2. Open a **terminal** (`Ctrl + ö` or `Ctrl + ~`).

---

## 🧩 2. Initialize Local Git Repository

`# 🧭 Git & GitHub Setup Guide for ESP32 / PlatformIO Projects

This guide explains how to take an existing working project folder,  
initialize Git, and publish it to a new GitHub repository — with a clean  
and professional branch structure.

---

## 🚀 Step 1 — Prepare Your Project

1. Open your **working project folder** in **Visual Studio Code**.  
2. Open the **terminal** (`Ctrl + ö` or `Ctrl + ~`).

---

## 🧩 Step 2 — Initialize Local Git Repository

```bash
git init
✅ Creates a local Git repository inside your project (.git folder).

📄 Step 3 — Create a .gitignore File
Create a new file named .gitignore in your project root
and paste this content:

gitignore
Code kopieren
# PlatformIO
.pio/
.vscode/
*.pyc
*.bak
*.tmp
*.log

# OS-specific
.DS_Store
Thumbs.db

# Build output
*.elf
*.bin
*.map
This prevents build and temporary files from being tracked by Git.

💾 Step 4 — Stage and Commit All Files
bash
Code kopieren
git add .
git commit -m "Initial commit"
✅ Saves a snapshot of all your current code.

🌐 Step 5 — Create a New GitHub Repository
Go to 👉 https://github.com/new

Enter a repository name, e.g.

Code kopieren
multi-dpm-control
Choose visibility:

Public → open source

Private → personal or internal

⚠️ Do not initialize with README or .gitignore (you already have them).

Click Create repository.

🔗 Step 6 — Link Local Repo to GitHub
GitHub will show a command like:

bash
Code kopieren
git remote add origin https://github.com/<your-username>/multi-dpm-control.git
Run that line inside your terminal.

🌿 Step 7 — Push the Main Branch
bash
Code kopieren
git branch -M main
git push -u origin main
✅ Your project is now live on GitHub under the main branch.

🪴 Step 8 — Create a Development Branch
bash
Code kopieren
git checkout -b dev
git push -u origin dev
✅ You now have:

Branch	Purpose
main	Stable, release-ready
dev	Daily development work

⚙️ Step 9 — Working on the Dev Branch
bash
Code kopieren
# Save changes
git add .
git commit -m "Describe what you changed"

# Push to GitHub
git push
🔁 Step 10 — Merging Dev → Main (after testing)
bash
Code kopieren
git checkout main
git merge dev
git push
🧠 Step 11 — Create a Feature Branch (Optional)
Use feature branches for big changes or experiments.

bash
Code kopieren
git checkout dev
git checkout -b feature/mqtt-async
# ...work, commit, test...
git push -u origin feature/mqtt-async
When finished:

bash
Code kopieren
git checkout dev
git merge feature/mqtt-async
git push
git branch -d feature/mqtt-async
✅ Step 12 — Verify Repository State
bash
Code kopieren
git status
git branch -a
git log --oneline
🧱 Recommended Folder Structure
css
Code kopieren
multi-dpm-control/
 ├─ .gitignore
 ├─ platformio.ini
 ├─ include/
 ├─ src/
 ├─ lib/
 ├─ data/
 ├─ .vscode/
 └─ README.md
📖 Example README.md Template
markdown
Code kopieren
# Multi DPM Control

Firmware for an ESP32-based multi-channel DPM controller.  
Includes MQTT Async, OTA update, Modbus, and Ethernet/Wi-Fi fallback.

## ✨ Features
- Async MQTT interface  
- OTA firmware updates with MD5 validation  
- Modbus device discovery and control  
- Energy- & time-based DPM automation  
- InfluxDB / Grafana integration

## ⚙️ Build
Use [PlatformIO](https://platformio.org) and select board  
`upesy_wroom` or `esp32-s3-usb`.

## 🌿 Branch Structure
- **main** → stable release  
- **dev** → development branch  
- **feature/** → temporary per-feature branches
🧰 Advanced Git Cheatsheet
Action	Command
Undo last commit (keep files)	git reset --soft HEAD~1
Undo last commit (discard files)	git reset --hard HEAD~1
See file changes	git diff
View commit history	git log --oneline
Rename current branch	git branch -m newname
Delete local branch	git branch -d branchname
Delete remote branch	git push origin --delete branchname
Pull latest remote changes	git pull
Tag version and push	git tag v1.0.0 && git push origin v1.0.0

🎉 You’re Done!