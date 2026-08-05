/**
 * @file Index.ets
 * @brief Spiration 主页面。
 * @author clk
 */

import { display, window } from '@kit.ArkUI';
import { resourceManager } from '@kit.LocalizationKit';
import { inputMethod } from '@kit.IMEKit';
import { inputDevice } from '@kit.InputKit';
import { pasteboard } from '@kit.BasicServicesKit';
import { abilityAccessCtrl, common, Permissions } from '@kit.AbilityKit';
import { fileIo, picker } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';
import nativeEntry from 'libentry.so';
import { GlobalContext, AbilityContext } from '../utils/GlobalContext';

@Entry
@Component
struct Index {
  private xcController: XComponentController = new XComponentController();
  @StorageLink('windowWidth') @Watch('onWindowSizeChange') winWidth: number = 0;
  @StorageLink('windowHeight') @Watch('onWindowSizeChange') winHeight: number = 0;
  // 修饰键状态跟踪
  private ctrlPressed: boolean = false;
  private shiftPressed: boolean = false;
  private altPressed: boolean = false;
  // 输入法控制器
  private inputMethodController: inputMethod.InputMethodController | null = null;
  private isInputMethodAttached: boolean = false;
  private inputListenersRegistered: boolean = false;
  private lastFocusTime: number = 0;
  // 软键盘空闲看门狗（带硬件键盘设备上，空闲自动断开以释放 Ctrl 快捷键）
  private imeWatchdogId: number = -1;
  private hasHwKeyboard: boolean = false;
  // 长按触摸 → 打开上下文菜单（Ctrl 快捷键被系统占用时的触屏替代方案）
  private longPressTimer: number = -1;
  private longPressTriggered: boolean = false;
  private longPressX: number = 0;
  private longPressY: number = 0;
  // 触摸活动时间戳：用于过滤触摸产生的合成鼠标事件
  private lastTouchTime: number = 0;
  // 键盘事件去重：onKeyPreIme 与 onKeyEvent 对同一按键会连续触发两次
  private lastKeyCode: number = -1;
  private lastKeyDown: boolean = false;
  private lastKeyTime: number = 0;

  aboutToAppear(): void {
    this.initDataDir();
    this.requestPermissions();
    this.detectHardwareKeyboard();
    this.registerWindowActions();
    this.setupClipboardGlobal();
    this.registerFilePicker();
    this.loadSystemInfo();
    this.initLangResources();
    this.initInputMethod();
  }

  // 检测是否存在硬件键盘（2in1 有物理键盘 → 用看门狗及时释放 IME）
  private async detectHardwareKeyboard(): Promise<void> {
    try {
      const deviceIds = await inputDevice.getDeviceList();
      for (const id of deviceIds) {
        try {
          const kt = inputDevice.getKeyboardTypeSync(id);
          if (kt === inputDevice.KeyboardType.ALPHABETIC_KEYBOARD ||
              kt === inputDevice.KeyboardType.DIGITAL_KEYBOARD) {
            this.hasHwKeyboard = true;
            console.info('Spiration: hardware keyboard detected');
            return;
          }
        } catch (e) {
        }
      }
    } catch (e) {
    }
    this.hasHwKeyboard = false;
  }

  // 看门狗：软键盘无输入 3 秒自动断开（仅带硬件键盘设备，避免干扰纯触屏设备）
  private resetImeWatchdog(): void {
    if (!this.hasHwKeyboard) {
      return;
    }
    if (this.imeWatchdogId !== -1) {
      clearTimeout(this.imeWatchdogId);
    }
    this.imeWatchdogId = setTimeout((): void => {
      this.imeWatchdogId = -1;
      console.info('Spiration: IME watchdog timeout, detaching');
      this.detachInputMethod();
    }, 3000);
  }

  /**
   * 请求运行时权限（剪贴板读取）。
   * READ_PASTEBOARD 若为 system_grant 则安装时已授予（本调用无副作用）；
   * 若为 user_grant 则会弹出授权对话框，未授权时 getDataSync 抛 201 错误。
   */
  private requestPermissions(): void {
    try {
      const atManager = abilityAccessCtrl.createAtManager();
      const ctx = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const perms: Array<Permissions> = ['ohos.permission.READ_PASTEBOARD'];
      atManager.requestPermissionsFromUser(ctx, perms).then((data) => {
        console.info('Spiration: READ_PASTEBOARD authResult = ' + (data.authResults && data.authResults[0]));
      }).catch((err: Error) => {
        console.error('Spiration: requestPermissions failed ' + JSON.stringify(err));
      });
    } catch (err) {
      console.error('Spiration: requestPermissions error ' + JSON.stringify(err));
    }
  }

