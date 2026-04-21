# 📦 NetherLink‑static

> 基于 Qt 的聊天社交客户端静态演示，Minecraft 风格，展现群聊/私聊、帖子中心、AI 对话等功能的美化界面与动画效果。

---

## 🧭 目录

- [项目介绍](#-项目介绍)
- [核心功能](#-核心功能)
- [界面预览](#-界面预览)
- [安装与启动](#-安装与启动)
- [更新日志](#-更新日志)

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

- 🔹 **AI 对话**
    - 静态页面版不展示对话功能，仅有消息列表展示

- 🔹 **界面美化 & 动画**
    - Minecraft 像素风图标与配色
    - 界面元素动效（滑动、弹出、淡入淡出等）


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
- 支持平台：Windows 11，macOS 26

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

## 📅 更新日志

### 2026/4/21 更新

🧩 **帖子瀑布流交互修复**  
移除帖子瀑布流对 `QAbstractItemView` 当前项与焦点滚动链路的依赖，修复图片被底部 Bar 遮挡时打开和关闭详情导致的页面跳动问题，交互过程更稳定自然。

✨ **窗口与平台适配优化**  
完成 macOS 窗口系统适配，重构原生窗口集成与标题栏行为，统一跨平台窗口抽象，改善窗口按钮、拖拽与整体显示效果。

🧱 **项目结构重构**  
按 `app / features / shared / platform` 重新整理工程目录，同时更新 CMake 配置与 `#include` 路径，代码组织从按技术堆叠转向按业务模块划分，后续维护更清晰。

💬 **聊天模块重构**  
消息列表完成滚动与虚拟化方向重构，引入 `Model + List/View + Delegate` 组织方式，聊天列表与消息列表的结构更加稳定。

👥 **好友与帖子体验增强**  
好友列表与帖子流完成虚拟化改造，帖子区域补充 masonry 布局、详情浮层与转场优化，并修复帖子详情动画，大幅度降低卡顿和内存占用。

🤖 **AI 对话与滚动体系优化**  
AI 聊天列表改为新的列表模型结构，统一滚动相关实现到覆盖式滚动组件，减少旧组件分叉带来的维护成本。

🗂 **数据与资源加载整理**  
重构 Repository 层与图片加载流程，引入统一请求类型、仓储模板和 `ImageService`，页面数据组织更一致。

🚀 **性能与加载策略优化**  
为主应用页和帖子页加入懒加载，降低首次初始化开销，页面切换时的资源占用更可控。
