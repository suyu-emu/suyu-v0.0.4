// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

import android.annotation.SuppressLint
import kotlin.collections.setOf
import org.jetbrains.kotlin.konan.properties.Properties
import org.jlleitschuh.gradle.ktlint.reporter.ReporterType

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("kotlin-parcelize")
    kotlin("plugin.serialization") version "1.8.21"
    id("androidx.navigation.safeargs.kotlin")
    id("org.jlleitschuh.gradle.ktlint") version "11.4.0"
}

/**
 * Use the number of seconds/10 since Jan 1 2016 as the versionCode.
 * This lets us upload a new build at most every 10 seconds for the
 * next 680 years.
 */
val autoVersion = (((System.currentTimeMillis() / 1000) - 1451606400) / 10).toInt()

@Suppress("UnstableApiUsage")
android {
    namespace = "org.yuzu.yuzu_emu"

    compileSdkVersion = "android-34"
    ndkVersion = "25.2.9519653"

    buildFeatures {
        viewBinding = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    packaging {
        // This is necessary for libadrenotools custom driver loading
        jniLibs.useLegacyPackaging = true
    }

    defaultConfig {
        // TODO If this is ever modified, change application_id in strings.xml
        applicationId = "org.yuzu.yuzu_emu"
        minSdk = 30
        targetSdk = 34
        versionName = getGitVersion()

        // If you want to use autoVersion for the versionCode, create a property in local.properties
        // named "autoVersioned" and set it to "true"
        val properties = Properties()
        val versionProperty = try {
            properties.load(project.rootProject.file("local.properties").inputStream())
            properties.getProperty("autoVersioned") ?: ""
        } catch (e: Exception) { "" }

        versionCode = if (versionProperty == "true") {
            autoVersion
        } else {
            1
        }

        ndk {
            @SuppressLint("ChromeOsAbiSupport")
            abiFilters += listOf("arm64-v8a")
        }

        buildConfigField("String", "GIT_HASH", "\"${getGitHash()}\"")
        buildConfigField("String", "BRANCH", "\"${getBranch()}\"")
    }

    val keystoreFile = System.getenv("ANDROID_KEYSTORE_FILE")
    if (keystoreFile != null) {
        signingConfigs {
            create("release") {
                storeFile = file(keystoreFile)
                storePassword = System.getenv("ANDROID_KEYSTORE_PASS")
                keyAlias = System.getenv("ANDROID_KEY_ALIAS")
                keyPassword = System.getenv("ANDROID_KEYSTORE_PASS")
            }
        }
    }

    // Define build types, which are orthogonal to product flavors.
    buildTypes {

        // Signed by release key, allowing for upload to Play Store.
        release {
            signingConfig = if (keystoreFile != null) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("debug")
            }

            resValue("string", "app_name_suffixed", "yuzu")
            isMinifyEnabled = true
            isDebuggable = false
            proguardFiles(
                getDefaultProguardFile("proguard-android.txt"),
                "proguard-rules.pro"
            )
        }

        // builds a release build that doesn't need signing
        // Attaches 'debug' suffix to version and package name, allowing installation alongside the release build.
        register("relWithDebInfo") {
            isDefault = true
            resValue("string", "app_name_suffixed", "yuzu Debug Release")
            signingConfig = signingConfigs.getByName("debug")
            isMinifyEnabled = true
            isDebuggable = true
            proguardFiles(
                getDefaultProguardFile("proguard-android.txt"),
                "proguard-rules.pro"
            )
            versionNameSuffix = "-relWithDebInfo"
            applicationIdSuffix = ".relWithDebInfo"
            isJniDebuggable = true
        }

        // Signed by debug key disallowing distribution on Play Store.
        // Attaches 'debug' suffix to version and package name, allowing installation alongside the release build.
        debug {
            resValue("string", "app_name_suffixed", "yuzu Debug")
            isDebuggable = true
            isJniDebuggable = true
            versionNameSuffix = "-debug"
            applicationIdSuffix = ".debug"
        }
    }

    flavorDimensions.add("version")
    productFlavors {
        create("mainline") {
            isDefault = true
            dimension = "version"
            buildConfigField("Boolean", "PREMIUM", "false")
        }

        create("ea") {
            dimension = "version"
            buildConfigField("Boolean", "PREMIUM", "true")
            applicationIdSuffix = ".ea"
        }
    }

    externalNativeBuild {
        cmake {
            version = "3.22.1"
            path = file("../../../CMakeLists.txt")
        }
    }

    defaultConfig {
        externalNativeBuild {
            cmake {
                arguments(
                    "-DENABLE_QT=0", // Don't use QT
                    "-DENABLE_SDL2=0", // Don't use SDL
                    "-DENABLE_WEB_SERVICE=0", // Don't use telemetry
                    "-DBUNDLE_SPEEX=ON",
                    "-DANDROID_ARM_NEON=true", // cryptopp requires Neon to work
                    "-DYUZU_USE_BUNDLED_VCPKG=ON",
                    "-DYUZU_USE_BUNDLED_FFMPEG=ON",
                    "-DYUZU_ENABLE_LTO=ON"
                )

                abiFilters("arm64-v8a", "x86_64")
            }
        }
    }
}