  /**
   * 通过 Context API 向 C++ 侧注入沙箱数据目录（替代硬编码路径）。
   * 参数 1：应用级 filesDir（getApplicationContext().filesDir，扩展/应用数据）；
   * 参数 2：模块级 filesDir（UIAbilityContext.filesDir = base/haps/<module>/files，lang/logs）。
   * 必须在任何 platform::app_data_dir() 使用之前调用。
   */
  private initDataDir(): void {
    try {
      const ctx = GlobalContext.getInstance().getValue('abilityContext') as AbilityContext;
      nativeEntry.spiration.platform.setDataDir(ctx.getApplicationContext().filesDir, ctx.filesDir);
    } catch (err) {
    }
  }

  /**
   * 注册文件选择器回调：C++ 侧 io::open_file_async / save_file_async 触发，
   * 此处拉起系统文件选择窗口并完成文件读写，结果经 onFilePicked 回传 C++。
   * @param content 另存时的文件内容（打开时为空串）。
   */
  private registerFilePicker(): void {
    try {
      nativeEntry.registerFilePickerCallback((title: string, isSave: boolean, content: string) => {
        this.showFilePicker(title, isSave, content);
      });
    } catch (err) {
    }
  }

  /**
   * 使用 @ohos.file.picker 拉起系统文件选择窗口，并处理文件读写：
   * 打开时读取所选文件内容，另存时把 content 写入所选位置。
   * 选择器返回的 URI 须经 @ohos.file.fs（fileIo）读写，不能直接用路径 API。
   */
  private async showFilePicker(title: string, isSave: boolean, content: string): Promise<void> {
    try {
      const ctx = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const docPicker = new picker.DocumentViewPicker(ctx);
      if (isSave) {
        const opt = new picker.DocumentSaveOptions();
        const uris: Array<string> = await docPicker.save(opt);
        if (uris.length > 0 && content.length > 0) {
          const file = fileIo.openSync(uris[0], fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.CREATE | fileIo.OpenMode.TRUNC);
          fileIo.writeSync(file.fd, content);
          fileIo.closeSync(file);
          nativeEntry.onFilePicked(uris[0], '');
        } else {
          nativeEntry.onFilePicked(uris.length > 0 ? uris[0] : '', '');
        }
      } else {
        const opt = new picker.DocumentSelectOptions();
        const uris: Array<string> = await docPicker.select(opt);
        if (uris.length > 0) {
          const file = fileIo.openSync(uris[0], fileIo.OpenMode.READ_ONLY);
          const stat = fileIo.statSync(file.fd);
          const buf = new ArrayBuffer(stat.size);
          fileIo.readSync(file.fd, buf);
          fileIo.closeSync(file);
          const text = util.TextDecoder.create('utf-8').decodeToString(new Uint8Array(buf));
          nativeEntry.onFilePicked(uris[0], text);
        } else {
          nativeEntry.onFilePicked('', '');
        }
      }
    } catch (err) {
      nativeEntry.onFilePicked('', '');
    }
  }

  private registerWindowActions(): void {
    try {
      const ctx = GlobalContext.getInstance().getValue('abilityContext') as AbilityContext;
      const win = GlobalContext.getInstance().getValue('mainWindow') as window.Window;
      let isMaximized: boolean = false;

      nativeEntry.registerCloseCallback(() => {
        ctx.terminateSelf();
      });

      nativeEntry.registerMaximizeCallback(() => {
        if (isMaximized) {
          win.recover().catch(() => {  });
          isMaximized = false;
        } else {
          win.maximize().catch(() => {  });
          isMaximized = true;
        }
        this.onWindowSizeChange();
      });

      nativeEntry.registerMinimizeCallback(() => {
        win.minimize().catch((_: Error) => {
        });
      });

      nativeEntry.registerStartMoveCallback(() => {
        win.startMoving().catch((_: Error) => {
        });
      });
    } catch (err) {
    }
  }

  private initLangResources(): void {
    try {
      const ctx = GlobalContext.getInstance().getValue('abilityContext') as AbilityContext;
      const resMgr: resourceManager.ResourceManager = ctx.resourceManager;
      nativeEntry.initResourceManager(resMgr);
    } catch (err) {
    }
  }

