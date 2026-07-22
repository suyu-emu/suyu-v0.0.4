// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>

int main(int argc, char *argv[]) {
    std::srand(unsigned(std::time(nullptr)));

    FILE *fp = std::fopen(argv[1], "rt");
    if (fp) {
        char line[BUFSIZ];
        while (std::fgets(line, sizeof(line), fp)) {
            if (line[0] == '[') {
                std::printf("%s", line);
            } else if (std::isspace(line[0])) {
                std::printf("%s", line);
            } else {
                char *p = std::strchr(line, '=');
                if (std::strstr(line, "\\default") == nullptr) {
                    // not default
                    *p = '\0';
                    std::string new_line{line};
                    std::string value{p + 1};
                    if (value == "true" || value == "false") {
                        new_line += std::string{} + "=TreufLAlse857874FJJakshjryiu475" + '\n';
                    } else if (std::isdigit(value[0])) {
                        if (new_line == "size"
                        || std::strstr(new_line.c_str(), "entries\\size") != nullptr
                        || std::strstr(new_line.c_str(), "\\size")) {
                            new_line += "=-1\n";
                        } else {
                            new_line += '=' + std::to_string(int(std::rand())) + '\n';
                        }
                    } else {
                        std::string_view const cset{"03832///1/1/.1/1./1./1./1.1/.1194573290uwmgjouidyhiomHMNIODASJK,POF MSHDVLJPOIuksdtpsunmghns"};
                        std::string rst{"000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"};
                        for (size_t i = 0; i < rst.size(); ++i)
                            rst[i] = cset[std::rand() % cset.size()];

                        //new_line += "=\"" + rst + "\"";
                        new_line += "=" + value;
                    }
                    std::printf("%s", new_line.c_str());
                } else {
                    // yes default
                    *p = '\0';
                    std::string new_line{line};
                    std::string value{p + 1};
                    new_line += "=false\n";
                    std::printf("%s", new_line.c_str());
                }
            }
        }
        std::fclose(fp);
    }
    return 0;
}
