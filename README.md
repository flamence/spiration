# Spiration

## 简要介绍

Next Generation 的 IDE，
拥有具有极高性能、极高拓展性的特点。

## 快速开始

### Windows

```batch
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### MacOS

参考 Linux。

### HarmonyOS

通过 [DevEco Studio](https://developer.huawei.com/consumer/cn/deveco-studio/) 打开 [ohos](ohos/) 文件夹。

借助菜单栏 **Build**>**Build Hap(s)/APP(s)** 的选项来构建。

## 拓展

拓展是 Spiration 的核心。

## 贡献

底层、框架及安全由 [clk](https://github.com/CoolCLK) 贡献，
存在 AI 编写代码，但是已经通过测试验证（`macos` 平台除外）。