  /**
   * 窗口大小变化时通知 C++ 侧更新渲染器和 widget 树。
   */
  onWindowSizeChange(): void {
    if (this.winWidth > 0 && this.winHeight > 0) {
      try {
        const density = this.getDensity();
        nativeEntry.onWindowResize(this.winWidth, this.winHeight, density);
      } catch (err) {
      }
    }
  }

  private loadSystemInfo(): void {
    try {
    } catch (err) {
    }
  }

  /**
   * 获取 DPI 缩放因子。
   */
  private getDensity(): number {
    try {
      const defaultDisplay = display.getDefaultDisplaySync();
      return defaultDisplay.densityPixels;
    } catch (err) {
      return 1.0;
    }
  }

  /**
   * 将点击坐标传递给 C++ widget 树。
   */
  private handleTouch(vpX: number, vpY: number, action: number): void {
    try {
      nativeEntry.onTouchEvent(vpX, vpY, action);
    } catch (err) {
    }
  }

  /**
   * 处理硬件键盘事件（onKeyPreIme + onKeyEvent 双通道 + 去重）。
   * onKeyPreIme 在 IME 处理前触发，保证即使 IME 消费了按键也能收到；
   * 随后 onKeyEvent 再触发时在 40ms 内按同键同状态去重，避免重复转发。
   * @returns false（不消费事件）
   */
  private processKeyEvent(event: KeyEvent): boolean {
    const rawKeyCode = event.keyCode;
    const isDown = event.type === KeyType.Down;
    const now = Date.now();
    // 同一按键的 pre-IME 与 post-IME 事件在极短时间内连续触发 → 去重
    if (this.lastKeyCode === rawKeyCode && this.lastKeyDown === isDown && now - this.lastKeyTime < 40) {
      return false;
    }
    this.lastKeyCode = rawKeyCode;
    this.lastKeyDown = isDown;
    this.lastKeyTime = now;

    // 硬件键盘输入时自动断开 IME：attach 的输入连接会吞掉 Ctrl 组合键
    // （Ctrl+C/V/X/A 被当作虚拟文本框的系统剪贴板快捷键，到不了本应用）。
    // 软键盘输入（insertText）不经此路径，不受影响；触摸可重新唤起。
    if (isDown && this.isInputMethodAttached) {
      this.detachInputMethod();
    }

    // 修饰键状态：优先从事件直接查询 getModifierKeyState（支持 ctrl/alt/shift），
    // 比追踪 Ctrl 键事件更可靠（某些系统不单独派发修饰键事件）。
    let ctrl: boolean = false;
    let shift: boolean = false;
    let alt: boolean = false;
    let queried: boolean = false;
    try {
      if (event.getModifierKeyState) {
        ctrl = event.getModifierKeyState(['ctrl']);
        shift = event.getModifierKeyState(['shift']);
        alt = event.getModifierKeyState(['alt']);
        queried = true;
      }
    } catch (e) {
    }
    if (!queried) {
      // 回退：按键事件追踪
      ctrl = this.ctrlPressed;
      shift = this.shiftPressed;
      alt = this.altPressed;
    }
    // 保持按键事件追踪（供回退路径使用，维持状态一致）
    if (rawKeyCode === 2072 || rawKeyCode === 2073) {
      this.ctrlPressed = isDown;
    } else if (rawKeyCode === 2047 || rawKeyCode === 2048) {
      this.shiftPressed = isDown;
    } else if (rawKeyCode === 2045 || rawKeyCode === 2046) {
      this.altPressed = isDown;
    }

    try {
      // HarmonyOS KeyCode → Windows VK（C++ 控件按 VK 处理特殊键/快捷键）
      const keyCode = this.mapKeyCode(rawKeyCode);

      // 文本输入：优先 event.unicode，回退到 keyText
      const codepoint = (event.unicode !== undefined && event.unicode > 0)
        ? event.unicode
        : (event.keyText ? event.keyText.charCodeAt(0) : 0);

      // 诊断日志：确认按键与修饰键状态（hilog 过滤 Spiration）
      console.info('Spiration: key code=' + rawKeyCode + ' vk=' + keyCode + ' cp=' + codepoint +
        ' ctrl=' + (ctrl ? 1 : 0) + ' shift=' + (shift ? 1 : 0) + ' alt=' + (alt ? 1 : 0) + ' down=' + (isDown ? 1 : 0));

      nativeEntry.onKeyEvent(
        keyCode,
        codepoint,
        ctrl,
        shift,
        alt,
        isDown
      );
    } catch (err) {
    }
    return false;
  }

