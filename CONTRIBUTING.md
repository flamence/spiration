# 为 Spiration 贡献

## 代码风格

- 简洁、明了，逻辑清晰，强可维护性，比如对可分离的功能进行模块化处理。
- 需注明文档注释，以 Doxygen 规范为主，例如文件、类、方法、变量等均可以进行适当标注。
- 不允许滥用行注释。
- 确保代码缩进正确、格式鲜明。
- 在编写通用代码库时，强化多平台思维，谨记使用预处理指令。

## 提交修改

贡献者应该 [Fork](https://github.com/flamence/spiration/fork) 项目后，
在本地使用 [Git](https://git-scm.cn/) 签出到分支（如 `feature/...`、`fix/...`），
并将修改提交到该分支上。
此后，在 [Pull requests](https://github.com/flamence/spiration/pulls) 页面点击 [New pull request](https://github.com/flamence/spiration/compare)，选择自己的分支并等待审查，通过后即可并入修改。

> 建议在提交时，对版本号进行同步修改。

## 引入依赖

通常都是通过编辑 `CMakeLists.txt` 来引入新的依赖，
利用 [CMake](https://cmake.com.cn/) 中 [FetchContent](https://cmake.com.cn/cmake/help/latest/module/FetchContent.html) 的特性。

> 为了减少 [CMP0135](https://cmake.com.cn/cmake/help/latest/policy/CMP0135.html) 警告，使用了 `cmake_policy(SET CMP0135 NEW)`。

首先 `FetchContent_Declare` 依赖，确保：

- 目标依赖的 `URL` / `GIT_REPOSITORY` 是可信的。
- 确保通过 `GIT_TAG` 或 `URL` 指定的版本是可信的（避免供应链攻击），
**必须**指定依赖版本或提交哈希。

接着使用 `FetchContent_MakeAvailable` 来配置，
添加依赖于其的功能，
再拉取请求修改分支。

**注意:** 非必要不引入依赖。

## 人工智能政策

我们欢迎在编码中使用大语言模型，但我们对所有贡献都保持高标准，并且**我们期望有人类参与其中，真正理解 LLM 代表他们产出的工作**。因此，我们**不接受来自自主代理的贡献**。涉嫌违反此规定的拉取请求可能被关闭，有时恕不另行通知。

**在与维护者沟通时，请不要完全依赖 LLM 来替你撰写全部内容**。谨记是真实的人类在阅读这些内容，我们希望听到你的声音，而不是模型的。如果你是非中文母语者，使用 LLM 对发给维护者的消息进行彻底编辑或翻译，我们鼓励你**将机器翻译放在引用块中，并在其后附上你母语撰写的原始文本**。

如果你认为**分享与 LLM 对话中的上下文**有帮助或必要，请将**相关部分**放在引用块中（例如使用 `>`），**注明其为 AI 生成**，并附上你自己的评论，解释**为什么它相关以及你从中得到了什么**。

本政策改编自 [zed-industries/zed](https://github.com/zed-industries/zed/blob/main/CONTRIBUTING.md#ai-policy)。