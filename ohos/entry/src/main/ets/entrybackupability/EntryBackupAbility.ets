import { BackupExtensionAbility, BundleVersion } from '@kit.CoreFileKit';

export default class EntryBackupAbility extends BackupExtensionAbility {
  async onBackup() {
    await Promise.resolve();
  }

  async onRestore(_: BundleVersion) {
    await Promise.resolve();
  }
}