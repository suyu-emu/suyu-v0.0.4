string(TOUPPER "${arch}" arch)
file(READ "${input_file}" f_contents)
file(WRITE "${output_file}" "#include <mcl/macro/architecture.hpp>\n#if defined(MCL_ARCHITECTURE_${arch})\n${f_contents}\n#endif\n")
