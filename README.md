# Spiration

[![](https://github.com/flamence/spiration/actions/workflows/cmake-multi-platform.yml/badge.svg?branch=gui)](https://github.com/flamence/spiration/actions/workflows/cmake-multi-platform.yml)

## 快速开始

前往 [Release](https://github.com/flamence/spiration/releases) 页面下载最新稳定的版本，

或是在 [Actions](https://github.com/flamence/spiration/actions) 中找到最近一次成功的构建。

## 运行要求

### Windows

推荐使用 Windows 10+ 系统运行，
需要确保设备支持 Direct2D。

*(Windows 11 通过测试)*

### Mac OS

确保系统版本不低于 10.15。

### Linux

需要支持 X11 桌面，Wayland 暂未测试。

*(Ubuntu 20.04.6 LTS 通过测试)*

### Open Harmony / Harmony OS

确保系统版本不低于 6.0.2。

## 构建编译

### Windows / Mac OS / Linux

#### 本地操作

使用 [Git](https://git-scm.cn/) 克隆、[CMake](https://cmake.org/) 进行构建：

```bash
git clone https://github.com/flamence/spiration
cd ./spiration/
cmake . -DCMAKE_BUILD_TYPE=Release
cmake --build ./build
```

#### Github Actions

[Fork](https://github.com/flamence/spiration/fork) 项目，
依照提示创建分支。

在分支仓库的 [Actions](#) 中找到最后一个编译成功的工作流，
在 [Summary](#) 的 **Artifacts** 中找到适合的文件下载，
或是在分支仓库的主页面找到 [Release](#) 并创建一个新版本，
稍等片刻，即可下载相关二进制文件。

### Open Harmony / Harmony OS

通过 [DevEco Studio](https://developer.huawei.com/consumer/cn/deveco-studio/) 打开 [ohos](ohos/) 文件夹。

借助菜单栏 **构建(<u>B</u>)**>**编译 Hap(s)/APP(s)** 的选项来构建。

## 贡献支持

详见 [CONTRIBUTING.md](CONTRIBUTION.md)

## 协议许可

详见 [LICENSE](LICENSE)。
