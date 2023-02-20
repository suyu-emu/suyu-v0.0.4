package org.yuzu.yuzu_emu.features.settings.ui;

import android.text.TextUtils;

import org.yuzu.yuzu_emu.R;
import org.yuzu.yuzu_emu.features.settings.model.Setting;
import org.yuzu.yuzu_emu.features.settings.model.SettingSection;
import org.yuzu.yuzu_emu.features.settings.model.Settings;
import org.yuzu.yuzu_emu.features.settings.model.StringSetting;
import org.yuzu.yuzu_emu.features.settings.model.view.CheckBoxSetting;
import org.yuzu.yuzu_emu.features.settings.model.view.DateTimeSetting;
import org.yuzu.yuzu_emu.features.settings.model.view.HeaderSetting;
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem;
import org.yuzu.yuzu_emu.features.settings.model.view.SingleChoiceSetting;
import org.yuzu.yuzu_emu.features.settings.model.view.SliderSetting;
import org.yuzu.yuzu_emu.features.settings.model.view.SubmenuSetting;
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile;

import java.util.ArrayList;

public final class SettingsFragmentPresenter {
    private SettingsFragmentView mView;

    private String mMenuTag;
    private String mGameID;

    private Settings mSettings;
    private ArrayList<SettingsItem> mSettingsList;

    public SettingsFragmentPresenter(SettingsFragmentView view) {
        mView = view;
    }

    public void onCreate(String menuTag, String gameId) {
        mGameID = gameId;
        mMenuTag = menuTag;
    }

    public void onViewCreated(Settings settings) {
        setSettings(settings);
    }

    /**
     * If the screen is rotated, the Activity will forget the settings map. This fragment
     * won't, though; so rather than have the Activity reload from disk, have the fragment pass
     * the settings map back to the Activity.
     */
    public void onAttach() {
        if (mSettings != null) {
            mView.passSettingsToActivity(mSettings);
        }
    }

    public void putSetting(Setting setting) {
        mSettings.getSection(setting.getSection()).putSetting(setting);
    }

    private StringSetting asStringSetting(Setting setting) {
        if (setting == null) {
            return null;
        }

        StringSetting stringSetting = new StringSetting(setting.getKey(), setting.getSection(), setting.getValueAsString());
        putSetting(stringSetting);
        return stringSetting;
    }

    public void loadDefaultSettings() {
        loadSettingsList();
    }

    public void setSettings(Settings settings) {
        if (mSettingsList == null && settings != null) {
            mSettings = settings;

            loadSettingsList();
        } else {
            mView.getActivity().setTitle(R.string.preferences_settings);
            mView.showSettingsList(mSettingsList);
        }
    }

    private void loadSettingsList() {
        if (!TextUtils.isEmpty(mGameID)) {
            mView.getActivity().setTitle("Game Settings: " + mGameID);
        }
        ArrayList<SettingsItem> sl = new ArrayList<>();

        if (mMenuTag == null) {
            return;
        }

        switch (mMenuTag) {
            case SettingsFile.FILE_NAME_CONFIG:
                addConfigSettings(sl);
                break;
            case Settings.SECTION_GENERAL:
                addGeneralSettings(sl);
                break;
            case Settings.SECTION_SYSTEM:
                addSystemSettings(sl);
                break;
            case Settings.SECTION_RENDERER:
                addGraphicsSettings(sl);
                break;
            case Settings.SECTION_AUDIO:
                addAudioSettings(sl);
                break;
            default:
                mView.showToastMessage("Unimplemented menu", false);
                return;
        }

        mSettingsList = sl;
        mView.showSettingsList(mSettingsList);
    }

