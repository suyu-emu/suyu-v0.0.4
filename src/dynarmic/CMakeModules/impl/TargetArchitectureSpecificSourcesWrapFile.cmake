# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

string(TOUPPER "${arch}" arch)
file(READ "${input_file}" f_contents)
file(WRITE "${output_file}" "#if defined(ARCHITECTURE_${arch})\n${f_contents}\n#endif\n")
