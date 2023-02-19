package org.yuzu.yuzu_emu.utils;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class GpuDriverMetadata {

    public String name;
    public String description;
    public String author;
    public String vendor;
    public String driverVersion;
    public int minApi;
    public String libraryName;

    public GpuDriverMetadata(String metadataFilePath) {
        try {
            JSONObject json = new JSONObject(getStringFromFile(metadataFilePath));
            name = json.getString("name");
            description = json.getString("description");
            author = json.getString("author");
            vendor = json.getString("vendor");
            driverVersion = json.getString("driverVersion");
            minApi = json.getInt("minApi");
            libraryName = json.getString("libraryName");
        } catch (JSONException e) {
            // JSON is malformed, ignore and treat as unsupported metadata.
        } catch (IOException e) {
            // File is inaccessible, ignore and treat as unsupported metadata.
        }
    }

    private static String getStringFromFile(String filePath) throws IOException {
        Path path = Paths.get(filePath);
        byte[] bytes = Files.readAllBytes(path);
        return new String(bytes, StandardCharsets.UTF_8);
    }

}