    private void addConfigSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_settings);

        sl.add(new SubmenuSetting(null, null, R.string.preferences_general, 0, Settings.SECTION_GENERAL));
        sl.add(new SubmenuSetting(null, null, R.string.preferences_system, 0, Settings.SECTION_SYSTEM));
        sl.add(new SubmenuSetting(null, null, R.string.preferences_graphics, 0, Settings.SECTION_RENDERER));
        sl.add(new SubmenuSetting(null, null, R.string.preferences_audio, 0, Settings.SECTION_AUDIO));
    }

    private void addGeneralSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_general);

        SettingSection rendererSection = mSettings.getSection(Settings.SECTION_RENDERER);
        Setting frameLimitEnable = rendererSection.getSetting(SettingsFile.KEY_RENDERER_USE_SPEED_LIMIT);
        Setting frameLimitValue = rendererSection.getSetting(SettingsFile.KEY_RENDERER_SPEED_LIMIT);

        sl.add(new CheckBoxSetting(SettingsFile.KEY_RENDERER_USE_SPEED_LIMIT, Settings.SECTION_RENDERER, R.string.frame_limit_enable, R.string.frame_limit_enable_description, true, frameLimitEnable));
        sl.add(new SliderSetting(SettingsFile.KEY_RENDERER_SPEED_LIMIT, Settings.SECTION_RENDERER, R.string.frame_limit_slider, R.string.frame_limit_slider_description, 1, 200, "%", 100, frameLimitValue));

        SettingSection cpuSection = mSettings.getSection(Settings.SECTION_CPU);
        Setting cpuAccuracy = cpuSection.getSetting(SettingsFile.KEY_CPU_ACCURACY);
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_CPU_ACCURACY, Settings.SECTION_CPU, R.string.cpu_accuracy, 0, R.array.cpuAccuracyNames, R.array.cpuAccuracyValues, 0, cpuAccuracy));
    }

    private void addSystemSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_system);

        SettingSection systemSection = mSettings.getSection(Settings.SECTION_SYSTEM);
        Setting dockedMode = systemSection.getSetting(SettingsFile.KEY_USE_DOCKED_MODE);
        Setting region = systemSection.getSetting(SettingsFile.KEY_REGION_INDEX);
        Setting language = systemSection.getSetting(SettingsFile.KEY_LANGUAGE_INDEX);

        sl.add(new CheckBoxSetting(SettingsFile.KEY_USE_DOCKED_MODE, Settings.SECTION_SYSTEM, R.string.use_docked_mode, R.string.use_docked_mode_description, true, dockedMode));
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_REGION_INDEX, Settings.SECTION_SYSTEM, R.string.emulated_region, 0, R.array.regionNames, R.array.regionValues, -1, region));
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_LANGUAGE_INDEX, Settings.SECTION_SYSTEM, R.string.emulated_language, 0, R.array.languageNames, R.array.languageValues, 1, language));
    }

    private void addGraphicsSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_graphics);

        SettingSection rendererSection = mSettings.getSection(Settings.SECTION_RENDERER);
        Setting rendererBackend = rendererSection.getSetting(SettingsFile.KEY_RENDERER_BACKEND);
        Setting rendererAccuracy = rendererSection.getSetting(SettingsFile.KEY_RENDERER_ACCURACY);
        Setting rendererReolution = rendererSection.getSetting(SettingsFile.KEY_RENDERER_RESOLUTION);
        Setting rendererAspectRation = rendererSection.getSetting(SettingsFile.KEY_RENDERER_ASPECT_RATIO);
        Setting rendererForceMaxClocks = rendererSection.getSetting(SettingsFile.KEY_RENDERER_FORCE_MAX_CLOCK);
        Setting rendererAsynchronousShaders = rendererSection.getSetting(SettingsFile.KEY_RENDERER_ASYNCHRONOUS_SHADERS);
        Setting rendererDebug = rendererSection.getSetting(SettingsFile.KEY_RENDERER_DEBUG);

        sl.add(new SingleChoiceSetting(SettingsFile.KEY_RENDERER_BACKEND, Settings.SECTION_RENDERER, R.string.renderer_api, 0, R.array.rendererApiNames, R.array.rendererApiValues, 1, rendererBackend));
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_RENDERER_ACCURACY, Settings.SECTION_RENDERER, R.string.renderer_accuracy, 0, R.array.rendererAccuracyNames, R.array.rendererAccuracyValues, 1, rendererAccuracy));
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_RENDERER_RESOLUTION, Settings.SECTION_RENDERER, R.string.renderer_resolution, 0, R.array.rendererResolutionNames, R.array.rendererResolutionValues, 2, rendererReolution));
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_RENDERER_ASPECT_RATIO, Settings.SECTION_RENDERER, R.string.renderer_aspect_ratio, 0, R.array.rendererAspectRatioNames, R.array.rendererAspectRatioValues, 0, rendererAspectRation));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_RENDERER_FORCE_MAX_CLOCK, Settings.SECTION_RENDERER, R.string.renderer_force_max_clock, R.string.renderer_force_max_clock_description, true, rendererForceMaxClocks));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_RENDERER_ASYNCHRONOUS_SHADERS, Settings.SECTION_RENDERER, R.string.renderer_asynchronous_shaders, R.string.renderer_asynchronous_shaders_description, false, rendererAsynchronousShaders));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_RENDERER_DEBUG, Settings.SECTION_RENDERER, R.string.renderer_debug, R.string.renderer_debug_description, false, rendererDebug));
    }

    private void addAudioSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_audio);

        SettingSection audioSection = mSettings.getSection(Settings.SECTION_AUDIO);
        Setting audioVolume = audioSection.getSetting(SettingsFile.KEY_AUDIO_VOLUME);

        sl.add(new SliderSetting(SettingsFile.KEY_AUDIO_VOLUME, Settings.SECTION_AUDIO, R.string.audio_volume, R.string.audio_volume_description, 0, 100, "%", 100, audioVolume));
    }
}
