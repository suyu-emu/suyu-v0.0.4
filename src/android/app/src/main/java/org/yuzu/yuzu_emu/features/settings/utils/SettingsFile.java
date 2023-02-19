package org.yuzu.yuzu_emu.features.settings.utils;

import androidx.annotation.NonNull;

import org.yuzu.yuzu_emu.YuzuApplication;
import org.yuzu.yuzu_emu.NativeLibrary;
import org.yuzu.yuzu_emu.R;
import org.yuzu.yuzu_emu.features.settings.model.FloatSetting;
import org.yuzu.yuzu_emu.features.settings.model.IntSetting;
import org.yuzu.yuzu_emu.features.settings.model.Setting;
import org.yuzu.yuzu_emu.features.settings.model.SettingSection;
import org.yuzu.yuzu_emu.features.settings.model.Settings;
import org.yuzu.yuzu_emu.features.settings.model.StringSetting;
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivityView;
import org.yuzu.yuzu_emu.utils.BiMap;
import org.yuzu.yuzu_emu.utils.DirectoryInitialization;
import org.yuzu.yuzu_emu.utils.Log;
import org.ini4j.Wini;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
public final class SettingsFile {
    public static final String FILE_NAME_CONFIG = "config";

    public static final String KEY_DESIGN = "design";

    // CPU
    public static final String KEY_CPU_ACCURACY = "cpu_accuracy";
    // System
    public static final String KEY_USE_DOCKED_MODE = "use_docked_mode";
    public static final String KEY_REGION_INDEX = "region_index";
    public static final String KEY_LANGUAGE_INDEX = "language_index";
    public static final String KEY_RENDERER_BACKEND = "backend";
    // Renderer
    public static final String KEY_RENDERER_RESOLUTION = "resolution_setup";
    public static final String KEY_RENDERER_ASPECT_RATIO = "aspect_ratio";
    public static final String KEY_RENDERER_ACCURACY = "gpu_accuracy";
    public static final String KEY_RENDERER_ASYNCHRONOUS_SHADERS = "use_asynchronous_shaders";
    public static final String KEY_RENDERER_FORCE_MAX_CLOCK = "force_max_clock";
    public static final String KEY_RENDERER_USE_SPEED_LIMIT = "use_speed_limit";
    public static final String KEY_RENDERER_SPEED_LIMIT = "speed_limit";
    // Audio
    public static final String KEY_AUDIO_VOLUME = "volume";

    private static BiMap<String, String> sectionsMap = new BiMap<>();

    static {
        //TODO: Add members to sectionsMap when game-specific settings are added
    }


    private SettingsFile() {
    }

    /**
     * Reads a given .ini file from disk and returns it as a HashMap of Settings, themselves
     * effectively a HashMap of key/value settings. If unsuccessful, outputs an error telling why it
     * failed.
     *
     * @param ini          The ini file to load the settings from
     * @param isCustomGame
     * @param view         The current view.
     * @return An Observable that emits a HashMap of the file's contents, then completes.
     */
    static HashMap<String, SettingSection> readFile(final File ini, boolean isCustomGame, SettingsActivityView view) {
        HashMap<String, SettingSection> sections = new Settings.SettingsSectionMap();

        BufferedReader reader = null;

        try {
            reader = new BufferedReader(new FileReader(ini));

            SettingSection current = null;
            for (String line; (line = reader.readLine()) != null; ) {
                if (line.startsWith("[") && line.endsWith("]")) {
                    current = sectionFromLine(line, isCustomGame);
                    sections.put(current.getName(), current);
                } else if ((current != null)) {
                    Setting setting = settingFromLine(current, line);
                    if (setting != null) {
                        current.putSetting(setting);
                    }
                }
            }
        } catch (FileNotFoundException e) {
            Log.error("[SettingsFile] File not found: " + e.getMessage());
            if (view != null)
                view.onSettingsFileNotFound();
        } catch (IOException e) {
            Log.error("[SettingsFile] Error reading from: " + e.getMessage());
            if (view != null)
                view.onSettingsFileNotFound();
        } finally {
            if (reader != null) {
                try {
                    reader.close();
                } catch (IOException e) {
                    Log.error("[SettingsFile] Error closing: " +  e.getMessage());
                }
            }
        }

        return sections;
    }

    public static HashMap<String, SettingSection> readFile(final String fileName, SettingsActivityView view) {
        return readFile(getSettingsFile(fileName), false, view);
    }

