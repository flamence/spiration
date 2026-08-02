/**
 * 初始化原生渲染器。
 * @param surfaceId XComponent 的 surface ID (字符串格式)
 * @param density 屏幕像素密度
 */
export function initNativeWindow(surfaceId: string, density: number): boolean;

/**
 * 传递触摸事件到 C++ widget 树。
 * @param x 触摸 X 坐标
 * @param y 触摸 Y 坐标
 * @param action 触摸动作
 */
export function onTouchEvent(x: number, y: number, action: number): void;

/**
 * 传递鼠标事件到 C++ widget 树。
 * @param x 鼠标 X 坐标
 * @param y 鼠标 Y 坐标
 * @param action 动作（0=press, 1=release）
 * @param button 按钮（0=左键, 1=中键, 2=右键）
 */
export function onMouseEvent(x: number, y: number, action: number, button: number): void;

/**
 * 传递键盘事件到 C++ widget 树。
 * @param keyCode 键码（HarmonyOS KeyCode）
 * @param codepoint Unicode 码点（文本输入）
 * @param ctrl Ctrl 键是否按下
 * @param shift Shift 键是否按下
 * @param alt Alt 键是否按下
 * @param isDown 是否按下（true=down, false=up）
 */
export function onKeyEvent(
  keyCode: number,
  codepoint: number,
  ctrl: boolean,
  shift: boolean,
  alt: boolean,
  isDown: boolean
): void;

export function onWindowResize(width: number, height: number, density: number): void;

export function onFrameTick(dtMs: number): void;

export function initResourceManager(resMgr: object): void;

export function registerCloseCallback(callback: () => void): void;
export function registerMaximizeCallback(callback: () => void): void;
export function registerMinimizeCallback(callback: () => void): void;
export function registerStartMoveCallback(callback: () => void): void;

export const spiration: {
  /** 系统信息与平台 API */
  readonly platform: {
    /** 获取操作系统名称 */
    getOsName(): string;
    /** 获取操作系统版本号 */
    getOsVersion(): string;
    /** 获取 CPU 架构 */
    getArchitecture(): string;
    /** 获取系统语言代码 */
    getSystemLocale(): string;
    /** 获取应用数据目录 */
    getAppDataDir(): string;
    /** 获取可执行文件目录 */
    getExecutableDir(): string;
    /** 通过 Context API 注入应用沙箱数据目录 */
    setDataDir(dir: string): void;
  };

  /** 国际化翻译 API */
  readonly i18n: {
    /** 根据键获取翻译文本 */
    tr(key: string): string;
    /** 设置当前语言 */
    setLocale(locale: string): string;
    /** 获取当前语言 */
    getCurrentLocale(): string;
    /** 从文件加载翻译 */
    loadTranslation(locale: string, filepath: string): boolean;
  };

  /** 文件系统操作 API */
  readonly fs: {
    /** 检查文件或目录是否存在 */
    fileExists(path: string): boolean;
    /** 创建目录 */
    createDirectory(path: string): boolean;
    /** 列出目录内容 */
    listDirectory(path: string): string[];
    /** 拼接路径 */
    joinPath(a: string, b: string): string;
  };

  /** 扩展管理 API */
  readonly extension: {
    /** 从指定目录加载所有扩展 */
    loadFromDir(dir: string): number;
    /** 获取扩展搜索目录 */
    getExtensionDir(): string;
  };

};