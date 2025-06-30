# 🚀 Tank Battle - 多人在线坦克大战游戏（项目，C/C++后端）
C/C++选手使用学习了服务端开发项目后，觉得业务代码不够的可以学习这个项目。增加redis、MySQL后会更加适合找工作。

一个基于客户端-服务器架构的多人在线坦克对战游戏，支持2~4名玩家同时在线同一房间对战。游戏使用C语言开发高性能服务器端，Python+Pygame开发友好的客户端界面。

## 📋 目录

- [游戏特色](#-游戏特色)
- [技术架构](#-技术架构)
- [系统要求](#-系统要求)
- [快速开始](#-快速开始)
- [详细安装指南](#-详细安装指南)
- [游戏玩法](#-游戏玩法)
- [网络协议](#-网络协议)
- [技术实现](#-技术实现)
- [配置说明](#-配置说明)
- [故障排除](#-故障排除)
- [开发指南](#-开发指南)
- [贡献指南](#-贡献指南)
- [许可证](#-许可证)

## 🎮 游戏特色

### 核心功能
- **多人在线对战**: 支持2-4名玩家同时游戏
- **房间系统**: 自动房间分配，支持多个房间并发运行
- **实时对战**: 流畅的实时移动、射击和碰撞检测
- **动态地图**: 每局游戏生成不同的随机地图
- **可破坏墙体**: 战术性地破坏障碍物开辟新路径

### 技术特色
- **高性能服务器**: 使用epoll的异步I/O模型
- **线程池架构**: 高效处理并发连接
- **跨平台客户端**: 基于Pygame的图形界面
- **自定义网络协议**: 二进制协议确保低延迟
- **资源管理**: 支持自定义图片资源

## 🏗️ 技术架构

### 服务器端 (C语言)
```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   主线程         │    │   线程池         │    │   房间线程        │
│  - 网络监听      │───▶│  - 客户端处理    │───▶│  - 游戏逻辑      │
│  - 连接管理      │    │  - 消息分发      │    │  - 碰撞检测      │
│  - epoll事件     │    │  - 工作队列      │    │  - 状态同步      │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

### 客户端 (Python)
```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   主线程         │    │   网络线程       │    │   渲染引擎        │
│  - 游戏循环      │◀──▶│  - 数据接收      │───▶│  - 图像渲染      │
│  - 事件处理      │    │  - 协议解析      │    │  - 动画显示      │
│  - 用户输入      │    │  - 状态更新      │    │  - UI绘制        │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## 💻 系统要求

### 服务器端
- **操作系统**: Linux (推荐 Ubuntu 18.04+, CentOS 7+)
- **编译器**: GCC 7.0+ 
- **系统库**: pthread, epoll (Linux系统自带)
- **内存**: 最少128MB RAM
- **网络**: 支持TCP连接的网络环境

### 客户端
- **操作系统**: Windows 10+, macOS 10.14+, Linux
- **Python版本**: Python 3.7+
- **依赖库**: pygame 2.0+
- **内存**: 最少256MB RAM
- **显示**: 支持800x600以上分辨率

## 🚀 快速开始

### 1. 克隆项目
```bash
git clone https://github.com/yourusername/tank-battle.git
cd tank-battle
```

### 2. 启动服务器
```bash
# 编译服务器
gcc -o tank_server server.c -lpthread

# 运行服务器
./tank_server
```

### 3. 启动客户端
```bash
# 安装依赖
pip install pygame

# 运行客户端
python client.py
```

### 4. 开始游戏
- 输入用户名
- 等待其他玩家加入
- 使用方向键移动，空格键射击

## 📦 详细安装指南

### 服务器端安装

#### Ubuntu/Debian
```bash
# 更新包管理器
sudo apt update

# 安装编译工具
sudo apt install build-essential

# 克隆项目
git clone https://github.com/yourusername/tank-battle.git
cd tank-battle

# 编译服务器
gcc -o tank_server server.c -lpthread -std=c99

# 运行服务器
./tank_server
```

#### CentOS/RHEL
```bash
# 安装编译工具
sudo yum groupinstall "Development Tools"

# 克隆项目
git clone https://github.com/yourusername/tank-battle.git
cd tank-battle

# 编译服务器
gcc -o tank_server server.c -lpthread -std=c99

# 运行服务器
./tank_server
```

### 客户端安装

#### Windows
```bash
# 安装Python (从python.org下载)
# 安装pip (通常随Python一起安装)

# 安装依赖
pip install pygame

# 运行客户端
python client.py
```

#### macOS
```bash
# 安装Homebrew (如果还没安装)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装Python
brew install python

# 安装依赖
pip3 install pygame

# 运行客户端
python3 client.py
```

#### Linux
```bash
# Ubuntu/Debian
sudo apt install python3 python3-pip
pip3 install pygame

# CentOS/RHEL
sudo yum install python3 python3-pip
pip3 install pygame

# 运行客户端
python3 client.py
```

## 🎯 游戏玩法

### 基本操作
- **移动**: 使用方向键 ↑↓←→ 控制坦克移动
- **射击**: 按空格键发射子弹
- **目标**: 击败所有其他玩家成为最后的胜利者

### 游戏规则
1. **玩家数量**: 每个房间支持2-4名玩家
2. **胜利条件**: 成为最后存活的玩家
3. **死亡条件**: 被其他玩家的子弹击中
4. **重生机制**: 游戏结束后自动开始新的一轮

### 地图元素
- **🟫 墙体**: 不可穿越，子弹无法穿透
- **🟤 可破坏墙体**: 可被子弹摧毁
- **🟢 空地**: 可自由移动的区域
- **🔴🔵🟡🟢 坦克**: 不同颜色代表不同玩家

### 战术提示
- 利用墙体作为掩护
- 战略性地破坏墙体开辟新路径
- 预判敌人移动方向
- 控制好射击时机

## 🌐 网络协议

### 协议设计
游戏使用自定义的二进制协议进行通信，确保低延迟和高效率。

### 客户端到服务器
```
登录消息: 'L' + 用户名(UTF-8字符串)
移动消息: 'M' + 玩家ID + 方向(0-3)
射击消息: 'S' + 玩家ID
```

### 服务器到客户端
```
房间分配: 'R' + 房间ID + 玩家ID
游戏更新: 'U' + 房间ID + 地图数据 + 玩家数据 + 子弹数据 + 游戏状态
游戏开始: 'G' + 房间ID
游戏结束: 'O' + 获胜者ID
```

### 数据结构
- **方向码**: 上(0), 右(1), 下(2), 左(3)
- **地图码**: 空地(0), 墙体(1), 可破坏墙体(2), 坦克(3-6)
- **玩家状态**: 位置(x,y), 方向, 存活状态, ID, 用户名

## 🔧 技术实现

### 服务器端核心技术

#### 1. 异步I/O模型
```c
// 使用epoll实现高效的事件驱动编程
int epoll_fd = epoll_create1(0);
struct epoll_event events[MAX_EVENTS];

while (1) {
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 50);
    // 处理网络事件
}
```

#### 2. 线程池管理
- 预创建16个工作线程
- 使用条件变量进行线程同步
- 工作队列管理客户端请求

#### 3. 房间管理系统
- 动态房间创建和销毁

#### 4. 碰撞检测算法
```c
// 子弹与玩家碰撞检测
for (int j = 0; j < room->game.player_count; j++) {
    Player *p = &room->game.players[j];
    if (p->alive && p->x == b->x && p->y == b->y && 
        b->owner_id != p->id) {
        // 处理碰撞
    }
}
```

### 客户端核心技术

#### 1. 多线程架构
- 主线程处理游戏逻辑和渲染
- 网络线程负责数据接收和处理
- 线程间通过共享状态通信

#### 2. 资源管理系统
```python
def load_resources(self):
    # 支持自定义图片资源
    if os.path.exists("tank.png"):
        base_tank = pygame.image.load("tank.png")
    else:
        # 使用程序生成的默认图像
        base_tank = self.create_default_tank()
```

#### 3. 状态同步机制
- 服务器权威的游戏状态
- 客户端预测和回滚
- 平滑的插值显示

## ⚙️ 配置说明

### 服务器配置
```c
#define MAX_PLAYERS 4        // 每房间最大玩家数
#define MAX_ROOMS 10         // 最大房间数
#define MAP_WIDTH 20         // 地图宽度
#define MAP_HEIGHT 20        // 地图高度
#define MAX_BULLETS 32       // 最大子弹数
#define SERVER_PORT 8888     // 服务器端口
#define THREAD_POOL_SIZE 16  // 线程池大小
```

### 客户端配置
```python
MAP_WIDTH = 20              # 地图宽度
MAP_HEIGHT = 20             # 地图高度
TILE_SIZE = 30              # 瓦片大小
SCREEN_WIDTH = 600          # 屏幕宽度
SCREEN_HEIGHT = 640         # 屏幕高度
SERVER_PORT = 8888          # 服务器端口
```

### 自定义资源
将以下图片文件放在客户端目录中可替换默认图像：
- `tank.png`: 坦克图像 (30x30像素)
- `bullet.png`: 子弹图像 (10x10像素)
- `block.png`: 可破坏墙体图像 (30x30像素)

## 🛠️ 故障排除

### 常见问题

#### 1. 服务器无法启动
**问题**: `bind: Address already in use`
**解决**: 
```bash
# 查找占用端口的进程
lsof -i :8888
# 终止进程
kill -9 <PID>
```

#### 2. 客户端连接失败
**问题**: `Connection refused`
**解决**: 
- 确认服务器已启动
- 检查防火墙设置
- 验证IP地址和端口

#### 3. 游戏卡顿
**问题**: 游戏画面不流畅
**解决**: 
- 关闭其他占用资源的程序
- 降低游戏分辨率
- 检查网络延迟

#### 4. 编译错误
**问题**: `undefined reference to pthread_create`
**解决**: 
```bash
gcc -o tank_server server.c -lpthread
```

### 调试模式
启用详细日志输出：
```bash
# 服务器调试
./tank_server --debug

# 客户端调试
python client.py --debug
```

## 👨‍💻 开发指南

### 代码结构
```
tank-battle/
├── server.c              # 服务器端源码
├── client.py             # 客户端源码
├── README.md             # 项目文档
├── LICENSE               # 许可证
│── tank.png              # 游戏资源
│── bullet.png
│── block.png

```

### 添加新功能

#### 1. 添加新的游戏元素
```c
// 在服务器端定义新元素
#define POWER_UP 7

// 在地图生成中添加
if (rand() % 100 < 5) {
    room->game.map[y][x] = POWER_UP;
}
```

#### 2. 扩展网络协议
```c
// 添加新的命令类型
#define CMD_POWER_UP 'P'

// 在消息处理中添加case
case CMD_POWER_UP:
    handle_power_up(client_fd, buffer, len);
    break;
```

### 性能优化

#### 1. 服务器优化
- 使用内存池减少内存分配
- 优化数据结构布局
- 实现更高效的碰撞检测算法

#### 2. 客户端优化
- 实现精灵批处理
- 使用脏矩形更新
- 优化网络数据解析

## 🤝 贡献指南

### 如何贡献
1. Fork 本项目
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建Pull Request

### 贡献规范
- 遵循现有的代码风格
- 添加适当的注释
- 包含必要的测试
- 更新相关文档

### 报告问题
使用GitHub Issues报告bug或建议新功能：
1. 使用清晰的标题
2. 提供详细的问题描述
3. 包含重现步骤
4. 附上相关的日志信息

## 📈 路线图

### 版本 1.1 (计划中)
- [ ] 添加道具系统
- [ ] 实现观战模式
- [ ] 支持自定义地图
- [ ] 添加排行榜功能

### 版本 1.2 (计划中)
- [ ] 支持团队模式
- [ ] 添加AI机器人
- [ ] 实现回放系统
- [ ] 优化网络协议

### 版本 2.0 (远期计划)
- [ ] 3D图形界面
- [ ] 语音聊天功能
- [ ] 账户系统
- [ ] 移动端支持

## 📄 许可证

本项目采用MIT许可证 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 👥 贡献者

感谢所有为这个项目做出贡献的开发者！


## 🙏 致谢

- 感谢Pygame社区提供的优秀游戏开发框架
- 感谢所有测试者和反馈者
- 灵感来源于经典坦克大战游戏

---

⭐ 如果这个项目对你有帮助，请给它一个星标！