  // 长按 500ms → 模拟右键按下/抬起打开 C++ 上下文菜单（复制/粘贴/剪切/全选等）
  private startLongPress(x: number, y: number): void {
    this.cancelLongPress();
    this.longPressX = x;
    this.longPressY = y;
    this.longPressTriggered = false;
    this.longPressTimer = setTimeout((): void => {
      this.longPressTimer = -1;
      this.longPressTriggered = true;
      try {
        nativeEntry.onMouseEvent(this.longPressX, this.longPressY, 0, 2); // 右键按下
        nativeEntry.onMouseEvent(this.longPressX, this.longPressY, 1, 2); // 右键抬起
      } catch (e) {
      }
    }, 500);
  }

  private cancelLongPress(): void {
    if (this.longPressTimer !== -1) {
      clearTimeout(this.longPressTimer);
      this.longPressTimer = -1;
    }
  }

  /**
   * HarmonyOS KeyCode → Windows VK 码。
   * C++ 控件（text_area/text_field 等）按 Windows VK 处理特殊键与快捷键，
   * 可打印字符经 codepoint 兜底，无需逐键映射。
   */
  private mapKeyCode(keyCode: number): number {
    switch (keyCode) {
      case 2012: return 38;  // DPAD_UP → VK_UP
      case 2013: return 40;  // DPAD_DOWN → VK_DOWN
      case 2014: return 37;  // DPAD_LEFT → VK_LEFT
      case 2015: return 39;  // DPAD_RIGHT → VK_RIGHT
      case 2016: return 13;  // DPAD_CENTER → VK_RETURN
      case 2049: return 9;   // TAB → VK_TAB
      case 2050: return 32;  // SPACE → VK_SPACE
      case 2054: return 13;  // ENTER → VK_RETURN
      case 2055: return 8;   // DEL(退格) → VK_BACK
      case 2070: return 27;  // ESCAPE → VK_ESCAPE
      case 2071: return 46;  // FORWARD_DEL → VK_DELETE
      case 2081: return 36;  // MOVE_HOME → VK_HOME
      case 2082: return 35;  // MOVE_END → VK_END
    }
    // 字母 A(2017)~Z(2042) → VK_A(65)~VK_Z(90)
    if (keyCode >= 2017 && keyCode <= 2042) return keyCode - 2017 + 65;
    // 数字 0(2000)~9(2009) → VK_0(48)~VK_9(57)
    if (keyCode >= 2000 && keyCode <= 2009) return keyCode - 2000 + 48;
    return keyCode;
  }

  /**
   * 确保 ArkUI 焦点保持在可接收键盘事件的组件上（优先 Column，回退 XComponent）。
   * requestFocus 失败会抛 BusinessError（150001 不可聚焦/150002 祖先不可聚焦/150003 不在树上）。
   */
  private keepFocus(): void {
    const fc = this.getUIContext().getFocusController();
    try {
      fc.requestFocus('main_column');
      return;
    } catch (err) {
    }
    try {
      fc.requestFocus('xcomponent_surface');
    } catch (err) {
      console.error('Spiration: keepFocus failed ' + JSON.stringify(err));
    }
  }

  private lastTick: number = 0;
  private timerId: number = -1;

  aboutToDisappear(): void {
    clearInterval(this.timerId);
    this.detachInputMethod();
  }

  // 初始化输入法：仅获取控制器。文本/删除监听须在 attach 成功后注册
  // （client 未 attach 时 on('insertText') 抛 "input method client detached"）。
  private initInputMethod(): void {
    try {
      this.inputMethodController = inputMethod.getController();
    } catch (err) {
      console.error('Spiration: getController failed ' + JSON.stringify(err));
    }
  }

