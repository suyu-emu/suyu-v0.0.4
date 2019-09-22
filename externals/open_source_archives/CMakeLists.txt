add_library(open_source_archives
    src/FontChineseSimplified.cpp
    src/FontChineseTraditional.cpp
    src/FontExtendedChineseSimplified.cpp
    src/FontKorean.cpp
    src/FontNintendoExtended.cpp
    src/FontStandard.cpp
    include/FontChineseSimplified.h
    include/FontChineseTraditional.h
    include/FontExtendedChineseSimplified.h
    include/FontKorean.h
    include/FontNintendoExtended.h
    include/FontStandard.h
)

target_include_directories(open_source_archives PUBLIC include)
