include(TargetArchitectureSpecificSources)

add_executable(dynarmic_tests
    fp/FPToFixed.cpp
    fp/FPValue.cpp
    fp/mantissa_util_tests.cpp
    fp/unpacked_tests.cpp
    rand_int.h
)

if ("A32" IN_LIST DYNARMIC_FRONTENDS)
    target_sources(dynarmic_tests PRIVATE
        A32/test_arm_disassembler.cpp
        A32/test_arm_instructions.cpp
        A32/test_coprocessor.cpp
        A32/test_svc.cpp
        A32/test_thumb_instructions.cpp
        A32/testenv.h
        decoder_tests.cpp
    )
endif()

if ("A64" IN_LIST DYNARMIC_FRONTENDS)
    target_link_libraries(dynarmic_tests PRIVATE merry::oaknut)

    target_sources(dynarmic_tests PRIVATE
        A64/a64.cpp
        A64/fibonacci.cpp
        A64/fp_min_max.cpp
        A64/misaligned_page_table.cpp
        A64/test_invalidation.cpp
        A64/real_world.cpp
        A64/testenv.h
    )
endif()

if (DYNARMIC_TESTS_USE_UNICORN)
    target_link_libraries(dynarmic_tests PRIVATE Unicorn::Unicorn)

    target_sources(dynarmic_tests PRIVATE
        fuzz_util.cpp
        fuzz_util.h
    )

    if ("A32" IN_LIST DYNARMIC_FRONTENDS)
        target_sources(dynarmic_tests PRIVATE
            A32/fuzz_arm.cpp
            A32/fuzz_thumb.cpp
            unicorn_emu/a32_unicorn.cpp
            unicorn_emu/a32_unicorn.h
        )
    endif()

    if ("A64" IN_LIST DYNARMIC_FRONTENDS)
        target_sources(dynarmic_tests PRIVATE
            A64/fuzz_with_unicorn.cpp
            A64/verify_unicorn.cpp
            unicorn_emu/a64_unicorn.cpp
            unicorn_emu/a64_unicorn.h
        )
    endif()
endif()

if ("riscv" IN_LIST ARCHITECTURE)
    target_link_libraries(dynarmic_tests PRIVATE biscuit::biscuit)
endif()

if ("x86_64" IN_LIST ARCHITECTURE)
    target_link_libraries(dynarmic_tests PRIVATE xbyak::xbyak)
    target_architecture_specific_sources(dynarmic_tests "x86_64"
        x64_cpu_info.cpp
    )

    target_architecture_specific_sources(dynarmic_tests "x86_64"
        native/preserve_xmm.cpp
    )

    if (NOT MSVC AND NOT DYNARMIC_MULTIARCH_BUILD)
        target_sources(dynarmic_tests PRIVATE
            rsqrt_test.cpp
            rsqrt_test_fn.s
        )
    endif()
endif()

include(CreateDirectoryGroups)

if (("A32" IN_LIST DYNARMIC_FRONTENDS) AND ("A64" IN_LIST DYNARMIC_FRONTENDS))
    add_executable(dynarmic_print_info
        print_info.cpp
    )

    create_target_directory_groups(dynarmic_print_info)

    target_link_libraries(dynarmic_print_info PRIVATE dynarmic Boost::headers fmt::fmt merry::mcl)
    target_include_directories(dynarmic_print_info PRIVATE . ../src)
    target_compile_options(dynarmic_print_info PRIVATE ${DYNARMIC_CXX_FLAGS})
    target_compile_definitions(dynarmic_print_info PRIVATE FMT_USE_USER_DEFINED_LITERALS=1)
endif()

if (("A32" IN_LIST DYNARMIC_FRONTENDS) AND ("A64" IN_LIST DYNARMIC_FRONTENDS))
    add_executable(dynarmic_test_generator
        fuzz_util.cpp
        fuzz_util.h
        test_generator.cpp
    )

    create_target_directory_groups(dynarmic_test_generator)

    target_link_libraries(dynarmic_test_generator PRIVATE dynarmic Boost::headers fmt::fmt merry::mcl)
    target_include_directories(dynarmic_test_generator PRIVATE . ../src)
    target_compile_options(dynarmic_test_generator PRIVATE ${DYNARMIC_CXX_FLAGS})
    target_compile_definitions(dynarmic_test_generator PRIVATE FMT_USE_USER_DEFINED_LITERALS=1)
endif()

if (("A32" IN_LIST DYNARMIC_FRONTENDS) AND ("A64" IN_LIST DYNARMIC_FRONTENDS))
    add_executable(dynarmic_test_reader
        test_reader.cpp
    )

    create_target_directory_groups(dynarmic_test_reader)

    target_link_libraries(dynarmic_test_reader PRIVATE dynarmic Boost::headers fmt::fmt merry::mcl)
    target_include_directories(dynarmic_test_reader PRIVATE . ../src)
    target_compile_options(dynarmic_test_reader PRIVATE ${DYNARMIC_CXX_FLAGS})
    target_compile_definitions(dynarmic_test_reader PRIVATE FMT_USE_USER_DEFINED_LITERALS=1)
endif()

create_target_directory_groups(dynarmic_tests)

target_link_libraries(dynarmic_tests PRIVATE dynarmic Boost::headers Catch2::Catch2WithMain fmt::fmt merry::mcl)
target_include_directories(dynarmic_tests PRIVATE . ../src)
target_compile_options(dynarmic_tests PRIVATE ${DYNARMIC_CXX_FLAGS})
target_compile_definitions(dynarmic_tests PRIVATE FMT_USE_USER_DEFINED_LITERALS=1)

target_compile_options(dynarmic_tests PRIVATE -mavx2)

add_test(dynarmic_tests dynarmic_tests --durations yes)
