# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                    PodRadio Unit Tests                                    ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

enable_testing()

find_package(GTest QUIET)

if(NOT GTest_FOUND)
    # Try pkg-config fallback for GTest
    if(PkgConfig_FOUND)
        pkg_check_modules(GTEST QUIET gtest gtest_main)
    endif()
endif()

if(GTest_FOUND OR GTEST_FOUND)
    message(STATUS "Unit tests: Enabled (GTest found)")

    # =====================================================================
    # test_utils - String/text formatting utilities
    # =====================================================================
    add_executable(test_utils test_utils.cpp)
    target_link_libraries(test_utils PRIVATE podradio_lib GTest::GTest GTest::Main)
    add_test(NAME test_utils COMMAND test_utils)

    # =====================================================================
    # test_database - SQLite persistence layer
    # =====================================================================
    add_executable(test_database test_database.cpp)
    target_link_libraries(test_database PRIVATE podradio_lib GTest::GTest GTest::Main)
    add_test(NAME test_database COMMAND test_database)

    # =====================================================================
    # test_parsers - URL classifier and feed parsers
    # =====================================================================
    add_executable(test_parsers test_parsers.cpp)
    target_link_libraries(test_parsers PRIVATE podradio_lib GTest::GTest GTest::Main)
    add_test(NAME test_parsers COMMAND test_parsers)

else()
    message(WARNING "Unit tests: Disabled (GTest not found - install libgtest-dev)")
endif()
