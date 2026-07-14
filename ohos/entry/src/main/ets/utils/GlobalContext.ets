/**
 * @file GlobalContext.ets
 * @brief ArkTS 全局上下文单例，替代被禁用的 globalThis。
 * @author clk
 */

import { resourceManager } from '@kit.LocalizationKit';

/** AbilityContext 接口（仅声明所需成员） */
export interface AbilityContext {
  terminateSelf(): void;
  resourceManager: resourceManager.ResourceManager;
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
