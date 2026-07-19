# Spiration

## 快速开始

前往 [Release](https://github.com/flamence/spiration/releases) 页面下载最新稳定的版本，

或是在 [Actions](https://github.com/flamence/spiration/actions) 中找到最近一次成功的构建。

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

### Harmony OS

通过 [DevEco Studio](https://developer.huawei.com/consumer/cn/deveco-studio/) 打开 [ohos](ohos/) 文件夹。

借助菜单栏 **Build**>**Build Hap(s)/APP(s)** 的选项来构建。

> [Command Line Tools](https://developer.huawei.com/consumer/cn/download/command-line-tools-for-hmos) 需要登录后才能下载，因而 [Actions](https://github.com/flamence/spiration/actions) 不会提供 `.hap` 文件的下载。
> 目前仅通过 [Release](https://github.com/flamence/spiration/releases) 发布。
> [CoolCLK/build-hap](#) 由于构建需要 Git LFS、Cache 而被放弃。

## 拓展功能

|名称|说明|仓库|
|---|---|---|
|`edit`|提供编辑文件的功能。|[flamence/spiration-edit](https://github/flamence/spiration-edit)|

## 贡献支持

### 代码风格

- 简洁、明了，逻辑清晰，强可维护性，比如对可分离的功能进行模块化处理。
- 需注明文档注释，例如文件、类、方法、变量等均可以进行适当标注。
- 不允许滥用行注释。
- 确保代码缩进正确、格式鲜明。
- 在编写通用代码库时，强化多平台思维，谨记使用预处理指令。

### 提交修改

> 贡献者应该 [Fork](https://github.com/flamence/spiration/fork) 项目后，
> 在本地使用 [Git](https://git-scm.cn/) 签出到分支（如 `feature/...`、`fix/...`），
> 并将修改提交到该分支上。
> 此后，在 [Pull requests](https://github.com/flamence/spiration/pulls) 页面点击 [New pull request](https://github.com/flamence/spiration/compare)，选择自己的分支并等待审查，通过后即可并入修改。
