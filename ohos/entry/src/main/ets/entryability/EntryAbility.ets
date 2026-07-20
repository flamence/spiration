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
const TAG = 'spiration';

export default class EntryAbility extends UIAbility {
  private win: window.Window | null = null;

  onCreate(_want: Want, _launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(DOMAIN, TAG, 'Spiration onCreate');
  }

  onDestroy(): void {
    hilog.info(DOMAIN, TAG, 'Spiration onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    hilog.info(DOMAIN, TAG, 'onWindowStageCreate');

    try {
      GlobalContext.getInstance().setValue('abilityContext', this.context);
      this.win = windowStage.getMainWindowSync();
      GlobalContext.getInstance().setValue('mainWindow', this.win);

      this.win.on('windowSizeChange', (data: window.Size) => {
        AppStorage.setOrCreate<number>('windowWidth', data.width);
        AppStorage.setOrCreate<number>('windowHeight', data.height);
        hilog.info(DOMAIN, TAG, 'Window resized: %{public}dx%{public}d', data.width, data.height);
      });
    } catch (err) {
      hilog.error(DOMAIN, TAG, 'Failed to configure window: %{public}s', JSON.stringify(err));
    }

    windowStage.loadContent('pages/Index', (err) => {
      if (err.code) {
        hilog.error(DOMAIN, TAG, 'Failed to load content: %{public}s', JSON.stringify(err));
        return;
      }
      hilog.info(DOMAIN, TAG, 'Succeeded in loading content');

      if (this.win) {
        try {
          this.win.setWindowDecorVisible(false);
          this.win.setTitleAndDockHoverShown(false).catch(() => {  });
          this.win.setWindowTitleMoveEnabled(false);
          this.win.setWindowTitleButtonVisible(false, false, false);
          hilog.info(DOMAIN, TAG, 'All window decorations hidden');
        } catch (e) {
          hilog.warn(DOMAIN, TAG, 'Hide decoration error: %{public}s', JSON.stringify(e));
        }
      }
    });
  }

  onWindowStageDestroy(): void {
    if (this.win) {
      this.win.off('windowSizeChange');
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