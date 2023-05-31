// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <jni.h>

// Function calls from the Java side
#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_UnPauseEmulation(JNIEnv* env,
                                                                              jclass clazz);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_PauseEmulation(JNIEnv* env,
                                                                            jclass clazz);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_StopEmulation(JNIEnv* env,
                                                                           jclass clazz);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_ResetRomMetadata(JNIEnv* env,
                                                                              jclass clazz);

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_IsRunning(JNIEnv* env,
                                                                           jclass clazz);

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_isHandheldOnly(JNIEnv* env,
                                                                                jclass clazz);

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_setDeviceType(JNIEnv* env,
                                                                               jclass clazz,
                                                                               jstring j_device,
                                                                               jstring j_type);

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadConnectEvent(
    JNIEnv* env, jclass clazz, jstring j_device);

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadDisconnectEvent(
    JNIEnv* env, jclass clazz, jstring j_device);

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadEvent(
    JNIEnv* env, jclass clazz, jstring j_device, jint j_button, jint action);

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadMoveEvent(
    JNIEnv* env, jclass clazz, jstring j_device, jint axis, jfloat x, jfloat y);

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadAxisEvent(
    JNIEnv* env, jclass clazz, jstring j_device, jint axis_id, jfloat axis_val);

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_onReadNfcTag(JNIEnv* env,
                                                                              jclass clazz,
                                                                              jbyteArray j_data);

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_onRemoveNfcTag(JNIEnv* env,
                                                                                jclass clazz);

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_onTouchEvent(JNIEnv* env,
                                                                              jclass clazz,
                                                                              jfloat x, jfloat y,
                                                                              jboolean pressed);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_onTouchMoved(JNIEnv* env, jclass clazz,
                                                                          jfloat x, jfloat y);

JNIEXPORT jbyteArray JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_GetIcon(JNIEnv* env,
                                                                           jclass clazz,
                                                                           jstring j_file);

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_GetTitle(JNIEnv* env, jclass clazz,
                                                                         jstring j_filename);

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_GetDescription(JNIEnv* env,
                                                                               jclass clazz,
                                                                               jstring j_filename);

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_GetGameId(JNIEnv* env, jclass clazz,
                                                                          jstring j_filename);

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_GetRegions(JNIEnv* env,
                                                                           jclass clazz,
                                                                           jstring j_filename);

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_GetCompany(JNIEnv* env,
                                                                           jclass clazz,
                                                                           jstring j_filename);

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_GetGitRevision(JNIEnv* env,
                                                                               jclass clazz);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_SetAppDirectory(JNIEnv* env,
                                                                             jclass clazz,
                                                                             jstring j_directory);

JNIEXPORT void JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_Java_org_yuzu_yuzu_1emu_NativeLibrary_InitializeGpuDriver(
    JNIEnv* env, jclass clazz, jstring hook_lib_dir, jstring custom_driver_dir,
    jstring custom_driver_name, jstring file_redirect_dir);

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_ReloadKeys(JNIEnv* env,
                                                                            jclass clazz);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_utils_DirectoryInitialization_SetSysDirectory(
    JNIEnv* env, jclass clazz, jstring path_);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_SetSysDirectory(JNIEnv* env,
                                                                             jclass clazz,
                                                                             jstring path);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_InitializeEmulation(JNIEnv* env,
                                                                                 jclass clazz);

JNIEXPORT jint JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_DefaultCPUCore(JNIEnv* env,
                                                                            jclass clazz);
JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_SetProfiling(JNIEnv* env, jclass clazz,
                                                                          jboolean enable);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_WriteProfileResults(JNIEnv* env,
                                                                                 jclass clazz);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_NotifyOrientationChange(
    JNIEnv* env, jclass clazz, jint layout_option, jint rotation);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_Run__Ljava_lang_String_2(
    JNIEnv* env, jclass clazz, jstring j_path);

JNIEXPORT void JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_Run__Ljava_lang_String_2Ljava_lang_String_2Z(
    JNIEnv* env, jclass clazz, jstring j_file, jstring j_savestate, jboolean j_delete_savestate);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_SurfaceChanged(JNIEnv* env,
                                                                            jclass clazz,
                                                                            jobject surf);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_SurfaceDestroyed(JNIEnv* env,
                                                                              jclass clazz);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_InitGameIni(JNIEnv* env, jclass clazz,
                                                                         jstring j_game_id);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_ReloadSettings(JNIEnv* env,
                                                                            jclass clazz);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_SetUserSetting(
    JNIEnv* env, jclass clazz, jstring j_game_id, jstring j_section, jstring j_key,
    jstring j_value);

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_GetUserSetting(
    JNIEnv* env, jclass clazz, jstring game_id, jstring section, jstring key);

JNIEXPORT jdoubleArray JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_GetPerfStats(JNIEnv* env,
                                                                                  jclass clazz);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_LogDeviceInfo(JNIEnv* env,
                                                                           jclass clazz);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_SubmitInlineKeyboardText(
    JNIEnv* env, jclass clazz, jstring j_text);

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_SubmitInlineKeyboardInput(
    JNIEnv* env, jclass clazz, jint j_key_code);

#ifdef __cplusplus
}
#endif
