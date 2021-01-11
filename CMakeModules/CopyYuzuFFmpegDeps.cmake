function(copy_yuzu_FFmpeg_deps target_dir)
    include(WindowsCopyFiles)
    set(DLL_DEST "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/")
    windows_copy_files(${target_dir} ${FFMPEG_DLL_DIR} ${DLL_DEST}
        avcodec-58.dll
        avutil-56.dll
        swresample-3.dll
        swscale-5.dll
    )
endfunction(copy_yuzu_FFmpeg_deps)
