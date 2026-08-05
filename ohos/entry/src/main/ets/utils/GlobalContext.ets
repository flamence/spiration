/**
 * @file GlobalContext.ets
 * @brief ArkTS 全局上下文单例。
 * @author clk
 */

import { resourceManager } from '@kit.LocalizationKit';

/**
 * 应用级上下文（getApplicationContext 返回）。
 * 其 filesDir 为应用级文件目录（无 haps/<module> 层级）。
 */
export interface ApplicationContext {
  filesDir: string;
}

export interface AbilityContext {
  terminateSelf(): void;
  resourceManager: resourceManager.ResourceManager;
  filesDir: string;
  getApplicationContext(): ApplicationContext;
}

export class GlobalContext {
  private constructor() {}
  private static instance: GlobalContext = new GlobalContext();
  private map_: Map<string, Object> = new Map();

  public static getInstance(): GlobalContext {
    return GlobalContext.instance;
  }

  public setValue(key: string, value: Object): void {
    this.map_.set(key, value);
  }

  public getValue(key: string): Object | undefined {
    return this.map_.get(key);
  }
}
