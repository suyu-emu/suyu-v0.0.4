package org.yuzu.yuzu_emu.features.settings.model.view;

import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import org.yuzu.yuzu_emu.YuzuApplication;
import org.yuzu.yuzu_emu.R;
import org.yuzu.yuzu_emu.features.settings.model.Setting;
import org.yuzu.yuzu_emu.features.settings.ui.SettingsFragmentView;

public final class PremiumSingleChoiceSetting extends SettingsItem {
    private int mDefaultValue;

    private int mChoicesId;
    private int mValuesId;
    private SettingsFragmentView mView;

    private static SharedPreferences mPreferences = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.getAppContext());

    public PremiumSingleChoiceSetting(String key, String section, int titleId, int descriptionId,
                                      int choicesId, int valuesId, int defaultValue, Setting setting, SettingsFragmentView view) {
        super(key, section, setting, titleId, descriptionId);
        mValuesId = valuesId;
        mChoicesId = choicesId;
        mDefaultValue = defaultValue;
        mView = view;
    }

    public int getChoicesId() {
        return mChoicesId;
    }

    public int getValuesId() {
        return mValuesId;
    }

    public int getSelectedValue() {
        return mPreferences.getInt(getKey(), mDefaultValue);
    }

    /**
     * Write a value to the backing int. If that int was previously null,
     * initializes a new one and returns it, so it can be added to the Hashmap.
     *
     * @param selection New value of the int.
     * @return null if overwritten successfully otherwise; a newly created IntSetting.
     */
    public void setSelectedValue(int selection) {
        final SharedPreferences.Editor editor = mPreferences.edit();
        editor.putInt(getKey(), selection);
        editor.apply();
        mView.showToastMessage(YuzuApplication.getAppContext().getString(R.string.design_updated), false);
    }

    @Override
    public int getType() {
        return TYPE_SINGLE_CHOICE;
    }
}
