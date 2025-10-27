# 🖥️ Visual Git Workflow in VS Code

A quick guide for managing your Git workflow directly inside **Visual Studio Code** — no terminal needed!

---

## 🧩 1️⃣ Open the Source Control View

- Click the **Source Control** icon on the left sidebar  
  (or press `Ctrl + Shift + G`)
- You’ll see lists for **CHANGES** and **STAGED CHANGES**

---

## 💾 2️⃣ Stage and Commit Changes

1. Hover a file under **CHANGES** → click the **➕ Plus** icon to stage  
2. Type a commit message at the top  
3. Press **Ctrl + Enter** or click the **✔ Commit** button

✅ Your changes are committed locally.

---

## 🔄 3️⃣ Push / Pull to GitHub

At the top of the Source Control panel:

- **🔄 Sync Changes** → pushes & pulls together  
  (same as `git push` + `git pull`)

or use the **Command Palette** →  
`Ctrl + Shift + P` → “Git: Push” / “Git: Pull”

---

## 🌿 4️⃣ Switch or Create Branches

At the **bottom-left corner**, click the current branch name (e.g. `dev`).

- Select an existing branch (`main`, `dev`)  
- Or click **+ Create new branch** → name it (e.g. `feature/mqtt-async`)

VS Code automatically switches to the new branch.

---

## 🪴 5️⃣ Merge Branches Graphically

1. Open the **Command Palette** (`Ctrl + Shift + P`)  
2. Type **Git: Merge Branch**  
3. Choose which branch to merge into the current one (for example: merge `dev` into `main`)

Confirm, review the diff, and commit if prompted.

---

## 🧱 6️⃣ Tag a Version (v1.0.0 etc.)

1. Open the **Command Palette** → `Ctrl + Shift + P`  
2. Search **Git: Create Tag**  
3. Enter the version, e.g. `v1.0.0`  
4. Push tags:  


✅ Appears on GitHub under **Releases**.

---

## 🌳 7️⃣ Install These Two Extensions

### 🔹 Git Graph
Visual timeline of all commits & branches.

- `Ctrl + Shift + X` → search **“Git Graph”** → **Install**

Open:  
`Ctrl + Shift + P` → **Git Graph: View Git Graph**

Features:
- Visual tree of all branches  
- Right-click → **Merge**, **Checkout**, **Tag**, **Delete Branch**  
- Push / Pull buttons on top right

### 🔹 GitLens
Detailed file history & commit info.

- `Ctrl + Shift + X` → search **“GitLens”** → **Install**

Open GitLens sidebar → see:
- Commits per file  
- Author info inline  
- File history and diff view  

---

## ⚙️ 8️⃣ Recommended Settings

### **Git Graph**
- ✅ Show Tags on Graph  
- ✅ Show Remote Branches  
- ✅ Show Avatars

### **GitLens**
- ✅ Show Commit Age  
- ✅ Enable Blame Annotation on Hover  
- ✅ Enable File History View  

---

## 🧠 9️⃣ Example Visual Workflow

| Task | How | Tool |
|------|------|------|
| Stage & Commit | ➕ then ✔ Commit | Source Control |
| Switch Branch | Click branch name (bottom-left) | VS Code |
| Merge Dev → Main | Command Palette → “Git: Merge Branch” | VS Code |
| Create Tag | Command Palette → “Git: Create Tag” | VS Code |
| View Graph | `Ctrl + Shift + P → Git Graph` | Git Graph |
| Inspect File History | Right-click file → “Open File History” | GitLens |

---

## 🧩 10️⃣ Pro Tips

- `Ctrl + Shift + P → Git: Fetch` to refresh remote changes  
- Use **Git Graph** to see `main`, `dev`, and tags visually  
- Right-click commits to **Cherry-Pick**, **Revert**, or **Tag**  
- Mix GUI + Terminal freely — both stay in sync  

---

## 🧭 Summary

| Tool | Shortcut | Purpose |
|------|-----------|----------|
| **Source Control** | `Ctrl + Shift + G` | Commit, stage, sync |
| **Command Palette** | `Ctrl + Shift + P` | All Git commands |
| **Git Graph** | `Ctrl + Shift + P → Git Graph` | Visual timeline |
| **GitLens** | Sidebar icon | Detailed file history & blame |

---

✅ **Result:**  
You can now manage your entire Git workflow — branching, merging, tagging, and visual history — directly inside Visual Studio Code with a clear, professional interface.

