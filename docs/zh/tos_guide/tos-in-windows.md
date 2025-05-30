---
title: Windows 系统下 tos.py 工具使用指南
---

## 概述

`tos.py` 是 TuyaOpen 的构建工具，支持项目创建、编译、配置等多种功能。本文档将指导您在 Windows 10/11 系统下使用 tos.py 工具进行开发调试。

### 功能特点
- 支持项目创建和编译
- 提供项目配置管理
- 支持固件烧录
- 支持设备日志监控
- 支持版本管理和环境检查

## 前提条件

在开始使用之前，请确保您的系统满足以下要求：
- Windows 10/11 操作系统
- Python 3.10.0 或更高版本 [https://apps.microsoft.com/detail/9pjpw5ldxlz5]
   - Windows Store App 商城下载安装
- Git 2.0.0 或更高版本 [https://git-scm.com/downloads/win]
   - 下载安装包，手动进行安装
- CMake 4.0.2 或更高版本 [https://cmake.org/download/]
   - 下载安装包，手动进行安装
- Ninja 1.11.1 或更高版本 [https://github.com/ninja-build/ninja/releases]
   - Ninja 安装说明
      1. 下载 Ninja
         - 访问 [Ninja 发布页面](https://github.com/ninja-build/ninja/releases)
         - 下载最新版本的 `ninja-win.zip`

      2. 安装步骤
         - 解压下载的 `ninja-win.zip` 文件
         - 将解压出的 `ninja.exe` 复制到以下任一位置：
         - `C:\Windows`（需要管理员权限）
         - 或添加到系统环境变量 Path 中的任意目录
         - 或直接放在 TuyaOpen 项目根目录下

      3. 验证安装
         ```powershell
         ninja --version
         ```

      :::tip 提示

      - 如果选择将 ninja.exe 放在项目目录下，确保在运行 `export.bat` 时 ninja.exe 已经存在
      - 建议将 ninja.exe 放在系统目录或添加到环境变量中，这样可以全局使用  

      :::

## 使用限制
:::tip  
2025/05/30 目前 Windows 开发环境下的 tos.py 工具暂时只支持 ESP32。 Tuya T5 芯片 计划`6月中`支持
:::

## 快速开始

1. 克隆 TuyaOpen 仓库
```powershell
# PowerShell 或 CMD
git clone https://github.com/tuya/TuyaOpen.git
cd TuyaOpen
git submodule update --init
```

2. 配置开发环境
```powershell
# PowerShell 或 CMD
.\export.bat
```

3. 验证环境
```powershell
# PowerShell 或 CMD
tos.py version
```

4. 退出开发环境
```powershell
# PowerShell 或 CMD
exit
```

:::tip 命令行选择
您可以使用以下任意一种命令行工具：
- PowerShell（推荐）
- Windows 命令提示符（CMD）

:::warning 注意
Git Bash（类 Linux 环境）与 tos.py 工具不兼容，请使用 PowerShell 或 CMD。
:::

## 使用说明

### 基本命令
```shell
tos.py [OPTIONS] COMMAND [ARGS]...

选项:
  -d, --debug  显示调试信息
  -h, --help   显示帮助信息

命令:
  version  显示版本信息
  check    检查依赖工具
  config   配置文件操作
  build    构建项目
  clean    清理项目
  flash    烧录固件
  monitor  显示设备日志
```

### 常用操作示例

1. 检查环境
```shell
# 工程目录下
tos.py check
```

- 确保工具链符合要求。如果有缺失工具，参照此文档上述方法进行安装。
   ```powershell
   (tos) $ tos.py check
   [INFO]: Running tos.py ...
   [INFO]: [git] (2.49.0 >= 2.0.0) is ok.
   [ERROR]: [cmake] not found, please install.
   [INFO]: [ninja] (1.12.1 >= 1.6.0) is ok.
   [INFO]: Downloading submoudules ...
   [INFO]: Download submoudules successfully.
    ```

2. 编译项目
```shell
# 工程目录下
tos.py build
```

3. 清理项目
```shell
# 工程目录下
tos.py clean
```

4. 烧录固件
```shell
# 工程目录下
tos.py flash
```
- 烧录流程
   - 连接 USB 到开发板上的USB串口
   - `tos.py flash` 运行串口烧写命令后：
      - T5 复位按键
      - ESP32  复位+Boot按键


5. 配置项目 (芯片/功能等)
   ```shell
   # 工程目录下
   tos.py config
   ```
   输出：
   ```shell
   Usage: tos.py config [OPTIONS] COMMAND [ARGS]...

   Configuration file operation.

   Options:
   -h, --help  Show this message and exit.

   Commands:
   choice  Choice config file.
   menu    Menuconfig.
   save    Save minimal config.
   ```

   5.1. 配置芯片平台  
      - TuyaOpen 支持了芯片跨平台能力，能快速切换兼容适配不同芯片平台。 
      ```shell
      # 工程目录下
      tos.py config choice
      ```
      输出：
      ```
      [INFO]: Running tos.py ...
      --------------------
      1. BK7231X.config
      2. ESP32-C3.config
      3. ESP32-S3.config
      4. ESP32.config
      5. LN882H.config
      6. T2.config
      7. T3.config
      8. T5AI.config
      9. Ubuntu.config
      --------------------
      Input "q" to exit.
      Choice config file:
      ```



## 常见问题

1. 问题：找不到 tos.py 命令
   解决方案：
   - 确保已经执行 `.\export.bat` 进入开发环境
   - 检查是否在 TuyaOpen 目录下

2. 问题：pip 安装依赖包失败
   解决方案：
   - 检查网络连接
   - 尝试使用国内镜像源：
     ```shell
     pip install -r requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple
     ```

## 后续操作
- 查看 [项目示例](../examples/index.md) 了解更多使用场景，开始编译项目。


<!-- todo：其他新增文档，"用tos 创建新项目"-->