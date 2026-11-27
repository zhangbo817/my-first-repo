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

**在线试玩：<https://zhangbo817.github.io/my-first-repo/>** （浏览器版，打开即玩，无需安装）

一个用 C 语言写的控制台小游戏，含修复后的版本与网页版。仓库根目录的 `index.html` 是
站点入口页（GitHub Pages 用），点「开始游戏」进入 `game/game.html`。

| 文件 | 说明 |
| --- | --- |
| `game.c` | 修复后的主程序源码 |
| `game_original.c` | 最初的原始版本（留作对比） |
| `game.html` | 网页版（浏览器直接打开即可玩） |
| `build.bat` | Windows 下的一键编译脚本 |
| `修复说明.md` | 改了什么、为什么这么改 |
| `预览.png` | 运行截图 |
| `bosses.png` | 七种 BOSS 一览图 |

#### BOSS 图鉴（网页版）

六个 BOSS 关各有独立机制，最终 BOSS 把前面所有技能揉在一起：

| 关卡 | BOSS | 机制 |
| --- | --- | --- |
| 第 5 关 | 晶簇核心 | 扇形弹幕 + 环形扩散 |
| 第 10 关 | 棱光之眼 | 蓄力激光扫射（红色预警线 → 光束扫过） |
| 第 15 关 | 熔核巨像 | 陨石雨，地面预警圈 |
| 第 20 关 | 裂空之刃 | 蓄力冲刺撞击，落地冲击环 |
| 第 25 关 | 虫巢母体 | 召唤小怪 + 追踪弹 |
| 第 30 关 | 虚空主宰 | 瞬移 + 螺旋弹幕 |
| 最终 | 终焉·母舰 | 前六种技能全部登场（分三阶段解锁） |

想看全部外形：打开 `game/game.html?gallery=1`（静态图鉴，见 `bosses.png`）。

编译：双击 `build.bat`，或用 MinGW 执行 `gcc game.c -o game.exe`。
（`*.exe` 已在 `.gitignore` 中忽略，编译产物不上传。）

---

Created with help from WorkBuddy.