tasks.create<Delete>("ktlintReset") {
    delete(File(buildDir.path + File.separator + "intermediates/ktLint"))
}

tasks.getByPath("loadKtlintReporters").dependsOn("ktlintReset")
tasks.getByPath("preBuild").dependsOn("ktlintCheck")

ktlint {
    version.set("0.47.1")
    android.set(true)
    ignoreFailures.set(false)
    disabledRules.set(
        setOf(
            "no-wildcard-imports",
            "package-name",
            "import-ordering"
        )
    )
    reporters {
        reporter(ReporterType.CHECKSTYLE)
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.10.1")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("androidx.recyclerview:recyclerview:1.3.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("androidx.fragment:fragment-ktx:1.6.0")
    implementation("androidx.documentfile:documentfile:1.0.1")
    implementation("com.google.android.material:material:1.9.0")
    implementation("androidx.preference:preference:1.2.0")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.6.1")
    implementation("io.coil-kt:coil:2.2.2")
    implementation("androidx.core:core-splashscreen:1.0.1")
    implementation("androidx.window:window:1.1.0")
    implementation("org.ini4j:ini4j:0.5.4")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("androidx.swiperefreshlayout:swiperefreshlayout:1.1.0")
    implementation("androidx.navigation:navigation-fragment-ktx:2.6.0")
    implementation("androidx.navigation:navigation-ui-ktx:2.6.0")
    implementation("info.debatty:java-string-similarity:2.0.0")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.5.0")
}

fun getGitVersion(): String {
    var versionName = "0.0"

    try {
        versionName = ProcessBuilder("git", "describe", "--always", "--long")
            .directory(project.rootDir)
            .redirectOutput(ProcessBuilder.Redirect.PIPE)
            .redirectError(ProcessBuilder.Redirect.PIPE)
            .start().inputStream.bufferedReader().use { it.readText() }
            .trim()
            .replace(Regex("(-0)?-[^-]+$"), "")
    } catch (e: Exception) {
        logger.error("Cannot find git, defaulting to dummy version number")
    }

    if (System.getenv("GITHUB_ACTIONS") != null) {
        val gitTag = System.getenv("GIT_TAG_NAME")
        versionName = gitTag ?: versionName
    }

    return versionName
}

fun getGitHash(): String {
    try {
        val processBuilder = ProcessBuilder("git", "rev-parse", "--short", "HEAD")
        processBuilder.directory(project.rootDir)
        val process = processBuilder.start()
        val inputStream = process.inputStream
        val errorStream = process.errorStream
        process.waitFor()

        return if (process.exitValue() == 0) {
            inputStream.bufferedReader()
                .use { it.readText().trim() } // return the value of gitHash
        } else {
            val errorMessage = errorStream.bufferedReader().use { it.readText().trim() }
            logger.error("Error running git command: $errorMessage")
            "dummy-hash" // return a dummy hash value in case of an error
        }
    } catch (e: Exception) {
        logger.error("$e: Cannot find git, defaulting to dummy build hash")
        return "dummy-hash" // return a dummy hash value in case of an error
    }
}

fun getBranch(): String {
    try {
        val processBuilder = ProcessBuilder("git", "rev-parse", "--abbrev-ref", "HEAD")
        processBuilder.directory(project.rootDir)
        val process = processBuilder.start()
        val inputStream = process.inputStream
        val errorStream = process.errorStream
        process.waitFor()

        return if (process.exitValue() == 0) {
            inputStream.bufferedReader()
                .use { it.readText().trim() } // return the value of gitHash
        } else {
            val errorMessage = errorStream.bufferedReader().use { it.readText().trim() }
            logger.error("Error running git command: $errorMessage")
            "dummy-hash" // return a dummy hash value in case of an error
        }
    } catch (e: Exception) {
        logger.error("$e: Cannot find git, defaulting to dummy build hash")
        return "dummy-hash" // return a dummy hash value in case of an error
    }
}
