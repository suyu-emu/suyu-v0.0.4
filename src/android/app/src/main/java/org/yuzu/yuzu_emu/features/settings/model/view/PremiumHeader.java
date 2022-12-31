package org.yuzu.yuzu_emu.features.settings.model.view;

public final class PremiumHeader extends SettingsItem {
    public PremiumHeader() {
        super(null, null, null, 0, 0);
    }

    @Override
    public int getType() {
        return SettingsItem.TYPE_PREMIUM;
    }
}
