# my-first-repo

This is my first GitHub repository — a place to practice Git and keep my code.

## What I'm learning

- `git init` / `git add` / `git commit` — the basic loop
- `git remote add origin <url>` — connect a local repo to GitHub
- `git push` / `git pull` — keep local and remote in sync
- branches, merges, and pull requests

## Daily commands

```bash
git status            # see what changed
git add .             # stage everything
git commit -m "msg"   # record a snapshot
git push              # upload to GitHub
```

## 项目 / Projects

### `game/` — C 语言小游戏

一个用 C 语言写的控制台小游戏，含修复后的版本与网页版。

| 文件 | 说明 |
| --- | --- |
| `game.c` | 修复后的主程序源码 |
| `game_original.c` | 最初的原始版本（留作对比） |
| `game.html` | 网页版（浏览器直接打开即可玩） |
| `build.bat` | Windows 下的一键编译脚本 |
| `修复说明.md` | 改了什么、为什么这么改 |
| `预览.png` | 运行截图 |

编译：双击 `build.bat`，或用 MinGW 执行 `gcc game.c -o game.exe`。
（`*.exe` 已在 `.gitignore` 中忽略，编译产物不上传。）

---

Created with help from WorkBuddy.
