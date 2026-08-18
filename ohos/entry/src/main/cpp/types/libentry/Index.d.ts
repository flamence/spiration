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

/**
 * 注册文件选择器回调。C++ 侧 io::open_file_async / save_file_async 触发，
 * ArkTS 侧收到后拉起系统文件选择窗口并完成文件读写。
 * @param callback 回调（标题, 是否另存, 另存时的文件内容）
 */
export function registerFilePickerCallback(callback: (title: string, isSave: boolean, content: string) => void): void;

/**
 * 文件选择完成后将结果回传 C++ 侧。
 * @param uri     选择的文件 URI；取消传空字符串
 * @param content 打开时读取的文件内容（另存时为空串）
 */
export function onFilePicked(uri: string, content: string): void;

/**
 * 注册剪贴板回调（同步）。C++ 侧复制/粘贴时调用。
 * @param copy  复制回调（文本）
 * @param paste 粘贴回调（返回文本）
 */
export function registerClipboardCallback(copy: (text: string) => void, paste: () => string): void;

/**
 * 注册 HTTP 桥接回调。C++ 侧 net::ohos_http 发起请求时调用，
 * ArkTS 侧使用 @ohos.net.http（Remote Communication Kit）执行。
 * @param callback 回调（任务描述对象）
 */
export function registerHttpHandler(callback: (task: HttpTask) => void): void;

/**
 * 流式请求：数据块回传 C++。
 * @param id    任务 id
 * @param status HTTP 状态码（可能为 0）
 * @param chunk 数据块文本
 */
export function httpStreamData(id: number, status: number, chunk: string): void;

/**
 * 流式请求：完成/出错。
 * @param id     任务 id
 * @param status HTTP 状态码
 * @param error  错误信息（空串表示成功）
 */
export function httpStreamEnd(id: number, status: number, error: string): void;

/**
 * 非流式请求完成。
 * @param id     任务 id
 * @param status HTTP 状态码
 * @param body   响应体文本
 * @param error  错误信息（空串表示成功）
 */
export function httpRequestDone(id: number, status: number, body: string, error: string): void;

/**
 * HTTP 请求头字段（由 C++ NAPI 桥接组装）。
 */
export interface HttpHeader {
  name: string;
  value: string;
}

/**
 * HTTP 任务描述（由 C++ NAPI 桥接组装，字段与 ohos_http.cpp 保持一致）。
 */
export interface HttpTask {
  id: number;
  kind: string;
  method: string;
  url: string;
  headers: HttpHeader[];
  body: string;
  timeout: number;
  userAgent: string;
  stream: boolean;
}

export const spiration: {
  /** 系统信息与平台 API */
  readonly platform: {
    /** 通过 Context API 注入沙箱数据目录 */
    setDataDir(appDir: string, moduleDir: string): void;
  };

};
