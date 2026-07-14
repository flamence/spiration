/**
 * @file Index.ets
 * @brief Spiration 主页面。
 * @author clk
 */

import { hilog } from '@kit.PerformanceAnalysisKit';
import { display, window } from '@kit.ArkUI';
import { resourceManager } from '@kit.LocalizationKit';
import spirationLib from 'libentry.so';
import { GlobalContext, AbilityContext } from '../utils/GlobalContext';

const DOMAIN = 0x0000;
const TAG = 'Spiration';

@Entry
@Component
struct Index {
  private xcController: XComponentController = new XComponentController();
  @StorageLink('windowWidth') @Watch('onWindowSizeChange') winWidth: number = 0;
  @StorageLink('windowHeight') winHeight: number = 0;

  aboutToAppear(): void {
    this.registerWindowActions();
    this.loadSystemInfo();
    this.initLangResources();
  }

  private registerWindowActions(): void {
    try {
      const ctx = GlobalContext.getInstance().getValue('abilityContext') as AbilityContext;
      const win = GlobalContext.getInstance().getValue('mainWindow') as window.Window;
      let isMaximized: boolean = false;

      spirationLib.registerCloseCallback(() => {
        ctx.terminateSelf();
      });
      hilog.info(DOMAIN, TAG, 'Close callback registered');

      spirationLib.registerMaximizeCallback(() => {
        if (isMaximized) {
          win.recover();
          isMaximized = false;
        } else {
          win.maximize();
          isMaximized = true;
        }
        /* 最大化/恢复后强制同步渲染尺寸（windowSizeChange 可能不触发） */
        this.onWindowSizeChange();
      });
      hilog.info(DOMAIN, TAG, 'Maximize/restore callback registered');

      spirationLib.registerMinimizeCallback(() => {
        win.minimize();
      });
      hilog.info(DOMAIN, TAG, 'Minimize callback registered');

      spirationLib.registerStartMoveCallback(() => {
        win.startMoving();
      });
      hilog.info(DOMAIN, TAG, 'StartMove callback registered');
    } catch (err) {
      hilog.error(DOMAIN, TAG, 'Register window actions failed: %{public}s', JSON.stringify(err));
    }
  }

  private initLangResources(): void {
    try {
      const ctx = GlobalContext.getInstance().getValue('abilityContext') as AbilityContext;
      const resMgr: resourceManager.ResourceManager = ctx.resourceManager;
      spirationLib.initResourceManager(resMgr);
      hilog.info(DOMAIN, TAG, 'ResourceManager initialized');
    } catch (err) {
      hilog.error(DOMAIN, TAG, 'ResourceManager init failed: %{public}s', JSON.stringify(err));
    }
  }

  /**
   * 窗口大小变化时通知 C++ 侧更新渲染器和 widget 树。
   */
  onWindowSizeChange(): void {
    if (this.winWidth > 0 && this.winHeight > 0) {
      try {
        const density = this.getDensity();
        spirationLib.onWindowResize(this.winWidth, this.winHeight, density);
      } catch (err) {
        hilog.error(DOMAIN, TAG, 'Resize error: %{public}s', JSON.stringify(err));
      }
    }
  }

  private loadSystemInfo(): void {
    try {
      const platform = spirationLib.spiration.platform;
      const sysLocale = platform.getSystemLocale();
      const osName = platform.getOsName();
      const arch = platform.getArchitecture();
      hilog.info(DOMAIN, TAG, 'OS: %{public}s, locale: %{public}s', osName + ' | ' + arch, sysLocale);
    } catch (err) {
      hilog.error(DOMAIN, TAG, 'Failed to load system info: %{public}s', JSON.stringify(err));
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
      spirationLib.onTouchEvent(vpX, vpY, action);
      hilog.info(DOMAIN, TAG, 'Touch: (%.0f, %.0f) action=%{public}d',
        vpX, vpY, action);
    } catch (err) {
      hilog.error(DOMAIN, TAG, 'Touch error: %{public}s', JSON.stringify(err));
    }
  }

  private lastTick: number = 0;
  private timerId: number = -1;

  aboutToDisappear(): void {
    clearInterval(this.timerId);
  }

  build() {
    Stack() {
      // OpenGL ES 原生渲染表面
      XComponent({
        id: 'spiration_surface',
        type: 'surface',
        libraryname: 'entry',
        controller: this.xcController
      })
        .onLoad(() => {
          this.onWindowSizeChange();
          try {
            const surfaceId: string = this.xcController.getXComponentSurfaceId();
            const idNum: number = Number(surfaceId);
            const density: number = this.getDensity();
            const result: boolean = spirationLib.initNativeWindow(idNum, density);
            hilog.info(DOMAIN, TAG, 'Native init result=%{public}s, id=%{public}s, density=%.1f',
              JSON.stringify(result), surfaceId, density);

            // 启动帧循环（~60fps）
            this.lastTick = Date.now();
            this.timerId = setInterval((): void => {
              const now = Date.now();
              const dt = now - this.lastTick;
              this.lastTick = now;
              try {
                spirationLib.onFrameTick(dt);
              } catch (e) {
                // 忽略
              }
            }, 16);
          } catch (err) {
            hilog.error(DOMAIN, TAG, 'Native init failed: %{public}s', JSON.stringify(err));
          }
        })
        .hitTestBehavior(HitTestMode.None)
        .width('100%')
        .height('100%')

      // 透明输入捕获层（onTouch 提供实时 down/move/up）
      Column()
        .width('100%')
        .height('100%')
        .hitTestBehavior(HitTestMode.Block)
        .onTouch((event: TouchEvent) => {
          if (event.touches.length > 0) {
            const touch = event.touches[0];
            /* TouchType: Down=0, Move=1, Up=2 → C++ action: Down=0, Up=1, Move=2 */
            let cppAction: number = event.type;
            if (event.type === 1) cppAction = 2;  // Move
            else if (event.type === 2) cppAction = 1;  // Up
            this.handleTouch(touch.x, touch.y, cppAction);
          }
        })
    }
    .width('100%')
    .height('100%')
  }
}