  // attach 成功后注册文本插入/删除监听（每次先 off 避免重复注册）
  private registerInputListeners(): void {
    if (!this.inputMethodController) {
      return;
    }
    const controller = this.inputMethodController;
    try {
      controller.off('insertText');
      controller.off('deleteLeft');
      controller.on('insertText', (text: string) => {
        // 软键盘有输入 → 看门狗续期（保持 IME 连接）
        this.resetImeWatchdog();
        for (let i = 0; i < text.length; i++) {
          const charCode = text.charCodeAt(i);
          nativeEntry.onKeyEvent(0, charCode, this.ctrlPressed, this.shiftPressed, this.altPressed, true);
        }
      });
      controller.on('deleteLeft', (length: number) => {
        for (let i = 0; i < length; i++) {
          // Backspace keyCode = 8
          nativeEntry.onKeyEvent(8, 0, this.ctrlPressed, this.shiftPressed, this.altPressed, true);
        }
      });
      this.inputListenersRegistered = true;
      console.info('Spiration: input listeners registered');
    } catch (err) {
      console.error('Spiration: register input listeners failed ' + JSON.stringify(err));
    }
  }

  // 拉起输入法（软键盘）。attach 失败时回退到 showTextInput，并输出日志便于排查。
  // 注：showSoftKeyboard/hideSoftKeyboard 仅限系统应用（需 CONNECT_IME_ABILITY），普通应用不可用。
  private attachInputMethod(): void {
    if (!this.inputMethodController) {
      this.initInputMethod();
    }
    if (!this.inputMethodController || this.isInputMethodAttached) {
      return;
    }
    const controller = this.inputMethodController;
    try {
      const textConfig: inputMethod.TextConfig = {
        inputAttribute: {
          textInputType: inputMethod.TextInputType.TEXT,
          enterKeyType: inputMethod.EnterKeyType.DONE
        }
      };
      controller.attach(true, textConfig).then(() => {
        this.isInputMethodAttached = true;
        console.info('Spiration: IME attached');
        this.registerInputListeners();
        this.resetImeWatchdog();
      }).catch((err: Error) => {
        console.error('Spiration: IME attach failed ' + JSON.stringify(err));
        // 回退：直接请求显示软键盘
        controller.showTextInput().then(() => {
          this.isInputMethodAttached = true;
          console.info('Spiration: soft keyboard shown');
          this.registerInputListeners();
          this.resetImeWatchdog();
        }).catch((e: Error) => {
          console.error('Spiration: showTextInput failed ' + JSON.stringify(e));
        });
      });
    } catch (err) {
      console.error('Spiration: attachInputMethod error ' + JSON.stringify(err));
    }
  }

  // 断开输入法（隐藏软键盘并释放输入连接）
  private detachInputMethod(): void {
    if (this.imeWatchdogId !== -1) {
      clearTimeout(this.imeWatchdogId);
      this.imeWatchdogId = -1;
    }
    if (this.inputMethodController && this.isInputMethodAttached) {
      const controller = this.inputMethodController;
      this.isInputMethodAttached = false;
      try {
        controller.detach().then(() => {
          console.info('Spiration: IME detached');
        }).catch((err: Error) => {
          console.error('Spiration: detach failed ' + JSON.stringify(err));
        });
      } catch (err) {
        console.error('Spiration: detach error ' + JSON.stringify(err));
      }
    }
  }

  // 剪贴板辅助函数（同步版：C++ 桥接同步调用，须返回字符串）
  copyToClipboard(text: string): void {
    try {
      const pasteData = pasteboard.createData(pasteboard.MIMETYPE_TEXT_PLAIN, text);
      pasteboard.getSystemPasteboard().setDataSync(pasteData);
    } catch (err) {
      console.error('Failed to copy to clipboard:', err);
    }
  }

  pasteFromClipboard(): string {
    try {
      const pasteData = pasteboard.getSystemPasteboard().getDataSync();
      return pasteData.getPrimaryText() || '';
    } catch (err) {
      console.error('Failed to paste from clipboard:', err);
      return '';
    }
  }

  /**
   * 注册剪贴板回调：C++ 侧（ohos_clipboard.cpp）同步调用
   * copyToClipboard / pasteFromClipboard，须返回字符串。
   */
  private setupClipboardGlobal(): void {
    try {
      nativeEntry.registerClipboardCallback(
        (text: string): void => {
          this.copyToClipboard(text);
        },
        (): string => {
          return this.pasteFromClipboard();
        }
      );
    } catch (err) {
    }
  }

