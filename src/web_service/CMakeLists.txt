# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(web_service STATIC
    announce_room_json.cpp
    announce_room_json.h
    verify_login.cpp
    verify_login.h
    verify_user_jwt.cpp
    verify_user_jwt.h
    web_backend.cpp
    web_backend.h
    web_result.h
)

create_target_directory_groups(web_service)
target_include_directories(web_service PUBLIC ${cpp-jwt_SOURCE_DIR}/include)
target_link_libraries(web_service PRIVATE common network nlohmann_json::nlohmann_json httplib::httplib cpp-jwt::cpp-jwt)

find_package(OpenSSL REQUIRED)
target_link_libraries(web_service PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_compile_definitions(web_service PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
