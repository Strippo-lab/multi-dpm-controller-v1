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



🎯 Goal

You have two branches:

main → your stable firmware

dev → your active development

You want to merge only certain parts (a few files or commits) from dev into main — not the whole branch.

🧩 Option 1 — Merge only one or more specific files
🔹 Step 1 — Checkout the branch you want to update
git checkout main

🔹 Step 2 — Pull the file(s) you want from dev
git checkout dev -- src/mqtt_if.cpp


✅ This takes only src/mqtt_if.cpp from the dev branch and places it into your main branch working directory.

You can do this for multiple files:

git checkout dev -- src/mqtt_if.cpp src/config.cpp include/mqtt_if.h

🔹 Step 3 — Commit it
git add .
git commit -m "Merge mqtt_if.cpp from dev branch"
git push


Done ✅ — only those files are merged.

🧠 Option 2 — Merge only specific commits (using cherry-pick)

Let’s say you did multiple commits in dev, but only one (or two) should go to main.

🔹 Step 1 — Switch to main
git checkout main

🔹 Step 2 — Find commit hash in dev
git log dev --oneline


Example output:

a1b2c3d  Fix OTA MD5 verification
e4f5g6h  Improve MQTT reconnect logic

🔹 Step 3 — Cherry-pick the commit(s)
git cherry-pick a1b2c3d


You can even cherry-pick multiple commits:

git cherry-pick a1b2c3d e4f5g6h


If there are no conflicts → done ✅
If conflicts appear, Git will tell you which files need manual resolution.

🔹 Step 4 — Push updated main
git push

⚙️ Option 3 — Merge a folder subtree (partial project)

If you have a big repo but want only one folder (like /lib/), you can do:

git checkout dev -- lib/
git add lib/
git commit -m "Merge updated libraries from dev"
git push

🧼 Option 4 — Use interactive rebase (for advanced cleanup)

If you want to merge multiple commits but squash or edit them before merging:

git checkout dev
git rebase -i main


Then you can pick only what you need before merging normally.

🧠 Summary Table
What you need	Command	Result
Merge one file	git checkout dev -- path/to/file	Only that file imported
Merge one commit	git cherry-pick <commit>	One specific change imported
Merge folder	git checkout dev -- folder/	Only that folder imported
Clean up before merge	git rebase -i main	Choose commits manually
💡 Tips

Always commit or stash your local changes before using checkout or cherry-pick.

You can test the merge on a temporary branch first:

git checkout -b test-merge main
git cherry-pick a1b2c3d


Then review everything safely.

Would you like me to make a small visual diagram showing how main, dev, and feature branches connect — with arrows showing cherry-pick and partial merge flow?
It’s very helpful for visualizing how these commands work.