  build() {
    Stack() {
      XComponent({
        type: XComponentType.SURFACE,
        controller: this.xcController
      })
        .id('xcomponent_surface')
        // 原生表面也持有焦点并接收键盘事件（与 Column 双通道 + 内部去重）
        .focusable(true)
        .onKeyPreIme((event: KeyEvent): boolean => {
          return this.processKeyEvent(event);
        })
        .onKeyEvent((event: KeyEvent): boolean => {
          return this.processKeyEvent(event);
        })
        .onLoad(() => {
          try {
            // 1. 获取 surface ID 并初始化原生渲染器
            const surfaceId = this.xcController.getXComponentSurfaceId();
            const density = this.getDensity();
            nativeEntry.initNativeWindow(surfaceId, density);

            // 2. 通知窗口大小
            this.onWindowSizeChange();

            // 3. 启动帧定时器
            this.lastTick = Date.now();
            this.timerId = setInterval((): void => {
              const now = Date.now();
              const dt = now - this.lastTick;
              this.lastTick = now;
              try {
                nativeEntry.onFrameTick(dt);
              } catch (e) {  }
              // 周期性确保 Column 持有 ArkUI 焦点（2in1 焦点可能漂移）
              if (now - this.lastFocusTime > 500) {
                this.lastFocusTime = now;
                this.keepFocus();
              }
            }, 16);
          } catch (err) {
          }
        })
        .hitTestBehavior(HitTestMode.None)
        .width('100%')
        .height('100%')

      Column()
        .id('main_column')
        .width('100%')
        .height('100%')
        .hitTestBehavior(HitTestMode.Block)
        .focusable(true)
        .defaultFocus(true)
        // 双通道接收硬件键盘：pre-IME（IME 消费前）与 post-IME，内部去重
        .onKeyPreIme((event: KeyEvent): boolean => {
          return this.processKeyEvent(event);
        })
        .onKeyEvent((event: KeyEvent): boolean => {
          return this.processKeyEvent(event);
        })
        .onTouch((event: TouchEvent) => {
          if (event.touches.length > 0) {
            const touch = event.touches[0];
            // 记录触摸活动时间，用于过滤随后由触摸合成的鼠标事件
            this.lastTouchTime = Date.now();
            // 枚举值: TouchType Down=0, Up=1, Move=2, Cancel=3
            // C++ 期望: 0=down, 1=up, 2=move
            if (event.type === TouchType.Down) {
              // 长按 → 打开上下文菜单
              this.startLongPress(touch.x, touch.y);
              this.handleTouch(touch.x, touch.y, 0);
              // 保持 ArkUI 焦点在 Column 上，保证键盘事件持续触发
              this.keepFocus();
              // 在触摸按下时拉起输入法
              this.attachInputMethod();
            } else if (event.type === TouchType.Up) {
              if (this.longPressTriggered) {
                // 长按已打开上下文菜单：跳过本次抬起，避免误触菜单
                this.cancelLongPress();
                return;
              }
              this.cancelLongPress();
              this.handleTouch(touch.x, touch.y, 1);
            } else if (event.type === TouchType.Move) {
              // 手指移动超过阈值（8px）→ 取消长按（拖拽/滚动场景）
              if (this.longPressTimer !== -1) {
                const dx = touch.x - this.longPressX;
                const dy = touch.y - this.longPressY;
                if (dx * dx + dy * dy > 64) {
                  this.cancelLongPress();
                }
              }
              this.handleTouch(touch.x, touch.y, 2);
            } else if (event.type === TouchType.Cancel) {
              this.cancelLongPress();
            }
          }
        })
        .onMouse((event: MouseEvent) => {
          // 移动事件始终转发（实时更新鼠标位置/悬停状态），不受触摸守卫影响
          if (event.action === MouseAction.Move) {
            nativeEntry.onMouseEvent(event.x, event.y, 2, 0); // move，无按键
            return;
          }
          // Press/Release：触摸后 300ms 内的合成鼠标事件跳过，避免与 onTouch 重复（"点两次"）
          if (Date.now() - this.lastTouchTime < 300) {
            return;
          }
          // 枚举值: MouseAction Press=0, Release=1 → C++: 0=press, 1=release
          if (event.button === MouseButton.Right) {
            if (event.action === MouseAction.Press) this.keepFocus();
            nativeEntry.onMouseEvent(event.x, event.y, event.action === MouseAction.Press ? 0 : 1, 2); // 右键
          } else if (event.button === MouseButton.Left) {
            if (event.action === MouseAction.Press) this.keepFocus();
            nativeEntry.onMouseEvent(event.x, event.y, event.action === MouseAction.Press ? 0 : 1, 0); // 左键
          }
        })
    }
    // 外层 Stack 也可聚焦，避免子组件因祖先不可聚焦而 requestFocus 失败（150002）
    .focusable(true)
    .width('100%')
    .height('100%')
  }
}
