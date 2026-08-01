/**
 * @file EntryAbility.ets
 * @brief Spiration 入口能力。
 * @author clk
 */

import { AbilityConstant, Configuration, UIAbility, Want } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { GlobalContext } from '../utils/GlobalContext';

export default class EntryAbility extends UIAbility {
  private win: window.Window | null = null;

  onCreate(_want: Want, _launchParam: AbilityConstant.LaunchParam): void {
  }

  onDestroy(): void {
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {

    try {
      GlobalContext.getInstance().setValue('abilityContext', this.context);
      this.win = windowStage.getMainWindowSync();
      GlobalContext.getInstance().setValue('mainWindow', this.win);

      this.win.on('windowSizeChange', (data: window.Size) => {
        AppStorage.setOrCreate<number>('windowWidth', data.width);
        AppStorage.setOrCreate<number>('windowHeight', data.height);
      });
    } catch (err) {
    }

    windowStage.loadContent('pages/Index', (err) => {
      if (err.code) {
        return;
      }

      if (this.win) {
        try {
          this.win.setWindowDecorVisible(false);
          this.win.setTitleAndDockHoverShown(false).catch(() => {  });
          this.win.setWindowTitleMoveEnabled(false);
          this.win.setWindowTitleButtonVisible(false, false, false);
        } catch (e) {
        }
      }
    });
  }

  onWindowStageDestroy(): void {
    if (this.win) {
      try {
        this.win.off('windowSizeChange');
      } catch (e) {
        // 窗口可能已销毁，忽略错误
      }
    }
  }

  onForeground(): void {
  }

  onBackground(): void {
  }

  onConfigurationUpdate(_: Configuration): void {
  }
}