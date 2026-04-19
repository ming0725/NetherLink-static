# 📦 NetherLink‑static

> 基于 Qt 的聊天社交客户端静态演示，Minecraft 风格，展现群聊/私聊、帖子中心、AI 对话等功能的美化界面与动画效果。

---

## 🧭 目录

- [项目介绍](#-项目介绍)
- [核心功能](#-核心功能)
- [技术栈](#-技术栈)
- [界面预览](#-界面预览)
- [安装与启动](#-安装与启动)
- [已知问题](#-已知问题)

---

## 📖 项目介绍

NetherLink‑static 是一个基于 Qt 的聊天社交客户端静态演示，**不包含后端逻辑**
，仅展示界面和动画效果。启动后直接进入主界面，无需登录/注册流程。所有页面、控件与交互均为静态展现，便于演示 UI 设计思路。

---

## ✨ 核心功能

- 🔹 **群聊 & 私聊**
    - 多人群组聊天页面
    - 单对单私聊页面
    - 消息类型：文字、图片

- 🔹 **帖子中心**
    - 浏览帖子列表
    - 支持对帖子 **点赞**
    - 支持对帖子 **发表评论**（暂不支持评论点赞及评论回复）

- 🔹 **AI 对话**
    - 演示与 AI 聊天机器人的对话界面
    - 支持文字互动

- 🔹 **界面美化 & 动画**
    - Minecraft 像素风图标与配色
    - 界面元素动效（滑动、弹出、淡入淡出等）

---

## 🛠 技术栈

| 类别      | 技术                      |
|---------|-------------------------|
| UI 框架   | Qt Widgets (C++ / Qt 6) |
| 动画      | QPropertyAnimation      |
| 图标 & 资源 | Minecraft 风格 PNG        |
| 构建工具    | CMake                   |

---

## 🖼 界面预览

> 以下为示例截图，展示主要页面风格。

1. **主界面**
2. **好友列表**
3. **帖子中心**
4. **AI 对话窗口**

<img src="https://github.com/ming0725/NetherLink-static/blob/master/doc/images/1_chat.jpg?raw=true" alt="聊天界面" width="600"/>

<img src="https://github.com/ming0725/NetherLink-static/blob/master/doc/images/2_friends.png?raw=true" alt="联系人界面" width="600"/>

<img src="https://github.com/ming0725/NetherLink-static/blob/master/doc/images/3_posts.png?raw=true" alt="帖子中心" width="600"/>

<img src="https://github.com/ming0725/NetherLink-static/blob/master/doc/images/4_detail_post.png?raw=true" alt="帖子详情页" width="600"/>

<img src="https://github.com/ming0725/NetherLink-static/blob/master/doc/images/5_aichat.png?raw=true" alt="AI 对话" width="600"/>

---

## 🚀 安装与启动

### 环境要求

- Qt 6.x
- CMake ≥ 3.10
- 支持平台：Windows 11

### 快速运行

```bash
# 克隆仓库
git clone https://github.com/ming0725/NetherLink-static.git
cd NetherLink-static

# 创建构建目录并编译
mkdir build && cd build
cmake ..
cmake --build . --config Release

# 运行可执行文件
./NetherLink-static
```

## ⚠️ 已知问题

- **内存占用较高**：部分页面连续切换或加载大量图片时内存飙升。
- **界面卡顿**：动画切换偶有卡顿现象。

📌 **后续计划**：优化资源加载、添加虚拟化/懒加载机制、进一步调优动画曲线。
