# Spiration

[![](https://github.com/flamence/spiration/actions/workflows/cmake-multi-platform.yml/badge.svg?branch=gui)](https://github.com/flamence/spiration/actions/workflows/cmake-multi-platform.yml)

## Quick Start

Go to the [Release](https://github.com/flamence/spiration/releases) page to download the latest stable version,

or find the most recent successful build in [Actions](https://github.com/flamence/spiration/actions).

## Building

### Windows / Mac OS / Linux

#### Local Build

Use [Git](https://git-scm.cn/) to clone and [CMake](https://cmake.org/) to build:

```bash
git clone https://github.com/flamence/spiration
cd ./spiration/
cmake . -DCMAKE_BUILD_TYPE=Release
cmake --build ./build
```

#### Github Actions

[Fork](https://github.com/flamence/spiration/fork) the project,
and create a branch as prompted.

In your forked repository, find the last successful workflow run in [Actions](#),
go to the **Artifacts** section of the [Summary](#) to download the appropriate files,
or go to the main page of your fork, find [Release](#) and create a new release.
After a short wait, you can download the relevant binary files.

### Harmony OS

Open the [ohos](ohos/) folder with [DevEco Studio](https://developer.huawei.com/consumer/cn/deveco-studio/).

Use the menu option **Build** > **Build Hap(s)/APP(s)** to build.

> [Command Line Tools](https://developer.huawei.com/consumer/cn/download/command-line-tools-for-hmos) requires login to download, so [Actions](https://github.com/flamence/spiration/actions) will not provide `.hap` file downloads.
> Currently, they are only published through [Release](https://github.com/flamence/spiration/releases).
> [CoolCLK/build-hap](#) was abandoned because the build required Git LFS and Cache.

## Extensions

|Name|Description|Repository|
|---|---|---|
|`edit`|Provides file editing functionality.|[flamence/spiration-edit](https://github/flamence/spiration-edit)|

## Contributing

See [CONTRIBUTING.md](CONTRIBUTION.md)

## License

See [LICENSE](LICENSE).

A Simplified Chinese version [LICENSE.zh-CN](LICENSE.zh-CN) is also provided,
adapted from [【英译中】Apache-2.0](https://openatom.org/journalism/article/rYJ6HgQ5vxex),
translated solely for ease of reading, understanding, and discussion.