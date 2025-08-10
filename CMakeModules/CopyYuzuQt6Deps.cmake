# SPDX-FileCopyrightText: 2024 kleidis

function(copy_yuzu_Qt6_deps target_dir)
    include(WindowsCopyFiles)
    if (MSVC)
        set(DLL_DEST "$<TARGET_FILE_DIR:${target_dir}>/")
        set(Qt6_DLL_DIR "${Qt6_DIR}/../../../bin")
    else()
        set(DLL_DEST "${CMAKE_BINARY_DIR}/bin/")
        set(Qt6_DLL_DIR "${Qt6_DIR}/../../../lib/")
    endif()
    set(Qt6_PLATFORMS_DIR "${Qt6_DIR}/../../../plugins/platforms/")
    set(Qt6_STYLES_DIR "${Qt6_DIR}/../../../plugins/styles/")
    set(Qt6_IMAGEFORMATS_DIR "${Qt6_DIR}/../../../plugins/imageformats/")
    set(Qt6_RESOURCES_DIR "${Qt6_DIR}/../../../resources/")
    set(PLATFORMS ${DLL_DEST}plugins/platforms/)
    set(STYLES ${DLL_DEST}plugins/styles/)
    set(IMAGEFORMATS ${DLL_DEST}plugins/imageformats/)
	set(RESOURCES ${DLL_DEST}resources/)
    if (MSVC)
        windows_copy_files(${target_dir} ${Qt6_DLL_DIR} ${DLL_DEST}
            Qt6Core$<$<CONFIG:Debug>:d>.*
            Qt6Gui$<$<CONFIG:Debug>:d>.*
            Qt6Widgets$<$<CONFIG:Debug>:d>.*
            Qt6Network$<$<CONFIG:Debug>:d>.*
        )
        if (YUZU_USE_QT_MULTIMEDIA)
            windows_copy_files(${target_dir} ${Qt6_DLL_DIR} ${DLL_DEST}
                Qt6Multimedia$<$<CONFIG:Debug>:d>.*
            )
        endif()
        if (YUZU_USE_QT_WEB_ENGINE)
            windows_copy_files(${target_dir} ${Qt6_DLL_DIR} ${DLL_DEST}
                Qt6OpenGL$<$<CONFIG:Debug>:d>.*
                Qt6Positioning$<$<CONFIG:Debug>:d>.*
                Qt6PrintSupport$<$<CONFIG:Debug>:d>.*
                Qt6Qml$<$<CONFIG:Debug>:d>.*
				Qt6QmlMeta$<$<CONFIG:Debug>:d>.*
                Qt6QmlModels$<$<CONFIG:Debug>:d>.*
				Qt6QmlWorkerScript$<$<CONFIG:Debug>:d>.*
                Qt6Quick$<$<CONFIG:Debug>:d>.*
                Qt6QuickWidgets$<$<CONFIG:Debug>:d>.*
                Qt6WebChannel$<$<CONFIG:Debug>:d>.*
                Qt6WebEngineCore$<$<CONFIG:Debug>:d>.*
                Qt6WebEngineWidgets$<$<CONFIG:Debug>:d>.*
				QtWebEngineProcess$<$<CONFIG:Debug>:d>.*
            )
            windows_copy_files(${target_dir} ${Qt6_RESOURCES_DIR} ${RESOURCES}
                icudtl.dat
                qtwebengine_devtools_resources.pak
                qtwebengine_resources.pak
                qtwebengine_resources_100p.pak
                qtwebengine_resources_200p.pak
				v8_context_snapshot.bin
            )
        endif()
        windows_copy_files(yuzu ${Qt6_PLATFORMS_DIR} ${PLATFORMS} qwindows$<$<CONFIG:Debug>:d>.*)
        windows_copy_files(yuzu ${Qt6_STYLES_DIR} ${STYLES} qmodernwindowsstyle$<$<CONFIG:Debug>:d>.*)
        windows_copy_files(yuzu ${Qt6_IMAGEFORMATS_DIR} ${IMAGEFORMATS}
            qjpeg$<$<CONFIG:Debug>:d>.*
            qgif$<$<CONFIG:Debug>:d>.*
        )
    else()
        # Update for non-MSVC platforms if needed
    endif()
endfunction(copy_yuzu_Qt6_deps)
