# LinHubWin

面向 **Windows 7 x64 / Windows 10 / Windows 11** 的终端连接管理器，由麒麟版 [LinHub](../LinHub) 移植而来。界面与会话库保持一致：左侧会话树、标签终端、右侧 SFTP、底部发送栏。

编译用 **Qt 5.15.2 + MinGW 8.1**（最后一套同时覆盖 Win7 和新系统的官方组合）。Win10 1809+ 使用 ConPTY；Win7 回退到匿名管道。SSH 走本机或捆绑的 OpenSSH。

本机工具链已装在 `F:\Qt`：

```
F:\Qt\5.15.2\mingw81_64          Qt 5.15.2
F:\Qt\Tools\mingw810_64          MinGW-w64 8.1
F:\Qt\Tools\CMake_64             CMake
F:\Qt\use-qt5152.bat             打开环境
```

## 功能

- **协议**：SSH、SFTP、Telnet、本地 PowerShell（pwsh 优先）
- **会话库**：分组、收藏、标签、备注、搜索、复制、导入/导出、从 `%USERPROFILE%\.ssh\config` 导入
- **终端**：自研 VT/ANSI、256 色、真彩色、中文宽字符、输入法、右键复制/粘贴
- **Xshell 风格**：会话日志、发送栏、快速命令、广播到全部标签
- **MobaXterm 风格**：快速连接条、标签页、侧栏 SFTP、SSH 端口转发
- **SFTP**：拖拽上传下载、文件夹上传、新建/重命名、传输历史
- **历史**：连接起止时间、时长、结果、日志路径

Windows 版与 Linux 版的差异：

| 项目 | Windows |
| --- | --- |
| 伪终端 | Win10 1809+ 用 ConPTY；Win7 用匿名管道 |
| 本地会话 | PowerShell 7 或 `powershell.exe` |
| SSH | 捆绑的 `ssh.exe`，否则 `System32\OpenSSH\ssh.exe` |
| SSH 复用 | 不使用 ControlMaster（每次独立连接） |
| 串口 | 暂未接入，会话会提示改用 SSH / 本地终端 |
| X11 转发 | 需本机已有 X server（如 VcXsrv），并设置 `DISPLAY` |

## 依赖

1. **Qt 5.15.2 + MinGW 8.1 + CMake**（见 `F:\Qt\use-qt5152.bat`）
2. **内置 OpenSSH 8.1 客户端**（`third_party/openssh`，编译后复制到 `linhub.exe` 同目录）。Win7 没有系统 OpenSSH，程序会优先用自带的 `ssh.exe`。重新下载：`powershell -File tools\fetch-openssh.ps1`

启用系统 OpenSSH：

```powershell
Add-WindowsCapability -Online -Name OpenSSH.Client~~~~0.0.1.0
```

Telnet 可选：

```powershell
Enable-WindowsOptionalFeature -Online -FeatureName TelnetClient
```

## 编译

```bat
call F:\Qt\use-qt5152.bat
cd /d e:\c++project\LinHubWin
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=F:\Qt\5.15.2\mingw81_64
cmake --build build
```

运行：

```bat
build\linhub.exe
```

用 `windeployqt` 收集 Qt DLL（发布用）：

```bat
windeployqt --release build\linhub.exe
```

`linhub-askpass.exe` 必须与 `linhub.exe` 在同一目录，密码登录才会走 ASKPASS。

## 数据位置

```
%APPDATA%\LinHub\LinHub\linhub.sqlite
%APPDATA%\LinHub\LinHub\logs\
```

密码按本机标识做本地混淆存储，**生产环境请优先使用 SSH 私钥 / ssh-agent**。

## 架构

```
会话树 / 历史          标签终端 (ConPTY + VT)         SFTP
SQLite 会话库    <--->  OpenSSH / telnet / pwsh    <--> ssh 远程命令
```

系统 OpenSSH 负责认证与算法，LinHubWin 负责会话资产、终端体验和记录。