    /**
     * Reads a given .ini file from disk and returns it as a HashMap of SettingSections, themselves
     * effectively a HashMap of key/value settings. If unsuccessful, outputs an error telling why it
     * failed.
     *
     * @param gameId the id of the game to load it's settings.
     * @param view   The current view.
     */
    public static HashMap<String, SettingSection> readCustomGameSettings(final String gameId, SettingsActivityView view) {
        return readFile(getCustomGameSettingsFile(gameId), true, view);
    }

    /**
     * Saves a Settings HashMap to a given .ini file on disk. If unsuccessful, outputs an error
     * telling why it failed.
     *
     * @param fileName The target filename without a path or extension.
     * @param sections The HashMap containing the Settings we want to serialize.
     * @param view     The current view.
     */
    public static void saveFile(final String fileName, TreeMap<String, SettingSection> sections,
                                SettingsActivityView view) {
        File ini = getSettingsFile(fileName);

        try {
            Wini writer = new Wini(ini);

            Set<String> keySet = sections.keySet();
            for (String key : keySet) {
                SettingSection section = sections.get(key);
                writeSection(writer, section);
            }
            writer.store();
        } catch (IOException e) {
            Log.error("[SettingsFile] File not found: " + fileName + ".ini: " + e.getMessage());
            view.showToastMessage(YuzuApplication.getAppContext().getString(R.string.error_saving, fileName, e.getMessage()), false);
        }
    }


    public static void saveCustomGameSettings(final String gameId, final HashMap<String, SettingSection> sections) {
        Set<String> sortedSections = new TreeSet<>(sections.keySet());

        for (String sectionKey : sortedSections) {
            SettingSection section = sections.get(sectionKey);

            HashMap<String, Setting> settings = section.getSettings();
            Set<String> sortedKeySet = new TreeSet<>(settings.keySet());

            for (String settingKey : sortedKeySet) {
                Setting setting = settings.get(settingKey);
                NativeLibrary.SetUserSetting(gameId, mapSectionNameFromIni(section.getName()), setting.getKey(), setting.getValueAsString());
            }
        }
    }

    private static String mapSectionNameFromIni(String generalSectionName) {
        if (sectionsMap.getForward(generalSectionName) != null) {
            return sectionsMap.getForward(generalSectionName);
        }

        return generalSectionName;
    }

    private static String mapSectionNameToIni(String generalSectionName) {
        if (sectionsMap.getBackward(generalSectionName) != null) {
            return sectionsMap.getBackward(generalSectionName);
        }

        return generalSectionName;
    }

    @NonNull
    private static File getSettingsFile(String fileName) {
        return new File(
                DirectoryInitialization.getUserDirectory() + "/config/" + fileName + ".ini");
    }

    private static File getCustomGameSettingsFile(String gameId) {
        return new File(DirectoryInitialization.getUserDirectory() + "/GameSettings/" + gameId + ".ini");
    }

    private static SettingSection sectionFromLine(String line, boolean isCustomGame) {
        String sectionName = line.substring(1, line.length() - 1);
        if (isCustomGame) {
            sectionName = mapSectionNameToIni(sectionName);
        }
        return new SettingSection(sectionName);
    }

    /**
     * For a line of text, determines what type of data is being represented, and returns
     * a Setting object containing this data.
     *
     * @param current The section currently being parsed by the consuming method.
     * @param line    The line of text being parsed.
     * @return A typed Setting containing the key/value contained in the line.
     */
    private static Setting settingFromLine(SettingSection current, String line) {
        String[] splitLine = line.split("=");

        if (splitLine.length != 2) {
            Log.warning("Skipping invalid config line \"" + line + "\"");
            return null;
        }

        String key = splitLine[0].trim();
        String value = splitLine[1].trim();

        if (value.isEmpty()) {
            Log.warning("Skipping null value in config line \"" + line + "\"");
            return null;
        }

        try {
            int valueAsInt = Integer.parseInt(value);

            return new IntSetting(key, current.getName(), valueAsInt);
        } catch (NumberFormatException ex) {
        }

        try {
            float valueAsFloat = Float.parseFloat(value);

            return new FloatSetting(key, current.getName(), valueAsFloat);
        } catch (NumberFormatException ex) {
        }

        return new StringSetting(key, current.getName(), value);
    }

    /**
     * Writes the contents of a Section HashMap to disk.
     *
     * @param parser  A Wini pointed at a file on disk.
     * @param section A section containing settings to be written to the file.
     */
    private static void writeSection(Wini parser, SettingSection section) {
        // Write the section header.
        String header = section.getName();

        // Write this section's values.
        HashMap<String, Setting> settings = section.getSettings();
        Set<String> keySet = settings.keySet();

        for (String key : keySet) {
            Setting setting = settings.get(key);
            parser.put(header, setting.getKey(), setting.getValueAsString());
        }
    }
}
