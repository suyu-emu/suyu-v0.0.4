package org.yuzu.yuzu_emu.utils;

import android.content.Context;
import android.net.Uri;

import org.yuzu.yuzu_emu.NativeLibrary;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public class GpuDriverHelper {
    private static final String META_JSON_FILENAME = "meta.json";
    private static final String DRIVER_INTERNAL_FILENAME = "gpu_driver.zip";
    private static String fileRedirectionPath;
    private static String driverInstallationPath;
    private static String hookLibPath;

    private static void unzip(String zipFilePath, String destDir) throws IOException {
        File dir = new File(destDir);

        // Create output directory if it doesn't exist
        if (!dir.exists()) dir.mkdirs();

        // Unpack the files.
        ZipInputStream zis = new ZipInputStream(new FileInputStream(zipFilePath));
        byte[] buffer = new byte[1024];
        ZipEntry ze = zis.getNextEntry();
        while (ze != null) {
            String fileName = ze.getName();
            File newFile = new File(destDir + fileName);
            newFile.getParentFile().mkdirs();
            FileOutputStream fos = new FileOutputStream(newFile);
            int len;
            while ((len = zis.read(buffer)) > 0) {
                fos.write(buffer, 0, len);
            }
            fos.close();
            zis.closeEntry();
            ze = zis.getNextEntry();
        }
        zis.closeEntry();
    }

    public static void initializeDriverParameters(Context context) {
        try {
            // Initialize the file redirection directory.
            fileRedirectionPath = context.getExternalFilesDir(null).getCanonicalPath() + "/gpu/vk_file_redirect/";

            // Initialize the driver installation directory.
            driverInstallationPath = context.getFilesDir().getCanonicalPath() + "/gpu_driver/";
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        // Initialize directories.
        initializeDirectories();

        // Initialize hook libraries directory.
        hookLibPath = context.getApplicationInfo().nativeLibraryDir + "/";

        // Initialize GPU driver.
        NativeLibrary.InitializeGpuDriver(hookLibPath, driverInstallationPath, getCustomDriverLibraryName(), fileRedirectionPath);
    }

    public static void installDefaultDriver(Context context) {
        // Removing the installed driver will result in the backend using the default system driver.
        File driverInstallationDir = new File(driverInstallationPath);
        deleteRecursive(driverInstallationDir);
        initializeDriverParameters(context);
    }

    public static void installCustomDriver(Context context, Uri driverPathUri) {
        // Revert to system default in the event the specified driver is bad.
        installDefaultDriver(context);

        // Ensure we have directories.
        initializeDirectories();

        // Copy the zip file URI into our private storage.
        FileUtil.copyUriToInternalStorage(context, driverPathUri, driverInstallationPath, DRIVER_INTERNAL_FILENAME);

        // Unzip the driver.
        try {
            unzip(driverInstallationPath + DRIVER_INTERNAL_FILENAME, driverInstallationPath);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        // Initialize the driver parameters.
        initializeDriverParameters(context);
    }

    public static String getCustomDriverName() {
        // Parse the custom driver metadata to retrieve the name.
        GpuDriverMetadata metadata = new GpuDriverMetadata(driverInstallationPath + META_JSON_FILENAME);
        return metadata.name;
    }

    private static String getCustomDriverLibraryName() {
        // Parse the custom driver metadata to retrieve the library name.
        GpuDriverMetadata metadata = new GpuDriverMetadata(driverInstallationPath + META_JSON_FILENAME);
        return metadata.libraryName;
    }

    private static void initializeDirectories() {
        // Ensure the file redirection directory exists.
        File fileRedirectionDir = new File(fileRedirectionPath);
        if (!fileRedirectionDir.exists()) {
            fileRedirectionDir.mkdirs();
        }
        // Ensure the driver installation directory exists.
        File driverInstallationDir = new File(driverInstallationPath);
        if (!driverInstallationDir.exists()) {
            driverInstallationDir.mkdirs();
        }
    }

    private static void deleteRecursive(File fileOrDirectory) {
        if (fileOrDirectory.isDirectory()) {
            for (File child : fileOrDirectory.listFiles()) {
                deleteRecursive(child);
            }
        }
        fileOrDirectory.delete();
    }
}
