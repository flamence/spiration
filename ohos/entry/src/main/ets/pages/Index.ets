/**
 * @file Index.ets
 * @brief Spiration 主页面。
 * @author clk
 */

import { hilog } from '@kit.PerformanceAnalysisKit';
import { display, window } from '@kit.ArkUI';
import { resourceManager } from '@kit.LocalizationKit';
import nativeEntry from 'libentry.so';
import { GlobalContext, AbilityContext } from '../utils/GlobalContext';

const DOMAIN = 0x0000;
const TAG = 'spiration';

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

      nativeEntry.registerCloseCallback(() => {
        ctx.terminateSelf();
      });
      hilog.info(DOMAIN, TAG, 'Close callback registered');

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
      hilog.info(DOMAIN, TAG, 'Maximize/restore callback registered');

      nativeEntry.registerMinimizeCallback(() => {
        win.minimize().catch((err: Error) => {
          hilog.error(DOMAIN, TAG, 'Minimize failed: %{public}s', err.message);
        });
      });
      hilog.info(DOMAIN, TAG, 'Minimize callback registered');

      nativeEntry.registerStartMoveCallback(() => {
        win.startMoving().catch((err: Error) => {
          hilog.error(DOMAIN, TAG, 'StartMoving failed: %{public}s', err.message);
        });
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
      nativeEntry.initResourceManager(resMgr);
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
        nativeEntry.onWindowResize(this.winWidth, this.winHeight, density);
      } catch (err) {
        hilog.error(DOMAIN, TAG, 'Resize error: %{public}s', JSON.stringify(err));
      }
    }
  }

  private loadSystemInfo(): void {
    try {
      const platform = nativeEntry.spiration.platform;
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
      nativeEntry.onTouchEvent(vpX, vpY, action);
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
      XComponent({
        type: XComponentType.SURFACE,
        controller: this.xcController
      })
        .onLoad(() => {
          this.onWindowSizeChange();
          try {
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
            hilog.error(DOMAIN, TAG, 'Native init failed: %{public}s', JSON.stringify(err));
          }
        })
        .hitTestBehavior(HitTestMode.None)
        .width('100%')
        .height('100%')

      Column()
        .width('100%')
        .height('100%')
        .hitTestBehavior(HitTestMode.Block)
        .onTouch((event: TouchEvent) => {
          if (event.touches.length > 0) {
            const touch = event.touches[0];
            let cppAction: number = event.type;
            if (event.type === 1) cppAction = 2;
            else if (event.type === 2) cppAction = 1;
            this.handleTouch(touch.x, touch.y, cppAction);
          }
        })
    }
    .width('100%')
    .height('100%')
  }
}
