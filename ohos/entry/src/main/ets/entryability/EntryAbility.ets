/**
 * @file EntryAbility.ets
 * @brief Spiration 入口能力。
 * @author clk
 */

import { AbilityConstant, Configuration, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { GlobalContext } from '../utils/GlobalContext';

const DOMAIN = 0x0000;
const TAG = 'Spiration';

export default class EntryAbility extends UIAbility {
  private win: window.Window | null = null;

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(DOMAIN, TAG, 'Spiration OHOS onCreate');
  }

  onDestroy(): void {
    hilog.info(DOMAIN, TAG, 'Spiration OHOS onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    hilog.info(DOMAIN, TAG, 'onWindowStageCreate');

    try {
      GlobalContext.getInstance().setValue('abilityContext', this.context);
      this.win = windowStage.getMainWindowSync();
      GlobalContext.getInstance().setValue('mainWindow', this.win);

      // 注册窗口大小变化监听
      this.win.on('windowSizeChange', (data: window.Size) => {
        AppStorage.setOrCreate<number>('windowWidth', data.width);
        AppStorage.setOrCreate<number>('windowHeight', data.height);
        hilog.info(DOMAIN, TAG, 'Window resized: %{public}dx%{public}d', data.width, data.height);
      });
    } catch (err) {
      hilog.error(DOMAIN, TAG, 'Failed to configure window: %{public}s', JSON.stringify(err));
    }

    // 加载主页面
    windowStage.loadContent('pages/Index', (err) => {
      if (err.code) {
        hilog.error(DOMAIN, TAG, 'Failed to load content: %{public}s', JSON.stringify(err));
        return;
      }
      hilog.info(DOMAIN, TAG, 'Succeeded in loading content');

      // UI 内容加载完成后，隐藏所有系统窗口装饰（由 C++ appbar 接管）
      if (this.win) {
        try {
          this.win.setWindowDecorVisible(false);           // 隐藏标题栏
          this.win.setTitleAndDockHoverShown(false);       // 禁用悬浮标题栏
          this.win.setWindowTitleMoveEnabled(false);       // 禁用标题栏拖拽移动
          this.win.setWindowTitleButtonVisible(false, false, false); // 隐藏三键
          hilog.info(DOMAIN, TAG, 'All window decorations hidden');
        } catch (e) {
          hilog.warn(DOMAIN, TAG, 'Hide decoration error: %{public}s', JSON.stringify(e));
        }
      }
    });
  }

  onWindowStageDestroy(): void {
    // 移除窗口监听
    if (this.win) {
      try {
        this.win.off('windowSizeChange');
      } catch (err) {
        // ignore
      }
    }
    hilog.info(DOMAIN, TAG, 'onWindowStageDestroy');
  }

  onForeground(): void {
    hilog.info(DOMAIN, TAG, 'onForeground');
  }

  onBackground(): void {
    hilog.info(DOMAIN, TAG, 'onBackground');
  }

  onConfigurationUpdate(config: Configuration): void {
    hilog.info(DOMAIN, TAG, 'onConfigurationUpdate: %{public}s', JSON.stringify(config));
  }
}