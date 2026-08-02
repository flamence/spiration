/**
 * @file Index.ets
 * @brief Spiration 主页面。
 * @author clk
 */

import { display, window } from '@kit.ArkUI';
import { resourceManager } from '@kit.LocalizationKit';
import { inputMethod } from '@kit.IMEKit';
import { pasteboard } from '@kit.BasicServicesKit';
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

  aboutToAppear(): void {
    this.initDataDir();
    this.registerWindowActions();
    this.loadSystemInfo();
    this.initLangResources();
    this.initInputMethod();
  }

  /**
   * 通过 Context API 向 C++ 侧注入沙箱数据目录（替代硬编码路径）。
   * 必须在任何 platform::app_data_dir() 使用之前调用。
   */
  private initDataDir(): void {
    try {
      const ctx = GlobalContext.getInstance().getValue('abilityContext') as AbilityContext;
      nativeEntry.spiration.platform.setDataDir(ctx.filesDir);
    } catch (err) {
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

  private lastTick: number = 0;
  private timerId: number = -1;

  aboutToDisappear(): void {
    clearInterval(this.timerId);
    this.detachInputMethod();
  }

  // 初始化输入法
  private initInputMethod(): void {
    try {
      this.inputMethodController = inputMethod.getController();
      
      // 监听文本插入事件
      this.inputMethodController.on('insertText', (text: string) => {
        for (let i = 0; i < text.length; i++) {
          const charCode = text.charCodeAt(i);
          nativeEntry.onKeyEvent(0, charCode, this.ctrlPressed, this.shiftPressed, this.altPressed, true);
        }
      });

      // 监听删除事件
      this.inputMethodController.on('deleteLeft', (length: number) => {
        for (let i = 0; i < length; i++) {
          // Backspace keyCode = 8
          nativeEntry.onKeyEvent(8, 0, this.ctrlPressed, this.shiftPressed, this.altPressed, true);
        }
      });
    } catch (err) {
      console.error('Failed to initialize input method:', err);
    }
  }

  // 拉起输入法
  private attachInputMethod(): void {
    if (this.inputMethodController && !this.isInputMethodAttached) {
      try {
        const textConfig: inputMethod.TextConfig = {
          inputAttribute: {
            textInputType: inputMethod.TextInputType.TEXT,
            enterKeyType: inputMethod.EnterKeyType.DONE
          }
        };
        this.inputMethodController.attach(true, textConfig);
        this.isInputMethodAttached = true;
      } catch (err) {
        console.error('Failed to attach input method:', err);
      }
    }
  }

  // 隐藏输入法
  private detachInputMethod(): void {
    if (this.inputMethodController && this.isInputMethodAttached) {
      try {
        this.inputMethodController.detach();
        this.isInputMethodAttached = false;
      } catch (err) {
        console.error('Failed to detach input method:', err);
      }
    }
  }

  // 剪贴板辅助函数
  async copyToClipboard(text: string): Promise<void> {
    try {
      const pasteData = pasteboard.createPlainTextData(text);
      const systemPasteboard = pasteboard.getSystemPasteboard();
      await systemPasteboard.setData(pasteData);
    } catch (err) {
      console.error('Failed to copy to clipboard:', err);
    }
  }

  async pasteFromClipboard(): Promise<string> {
    try {
      const systemPasteboard = pasteboard.getSystemPasteboard();
      const pasteData = await systemPasteboard.getData();
      return pasteData.getPrimaryText() || '';
    } catch (err) {
      console.error('Failed to paste from clipboard:', err);
      return '';
    }
  }

  build() {
    Stack() {
      XComponent({
        type: XComponentType.SURFACE,
        controller: this.xcController
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
            }, 16);
          } catch (err) {
          }
        })
        .hitTestBehavior(HitTestMode.None)
        .width('100%')
        .height('100%')

      Column()
        .width('100%')
        .height('100%')
        .hitTestBehavior(HitTestMode.Block)
        .focusable(true)
        .defaultFocus(true)
        .onKeyEvent((event: KeyEvent) => {
          try {
            const keyCode = event.keyCode;
            const isDown = event.type === KeyType.Down;
            
            // 更新修饰键状态
            // HarmonyOS KeyCode: Ctrl=2072/2073, Shift=2019/2020, Alt=2054/2055
            if (keyCode === 2072 || keyCode === 2073) {
              this.ctrlPressed = isDown;
            } else if (keyCode === 2019 || keyCode === 2020) {
              this.shiftPressed = isDown;
            } else if (keyCode === 2054 || keyCode === 2055) {
              this.altPressed = isDown;
            }
            
            // 文本输入：使用 Unicode 码点
            const codepoint = event.keyText ? event.keyText.charCodeAt(0) : 0;
            
            nativeEntry.onKeyEvent(
              keyCode,
              codepoint,
              this.ctrlPressed,
              this.shiftPressed,
              this.altPressed,
              isDown
            );
          } catch (err) {
          }
        })
        .onTouch((event: TouchEvent) => {
          if (event.touches.length > 0) {
            const touch = event.touches[0];
            let cppAction: number = event.type;
            if (event.type === 1) cppAction = 2;
            else if (event.type === 2) cppAction = 1;
            this.handleTouch(touch.x, touch.y, cppAction);
            
            // 在触摸按下时拉起输入法
            if (event.type === TouchType.Down) {
              this.attachInputMethod();
            }
          }
        })
        .onMouse((event: MouseEvent) => {
          // 处理鼠标右键事件
          if (event.button === MouseButton.Right) {
            const action = event.action === MouseAction.Press ? 0 : 1;  // 0=press, 1=release
            nativeEntry.onMouseEvent(event.x, event.y, action, 2);  // button=2 表示右键
          }
          // 处理鼠标左键拖动
          if (event.button === MouseButton.Left || event.button === MouseButton.None) {
            // MouseAction: Press=1, Release=2, Move=3
            // 转换为 C++ 期望的格式: 0=press, 1=release, 2=move
            let cppAction: number = 2; // 默认 move
            if (event.action === MouseAction.Press) cppAction = 0;
            else if (event.action === MouseAction.Release) cppAction = 1;
            nativeEntry.onMouseEvent(event.x, event.y, cppAction, 0); // button=0 表示左键
          }
        })
    }
    .width('100%')
    .height('100%')
  }
}
