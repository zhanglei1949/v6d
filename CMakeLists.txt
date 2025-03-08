cmake_minimum_required(VERSION 3.3)

set(VINEYARD_MAJOR_VERSION 0)
set(VINEYARD_MINOR_VERSION 8)
set(VINEYARD_PATCH_VERSION 5)
set(VINEYARD_VERSION ${VINEYARD_MAJOR_VERSION}.${VINEYARD_MINOR_VERSION}.${VINEYARD_PATCH_VERSION})

message(STATUS "Configuring and building vineyard version '${VINEYARD_VERSION}'.")

if(POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()

if(POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)
endif()
project(vineyard LANGUAGES C CXX VERSION ${VINEYARD_VERSION})

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(USE_STATIC_BOOST_LIBS "Build with static-linked boost libraries" OFF)
option(USE_EXTERNAL_ETCD_LIBS "Build with external etcd-cpp-apiv3 library rather than the submodule one" OFF)
option(USE_EXTERNAL_REDIS_LIBS "Build with external redis-plus-plus library rather than the submodule one" ON)
option(USE_EXTERNAL_HIREDIS_LIBS "Build with external hiredis library rather than the submodule one" ON)
option(USE_EXTERNAL_TBB_LIBS "Build with external tbb library rather than the submodule one" OFF)
option(USE_EXTERNAL_NLOHMANN_JSON_LIBS "Build with external nlohmann-json library rather than the submodule one" ON)
option(USE_ASAN "Using address sanitizer to check memory accessing" OFF)
option(USE_LIBUNWIND "Using libunwind to retrieve the stack backtrace when exception occurs" ON)
option(USE_INCLUDE_WHAT_YOU_USE "Simply the intra-module dependencies with iwyu" OFF)
option(USE_JSON_DIAGNOSTICS "Using json diagnostics to check the validity of metadata" OFF)
option(USE_GPU "Using gpu support" OFF)

option(BUILD_VINEYARD_SERVER "Build vineyard's server" ON)
option(BUILD_VINEYARD_SERVER_REDIS "Enable redis as the metadta backend" OFF)
option(BUILD_VINEYARD_CLIENT "Build vineyard's client" ON)
option(BUILD_VINEYARD_CLIENT_VERBOSE "Build vineyard's client with the most verbose logs" OFF)
option(BUILD_VINEYARD_PYTHON_BINDINGS "Build vineyard's python bindings" ON)
option(BUILD_VINEYARD_PYPI_PACKAGES "Build vineyard's python bindings" OFF)

option(BUILD_VINEYARD_JAVA "Build vineyard's java SDK" OFF)

option(BUILD_VINEYARD_BASIC "Build vineyard's basic data structures" ON)
option(BUILD_VINEYARD_IO "Enable vineyard's IOAdaptor support" ON)
option(BUILD_VINEYARD_GRAPH "Enable vineyard's graph data structures" ON)
option(BUILD_VINEYARD_MALLOC "Build vineyard's implementation for client-side malloc" ON)
option(BUILD_VINEYARD_MALLOC_OVERRIDE "Using the client-side malloc to override the default ones" OFF)
option(BUILD_VINEYARD_FUSE "Enable vineyard's fuse support" OFF)
option(BUILD_VINEYARD_FUSE_PARQUET "Enable vineyard's fuse parquet support" OFF)
option(BUILD_VINEYARD_HOSSEINMOEIN_DATAFRAME "Enable hosseinmoein dataframe support" OFF)

option(BUILD_VINEYARD_TESTS "Generate make targets for vineyard tests" ON)
option(BUILD_VINEYARD_TESTS_ALL "Include make targets for vineyard tests to ALL" OFF)
option(BUILD_VINEYARD_BENCHMARKS "Generate make targets for vineyard benchmarks" ON)
option(BUILD_VINEYARD_BENCHMARKS_ALL "Include make targets for vineyard benchmarks to ALL" OFF)
option(BUILD_VINEYARD_COVERAGE "Build vineyard with coverage information, requires build with Debug" OFF)
option(BUILD_VINEYARD_PROFILING "Build vineyard with profiling information" OFF)

include(CheckCXXCompilerFlag)
include(CheckLibraryExists)
include(ExternalProject)
include(GNUInstallDirs)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(ProcessorCount)
ProcessorCount(N)

set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE
        STRING "Choose the type of build." FORCE
    )
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo"
    )
else()
    message(STATUS "Setting build type to '${CMAKE_BUILD_TYPE}'.")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release" AND
        ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR
            "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang"))
    # avoid the llvm-strip: error: unsupported load command (cmd=0x80000034) error
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

if(NOT (CMAKE_CXX_COMPILER_LAUNCHER MATCHES "ccache") AND NOT (CMAKE_C_COMPILER_LAUNCHER MATCHES "ccache"))
    find_program(ccache_EXECUTABLE ccache)
    if(ccache_EXECUTABLE)
        set(CMAKE_C_COMPILER_LAUNCHER ${ccache_EXECUTABLE})
        set(CMAKE_CXX_COMPILER_LAUNCHER ${ccache_EXECUTABLE})
        add_custom_target(ccache-stats
            COMMAND ${ccache_EXECUTABLE} --show-stats
        )
    else()
        add_custom_target(ccache-stats
            COMMAND echo "ccache not found."
        )
    endif(ccache_EXECUTABLE)
endif()

# enable colored diagnostics
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    add_compile_options(-fcolor-diagnostics)
endif()

if(USE_INCLUDE_WHAT_YOU_USE AND NOT CMAKE_CXX_INCLUDE_WHAT_YOU_USE)
    find_program(iwyu_EXECUTABLE include-what-you-use)
    if(iwyu_EXECUTABLE)
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${iwyu_EXECUTABLE})
    else()
        message(WARNING "Failed to locate iwyu, please specify with -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE instead")
    endif()
endif()

set(DEFAUTL_ALLOCATOR "dlmalloc")
set(WITH_ALLOCATOR "${DEFAUTL_ALLOCATOR}" CACHE
    STRING "Choose the allocator for vineyard server (vineyardd)."
)
set_property(CACHE WITH_ALLOCATOR PROPERTY STRINGS
    "dlmalloc" "mimalloc"
)
message(STATUS "Setting the allocator to vineyardd as '${WITH_ALLOCATOR}'.")

# check the cxx abi
include(cmake/CheckGCCABI.cmake)
check_gcc_cxx11abi()

get_directory_property(CURRENT_DEFINITIONS COMPILE_DEFINITIONS)
if(NOT ("${CURRENT_DEFINITIONS} ${CMAKE_CXX_FLAGS}" MATCHES ".*_GLIBCXX_USE_CXX11_ABI.*"))
    if(GCC_USE_CXX11_ABI EQUAL 1)
       add_compile_options(-D_GLIBCXX_USE_CXX11_ABI=1)
    endif()
endif()

# disables messages from sub_directory, see also: https://stackoverflow.com/a/38983571/5080177
function(message)
    if (NOT MESSAGE_QUIET)
        _message(${ARGN})
    endif()
endfunction()

# reference: https://gitlab.kitware.com/cmake/community/-/wikis/doc/cmake/RPATH-handling#always-full-rpath
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
if(APPLE)
    # the LC_RPATH on Mac seems doesn't support multiple path (seperated with ':seperated with `:`)
    # fortunately, we just need to take care `lib` on Mac.
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
else()
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:${CMAKE_INSTALL_PREFIX}/lib64:${CMAKE_INSTALL_PREFIX}/lib/x86_64-linux-gnu")
endif()
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC \
                                        -Wall \
                                        -Wno-attributes \
                                        -Wno-unknown-pragmas"
)

check_cxx_compiler_flag(-Wno-class-memaccess W_NO_CLASS_MEMACCESS)
check_cxx_compiler_flag(-Wno-deprecated-declarations W_NO_DEPRECATED_DECLARATIONS)
check_cxx_compiler_flag(-Wno-defaulted-function-deleted W_NO_DEFAULTED_FUNCTION_DELETED)
check_cxx_compiler_flag(-Wno-error=c++11-narrowing W_NO_ERROR_CXX11_NARROWING)
check_cxx_compiler_flag(-Wno-format-truncation W_NO_FORMAT_TRUNCATION)
check_cxx_compiler_flag(-Wno-unused-but-set-parameter W_NO_UNUSED_BUT_SET_PARAMETER)
check_cxx_compiler_flag(-Wno-unused-private-field W_NO_UNUSED_PRIVATE_FIELD)
check_cxx_compiler_flag(-Wno-noexcept-type W_NO_NOEXCEPT_TYPE)
if(W_NO_CLASS_MEMACCESS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-class-memaccess")
endif()
if(W_NO_DEPRECATED_DECLARATIONS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
endif()
if(W_NO_DEFAULTED_FUNCTION_DELETED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-defaulted-function-deleted")
endif()
if(W_NO_ERROR_CXX11_NARROWING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=c++11-narrowing")
endif()
if(W_NO_FORMAT_TRUNCATION)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-truncation")
endif()
if(W_NO_UNUSED_BUT_SET_PARAMETER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-parameter")
endif()
if(W_NO_UNUSED_PRIVATE_FIELD)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-private-field")
endif()
if(W_NO_NOEXCEPT_TYPE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-noexcept-type")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
if(BUILD_VINEYARD_COVERAGE)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
endif()
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g")

if(APPLE)
    set(CMAKE_MACOSX_RPATH ON)
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,$ORIGIN:$ORIGIN/../lib")
endif()

if(CMAKE_VERSION VERSION_LESS "3.1")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        check_cxx_compiler_flag(-std=c++14 HAVE_FLAG_STD_CXX14)
        if(BUILD_VINEYARD_PYPI_PACKAGES AND NOT HAVE_FLAG_STD_CXX14)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
        endif()
    else()
        set(HAVE_FLAG_STD_CXX14 TRUE)
    endif()
else()
    set(CMAKE_CXX_STANDARD 14)
    set(HAVE_FLAG_STD_CXX14 TRUE)
endif()

macro(add_subdirectory_shared directory)
    set(BUILD_SHARED_LIBS_SAVED "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS ON)
    set(CMAKE_BUILD_TYPE_SAVED "${CMAKE_BUILD_TYPE}")
    set(CMAKE_BUILD_TYPE Release)
    add_subdirectory(${directory} ${ARGN})
    set(BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS_SAVED}")
    set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE_SAVED}")
endmacro()

macro(add_subdirectory_static directory)
    set(BUILD_SHARED_LIBS_SAVED "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_BUILD_TYPE_SAVED "${CMAKE_BUILD_TYPE}")
    set(CMAKE_BUILD_TYPE Release)
    add_subdirectory(${directory} ${ARGN})
    set(BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS_SAVED}")
    set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE_SAVED}")
endmacro()

macro(add_subdirectory_shared_debug directory)
    set(BUILD_SHARED_LIBS_SAVED "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS ON)
    set(CMAKE_BUILD_TYPE_SAVED "${CMAKE_BUILD_TYPE}")
    set(CMAKE_BUILD_TYPE Debug)
    add_subdirectory(${directory} ${ARGN})
    set(BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS_SAVED}")
    set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE_SAVED}")
endmacro()

macro(add_subdirectory_static_debug directory)
    set(BUILD_SHARED_LIBS_SAVED "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_BUILD_TYPE_SAVED "${CMAKE_BUILD_TYPE}")
    set(CMAKE_BUILD_TYPE Debug)
    add_subdirectory(${directory} ${ARGN})
    set(BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS_SAVED}")
    set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE_SAVED}")
endmacro()

macro(find_apache_arrow)
    # workaround for: https://issues.apache.org/jira/browse/ARROW-11836
    if(NOT BUILD_VINEYARD_PYPI_PACKAGES)
      find_package(Arrow QUIET)
    endif()
    if(Arrow_FOUND)
        set(ARROW_INCLUDE_DIR)
        if (TARGET arrow_shared)
            set(ARROW_SHARED_LIB arrow_shared)
        endif()
        if (TARGET arrow_static)
            set(ARROW_STATIC_LIB arrow_static)
        endif()
    else()
        include("cmake/FindArrow.cmake")
        if(NOT ARROW_FOUND)
            message(FATAL_ERROR "apache-arrow is required, please install it and retry")
        endif()
    endif()
endmacro(find_apache_arrow)

macro(find_boost)
    if(NOT BUILD_SHARED_LIBS AND USE_STATIC_BOOST_LIBS)
        set(Boost_USE_STATIC_LIBS ON CACHE BOOL "Use static version of boost libraries.")
        set(Boost_USE_STATIC_RUNTIME ON CACHE BOOL "Use static version of boost runtimes.")
    endif()
    find_package(Boost COMPONENTS system)
    # Make boost::property_tree thread safe.
    add_compile_options(-DBOOST_SPIRIT_THREADSAFE)
    # Don't depends on the boost::system library.
    add_compile_options(-DBOOST_ERROR_CODE_HEADER_ONLY)
    add_compile_options(-DBOOST_SYSTEM_NO_DEPRECATED)
    # eliminate a lot of warnings for newer version of boost library.
    add_compile_options(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)
endmacro()

macro(find_ctti)
    set(MESSAGE_QUIET ON)
    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
    add_subdirectory_static(thirdparty/ctti)
    unset(MESSAGE_QUIET)
    set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)
endmacro()

macro(find_etcd_cpp_apiv3)
    if(USE_EXTERNAL_ETCD_LIBS)
        find_package(etcd-cpp-api 0.2.6 QUIET)
        if(NOT etcd-cpp-api_FOUND)
            message(WARNING "etcd-cpp-apiv3 not found, will use the bundled one from git submodules.")
            include("cmake/BuildEtcdCpp.cmake")
        endif()
    else()
        include("cmake/BuildEtcdCpp.cmake")
    endif()
endmacro()

macro(find_hiredis)
    if(USE_EXTERNAL_HIREDIS_LIBS)
    # not use find_package, because maybe we don't have hiredis-config.cmake
        find_path(HIREDIS_HEADER_PATH hiredis)
        find_library(HIREDIS_LIBRARY hiredis)
        if(HIREDIS_HEADER_PATH AND HIREDIS_LIBRARY)
            set(HIREDIS_INCLUDE_DIR ${HIREDIS_HEADER_PATH})
            set(HIREDIS_LIBRARIES ${HIREDIS_LIBRARY})
        else()
            message(WARNING "hiredis not found, will use the bundled one from git submodules.")
            include("cmake/BuildHiredis.cmake")
        endif()
    else()
        include("cmake/BuildHiredis.cmake")
    endif()
endmacro()

macro(find_redis_plus_plus)
    if(USE_EXTERNAL_REDIS_LIBS)
        # not use find_package, because of hiredis
        find_path(REDIS_PLUS_PLUS_HEADER sw)
        find_library(REDIS_PLUS_PLUS_LIB redis++)
        if(REDIS_PLUS_PLUS_HEADER AND REDIS_PLUS_PLUS_LIB)
            set(REDIS_PLUS_PLUS_INCLUDE_DIR "${REDIS_PLUS_PLUS_HEADER}/sw")
            set(REDIS_PLUS_PLUS_LIBRARIES ${REDIS_PLUS_PLUS_LIB})
        else()
            message(WARNING "redis-plus-plus not found, will use the bundled one from git submodules.")
            include("cmake/BuildRedisCpp.cmake")
        endif()
    else()
        include("cmake/BuildRedisCpp.cmake")
    endif()
endmacro()

macro(find_fuse)
    include("cmake/FindFUSE3.cmake")
    if(BUILD_VINEYARD_FUSE_PARQUET)
        if(NOT ARROW_PARQUET)
            message(FATAL_ERROR "Parquet is not enabled in the installed arrow.")
        endif()
        find_package(Parquet REQUIRED HINTS ${Arrow_DIR})
    endif()
endmacro()

macro(find_intel_tbb)
    set(TBB_FIND_PACKAGE OFF CACHE BOOL "Build TBB from bundled source")
    set(TBB_TEST OFF CACHE BOOL "Disable tbb testing")
    set(TBB_EXAMPLES OFF CACHE BOOL "Disable tbb examples")
    set(TBB_STRICT OFF CACHE BOOL "Don't treat warnings as errors for tbb library")
    set(TBB4PY_BUILD OFF CACHE BOOL "Disable tbb4py build")
    set(TBBMALLOC_BUILD OFF CACHE BOOL "Disable tbb4py build")
    set(TBBMALLOC_PROXY_BUILD OFF CACHE BOOL "Disable tbb4py build")
    set(TBB_ENABLE_IPO OFF CACHE BOOL "Enable interprocedural optimization")
    set(TBB_CPF ON CACHE BOOL "Enable preview features of the library")

    if(USE_EXTERNAL_TBB_LIBS)
        find_package(TBB QUIET)
        if(NOT TBB_FOUND)
            message(WARNING "intel-tbb not found, will use the bundled one from git submodules.")
            # clear previous cmake configure result
            set(TBB_DIR)
            set(MESSAGE_QUIET ON)
            add_subdirectory_static(thirdparty/intel-tbb EXCLUDE_FROM_ALL)
            unset(MESSAGE_QUIET)
        endif()
    else()
        # clear previous cmake configure result
        set(TBB_DIR)
        set(MESSAGE_QUIET ON)
        add_subdirectory_static(thirdparty/intel-tbb EXCLUDE_FROM_ALL)
        unset(MESSAGE_QUIET)
    endif()
endmacro(find_intel_tbb)

macro(find_mimalloc)
    set(MI_OVERRIDE OFF CACHE INTERNAL "")
    set(MI_OSX_INTERPOSE OFF CACHE INTERNAL "")
    set(MI_OSX_ZONE OFF CACHE INTERNAL "")
    set(MI_DEBUG_FULL OFF CACHE INTERNAL "")
    set(MI_BUILD_SHARED OFF CACHE INTERNAL "")
    set(MI_BUILD_STATIC ON CACHE INTERNAL "")
    set(MI_BUILD_TESTS OFF CACHE INTERNAL "")
    if(NOT TARGET mimalloc-static)
        add_subdirectory_static(thirdparty/mimalloc EXCLUDE_FROM_ALL)
        # Its a hack that expose the internal API symbols inside the mimalloc library, even when
        # we are building a static library.
        #
        # I have checked the source of all usages of the following two macros and it is safe to
        # enable them on Linux and MacOS.
        target_compile_options(mimalloc-static PRIVATE -DMI_SHARED_LIB -DMI_SHARED_LIB_EXPORT)
    endif()
endmacro()

macro(find_glog)
    include("cmake/FindGlog.cmake")
endmacro()

macro(find_gflags)
    include("cmake/FindGFlags.cmake")
endmacro()

macro(find_libunwind)
    if(USE_LIBUNWIND)
        include("cmake/FindLibUnwind.cmake")
    endif()
endmacro()

macro(find_nlohmann_json)
    # include nlohmann/json
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND USE_JSON_DIAGNOSTICS)
        set(JSON_Diagnostics ON CACHE INTERNAL "")
    else()
        set(JSON_Diagnostics OFF CACHE INTERNAL "")
    endif()
    set(JSON_Install ON CACHE INTERNAL "")
    set(JSON_MultipleHeaders ON CACHE INTERNAL "")
    set(JSON_ImplicitConversions OFF CACHE INTERNAL "")
    if(NOT TARGET nlohmann_json AND NOT TARGET nlohmann_json::nlohmann_json)
        if(USE_EXTERNAL_NLOHMANN_JSON_LIBS)
            find_package(nlohmann_json 3.10.5 QUIET)
            if(NOT TARGET nlohmann_json AND NOT TARGET nlohmann_json::nlohmann_json)
                message(WARNING "nlohmann-json not found, will use the bundled one from git submodules.")
                add_subdirectory_static(thirdparty/nlohmann-json)
            endif()
        else()
            add_subdirectory_static(thirdparty/nlohmann-json)
        endif()
    endif()
endmacro()

macro(find_openssl_libraries)
    if (APPLE)
        # If we're on OS X check for Homebrew's copy of OpenSSL instead of Apple's
        if (NOT OpenSSL_DIR)
            find_program(HOMEBREW brew)
            if (HOMEBREW STREQUAL "HOMEBREW-NOTFOUND")
                message(WARNING "Homebrew not found: not using Homebrew's OpenSSL")
                if (NOT OPENSSL_ROOT_DIR)
                    message(WARNING "Use -DOPENSSL_ROOT_DIR for non-Apple OpenSSL")
                endif()
            else()
                execute_process(COMMAND brew --prefix openssl
                    OUTPUT_VARIABLE OPENSSL_ROOT_DIR
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
            endif()
        endif()
    endif()
    find_package(OpenSSL ${ARGN})
    if (OPENSSL_FOUND)
        include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})
    endif()
endmacro()

macro(find_pthread)
    # find pthread
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads)
endmacro()

macro(find_cuda)
  #find cuda
  enable_language(CUDA)
  include(CheckLanguage)
  check_language(CUDA)
  find_package(CUDA REQUIRED)
endmacro(find_cuda)

macro(find_common_libraries)
    find_apache_arrow()
    find_boost()
    find_ctti()
    find_gflags()
    find_glog()
    find_libunwind()
    find_pthread()
    find_nlohmann_json()

    include_directories(SYSTEM ${ARROW_INCLUDE_DIR})
    include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
    include_directories(SYSTEM ${GLOG_INCLUDE_DIR})
    include_directories(SYSTEM ${GFLAGS_INCLUDE_DIR})
endmacro()

# find openssl first since apache-arrow may requires that (on MacOS, installed by brew)
find_openssl_libraries(QUIET)
find_common_libraries()

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/modules)
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/thirdparty)

# build profiling library
if(BUILD_VINEYARD_PROFILING)
    add_definitions(-DWITH_PROFILING)
    include("cmake/FindGperftools.cmake")
endif()

if(${LIBUNWIND_FOUND})
    add_definitions(-DWITH_LIBUNWIND)
endif ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/static-lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/shared-lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

macro(target_add_link_options target scope)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs OPTIONS)
    cmake_parse_arguments(target_add_link_options "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(${CMAKE_VERSION} VERSION_LESS "3.13")
        target_link_libraries(${target} INTERFACE ${target_add_link_options_OPTIONS})
    else()
        target_link_options(${target} ${scope} ${target_add_link_options_OPTIONS})
    endif()
endmacro()

macro(target_enable_sanitizer target visibility)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND USE_ASAN)
        message(STATUS "Vineyardd will be bulit with -fsanitize=address")
        target_compile_options(${target} ${visibility} -fsanitize=address)
        target_add_link_options(${target} ${visibility} OPTIONS -fsanitize=address)
    endif()
endmacro()

macro(install_vineyard_target target)
    install(TARGETS ${target}
        EXPORT vineyard-targets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
    )
endmacro()

macro(install_vineyard_headers header_path)
    if(${ARGC} GREATER_EQUAL 2)
        set(install_headers_destination "${ARGV1}")
    else()
        set(install_headers_destination "include/vineyard")
    endif()
    install(DIRECTORY ${header_path}
            DESTINATION ${install_headers_destination}          # target directory
            FILES_MATCHING                                      # install only matched files
            PATTERN "*.h"                                       # select header files
            PATTERN "*.hpp"                                     # select C++ template header files
            PATTERN "*.vineyard-mod"                            # select vineyard template files
            PATTERN "*/thirdparty/*" EXCLUDE                    # exclude thirdparty
    )
endmacro()

install_vineyard_headers("${PROJECT_SOURCE_DIR}/src/common")
set(VINEYARD_INSTALL_LIBS)

# resolve targets dependencies
if(BUILD_VINEYARD_PYPI_PACKAGES)
    set(BUILD_VINEYARD_PYTHON_BINDINGS ON)
endif()

if(BUILD_VINEYARD_FUSE_PARQUET)
    set(BUILD_VINEYARD_FUSE ON)
endif()

if(BUILD_VINEYARD_FUSE)
    set(BUILD_VINEYARD_BASIC ON)
endif()

if(BUILD_VINEYARD_GRAPH)
    set(BUILD_VINEYARD_BASIC ON)
    set(BUILD_VINEYARD_IO ON)
endif()

if(BUILD_VINEYARD_HOSSEINMOEIN_DATAFRAME)
    set(BUILD_VINEYARD_BASIC ON)
endif()

if(BUILD_VINEYARD_IO)
    set(BUILD_VINEYARD_BASIC ON)
endif()

if(BUILD_VINEYARD_BASIC OR BUILD_VINEYARD_IO OR BUILD_VINEYARD_MIGRATION OR BUILD_VINEYARD_PYTHON_BINDINGS)
    set(BUILD_VINEYARD_CLIENT ON)
endif()

if(BUILD_VINEYARD_MALLOC)
    find_mimalloc()
    set(BUILD_VINEYARD_CLIENT ON)
endif()

add_custom_target(vineyard_tests)
add_custom_target(vineyard_benchmarks)
add_custom_target(vineyard_codegen)
add_custom_target(vineyard_codegen_java)

include("cmake/GenerateVineyard.cmake")
include("cmake/GenerateVineyardJava.cmake")

# build vineyardd
if(BUILD_VINEYARD_SERVER)
    find_openssl_libraries(REQUIRED)
    find_etcd_cpp_apiv3()
    find_intel_tbb()
    find_mimalloc()
    if(BUILD_VINEYARD_SERVER_REDIS)
        find_hiredis()
        find_redis_plus_plus()
    endif()
    file(GLOB_RECURSE SERVER_SRC_FILES "src/server/*.cc"
                                       "src/common/memory/*.cc"
                                       "src/common/util/*.cc"
                                       "src/common/memory/gpu/*.cc"
                                       
    )
    if(BUILD_VINEYARD_SERVER_REDIS)
        list(APPEND SERVER_SRC_FILES "thirdparty/redis-plus-plus-shim/recipes/redlock.cpp")
    endif()
    add_executable(vineyardd ${SERVER_SRC_FILES})
    target_compile_options(vineyardd PRIVATE -DBUILD_VINEYARDD)
    target_link_libraries(vineyardd PRIVATE ${Boost_LIBRARIES}
                                            ${CPPREST_LIB}
                                            ${ETCD_CPP_LIBRARIES}
                                            ${GFLAGS_LIBRARIES}
                                            ${GLOG_LIBRARIES}
                                            ${OPENSSL_LIBRARIES}
                                            mimalloc-static
                                            nlohmann_json::nlohmann_json
                                            TBB::tbb
    )
    if(ARROW_SHARED_LIB)
        target_link_libraries(vineyardd PRIVATE ${ARROW_SHARED_LIB})
    else()
        target_link_libraries(vineyardd PRIVATE ${ARROW_STATIC_LIB})
    endif()

    if(USE_GPU)
        enable_language(CUDA)
        find_package(CUDA REQUIRED)
        add_definitions(-DENABLE_GPU)
        include_directories(${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES}) 
        target_link_libraries(vineyardd PUBLIC ${CUDA_LIBRARIES})
    endif()

    target_include_directories(vineyardd PRIVATE ${ETCD_CPP_INCLUDE_DIR})
    if(BUILD_VINEYARD_SERVER_REDIS)
        target_compile_options(vineyardd PRIVATE -DBUILD_VINEYARDD_REDIS)
        target_include_directories(vineyardd PRIVATE ${HIREDIS_INCLUDE_DIR}
                                                     ${REDIS_PLUS_PLUS_INCLUDE_DIR}
        )
        target_link_libraries(vineyardd PRIVATE ${HIREDIS_LIBRARIES}
                                                ${REDIS_PLUS_PLUS_LIBRARIES}
        )
    endif()

    if(${LIBUNWIND_FOUND})
        target_link_libraries(vineyardd PRIVATE ${LIBUNWIND_LIBRARIES})
    endif()
    install_vineyard_target(vineyardd)
    install_vineyard_headers("${PROJECT_SOURCE_DIR}/src/server")
    target_enable_sanitizer(vineyardd PRIVATE)
    if(NOT BUILD_SHARED_LIBS)
        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
            target_compile_options(vineyardd PRIVATE -static-libgcc -static-libstdc++ -Os)
            target_add_link_options(vineyardd PRIVATE OPTIONS -static-libgcc -static-libstdc++ -Os)
        endif()
        target_link_libraries(vineyardd PRIVATE ${GRPC_GRPC++_LIBRARY} ${GRPC_LIBRARY} ${GPR_LIBRARY})
    endif()
    if(BUILD_VINEYARD_PROFILING)
        target_include_directories(vineyardd PRIVATE ${Gperftools_INCLUDE_DIRS})
        target_link_libraries(vineyardd PRIVATE ${Gperftools_LIBRARIES})
    endif()

    message(STATUS "Vineyard will use '${WITH_ALLOCATOR}' by default for shared memory allocation")
    # bundle dlmalloc and mimalloc at the same time
    target_compile_options(vineyardd PRIVATE -DWITH_DLMALLOC -DWITH_MIMALLOC)

    if(WITH_ALLOCATOR STREQUAL "dlmalloc")
        target_compile_options(vineyardd PRIVATE -DDEFAULT_ALLOCATOR=dlmalloc)
    endif()
    if(WITH_ALLOCATOR STREQUAL "mimalloc")
        target_compile_options(vineyardd PRIVATE -DDEFAULT_ALLOCATOR=mimalloc)
    endif()

    file(GLOB_RECURSE VINEYARD_CLIENT_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src"
                                              "client/*.h")
    if(BUILD_VINEYARD_JAVA AND VINEYARD_CLIENT_HEADERS)
        vineyard_generate_java(
            OUT_VAR VINEYARD_JAVA_GENERATES
            VINEYARD_MODULES ${VINEYARD_CLIENT_HEADERS}
        )
    else()
        set(VINEYARD_JAVA_GENERATES)
    endif()
    add_custom_target(vineyard_client_gen_java
        DEPENDS ${VINEYARD_JAVA_GENERATES}
        COMMENT "Running java code generation for vineyard_client."
    )
    if(VINEYARD_JAVA_GENERATES)
        add_dependencies(vineyard_codegen_java vineyard_client_gen_java)
    endif()
endif()

# build vineyard-client
if(BUILD_VINEYARD_CLIENT)
    # build vineyard registry library, it can only be a shared library, as a singleton.
    add_library(vineyard_internal_registry SHARED "src/client/ds/factory/registry.cc")
    target_link_libraries(vineyard_internal_registry PRIVATE ${CMAKE_DL_LIBS})
    if(BUILD_VINEYARD_PYPI_PACKAGES)
        target_compile_options(vineyard_internal_registry PRIVATE -Os)
        target_add_link_options(vineyard_internal_registry PRIVATE OPTIONS -Os)
        target_add_link_options(vineyard_internal_registry PRIVATE OPTIONS -Os)
    endif()
    install_vineyard_target(vineyard_internal_registry)
    target_add_link_options(vineyard_internal_registry PRIVATE
                            OPTIONS -Wl,-rpath,.:$ORIGIN:${CMAKE_INSTALL_PREFIX}/lib:${CMAKE_INSTALL_PREFIX}/lib64
    )
    set_target_properties(vineyard_internal_registry PROPERTIES CXX_VISIBILITY_PRESET "hidden")

    target_include_directories(vineyard_internal_registry PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ctti/include>
        $<INSTALL_INTERFACE:include>
    )

    # build the vineyard client library
    file(GLOB CLIENT_SRC_FILES "src/client/*.cc"
                               "src/client/ds/*.cc"
                               "src/common/memory/*.cc"
                               "src/common/util/*.cc"
                               "src/common/memory/gpu/*.cc"
    )
    # the vineyard_client can only be a shared library, since the ObjectFactory
    # is a singleton.
    add_library(vineyard_client ${CLIENT_SRC_FILES})
    if(USE_GPU)
        find_package(CUDA REQUIRED)
        target_link_libraries(vineyard_client PUBLIC ${CUDA_LIBRARIES})
    endif()
    target_link_libraries(vineyard_client PUBLIC ${CMAKE_DL_LIBS}
                                                 Threads::Threads
                                                 nlohmann_json::nlohmann_json
    )
    # make sure `vineyard_internal_registry` been built.
    add_dependencies(vineyard_client vineyard_internal_registry)
    if(ARROW_SHARED_LIB)
        target_link_libraries(vineyard_client PUBLIC ${ARROW_SHARED_LIB})
    else()
        target_link_libraries(vineyard_client PUBLIC ${ARROW_STATIC_LIB})
    endif()
    if(${LIBUNWIND_FOUND})
        target_link_libraries(vineyard_client PRIVATE ${LIBUNWIND_LIBRARIES})
    endif()

    target_include_directories(vineyard_client PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ctti/include>
        $<INSTALL_INTERFACE:include/vineyard/contrib>
    )

    if(BUILD_VINEYARD_CLIENT_VERBOSE)
        target_compile_options(vineyard_client PRIVATE -DWITH_VERBOSE)
    endif()

    if(BUILD_VINEYARD_PYPI_PACKAGES)
        target_compile_options(vineyard_client PRIVATE -Os)
        target_add_link_options(vineyard_client PRIVATE OPTIONS -Os)
        if(APPLE)
            target_compile_options(vineyard_client PRIVATE -fvisibility=hidden)
        endif()
    endif()
    install_vineyard_target(vineyard_client)
    install_vineyard_headers("${PROJECT_SOURCE_DIR}/src/client")
    list(APPEND VINEYARD_INSTALL_LIBS vineyard_client)

    install(DIRECTORY thirdparty/ctti/include/ctti
            DESTINATION include/vineyard/contrib  # target directory
            FILES_MATCHING                        # install only matched files
            PATTERN "*.hpp"                       # select C++ template header files
    )
endif()

if(BUILD_VINEYARD_PYTHON_BINDINGS)
    set(PYBIND11_PYTHON_VERSION 3)
    add_subdirectory_static(thirdparty/pybind11)
    set(PYTHON_BIND_FILES "python/client.cc"
                          "python/core.cc"
                          "python/error.cc"
                          "python/pybind11_utils.cc"
                          "python/vineyard.cc")
    pybind11_add_module(_C MODULE ${PYTHON_BIND_FILES})
    target_link_libraries(_C PRIVATE vineyard_client)
    target_include_directories(_C PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                                         thirdparty/pybind11/include)
    target_compile_options(_C PRIVATE -Wno-unused-value)
    set_target_properties(_C PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/shared-lib")
    target_add_link_options(_C PRIVATE
                            OPTIONS -Wl,-rpath,.:$ORIGIN:${CMAKE_INSTALL_PREFIX}/lib:${CMAKE_INSTALL_PREFIX}/lib64
    )
    if(UNIX AND NOT APPLE)
        target_add_link_options(_C PRIVATE OPTIONS -Wl,--exclude-libs=ALL)
    endif()
    if(BUILD_VINEYARD_PYPI_PACKAGES AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(_C PRIVATE -static)
        target_add_link_options(_C PRIVATE OPTIONS -static)
    else()
        target_compile_options(_C PRIVATE -Os)
        target_add_link_options(_C PRIVATE OPTIONS -Os)
    endif()

    add_custom_target(vineyard_client_python
        ALL
        COMMAND cp "$<TARGET_FILE:_C>" "${PROJECT_SOURCE_DIR}/python/vineyard/"
        COMMAND cp "$<TARGET_FILE:vineyard_internal_registry>" "${PROJECT_SOURCE_DIR}/python/vineyard/"
        DEPENDS _C vineyard_internal_registry
        COMMENT "Copying python extensions."
        VERBATIM)
endif()

# add sub_directories
if(BUILD_VINEYARD_BASIC)
    add_subdirectory(modules/basic)
    list(APPEND VINEYARD_INSTALL_LIBS vineyard_basic)
endif()

if(BUILD_VINEYARD_IO)
    add_subdirectory(modules/io)
    list(APPEND VINEYARD_INSTALL_LIBS vineyard_io)
endif()

if(BUILD_VINEYARD_GRAPH)
    add_subdirectory(modules/graph)
    list(APPEND VINEYARD_INSTALL_LIBS vineyard_graph)
endif()

if(BUILD_VINEYARD_MALLOC)
    add_subdirectory(modules/malloc)
    list(APPEND VINEYARD_INSTALL_LIBS vineyard_malloc)
endif()

if(BUILD_VINEYARD_FUSE)
    find_fuse()
    add_subdirectory(modules/fuse)
    # don't includes vineyard_fuse to "VINEYARD_LIBRARIES"
endif()

if(BUILD_VINEYARD_HOSSEINMOEIN_DATAFRAME)
    add_subdirectory(modules/hosseinmoein-dataframe)
    list(APPEND VINEYARD_INSTALL_LIBS vineyard_hosseinmoein_dataframe)
endif()


if(BUILD_VINEYARD_TESTS)
    add_subdirectory(test)
endif()

if(BUILD_VINEYARD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()

file(GLOB_RECURSE FILES_NEED_FORMAT "src/*.cc" "src/*.h" "src/*.hpp" "src/*.vineyard-mod"
                                    "modules/*.cc" "modules/*.h" "modules/*.vineyard-mod"
                                    "python/*.cc" "python/*.h"
                                    "test/*.cc"
                                    "benchmark/*.h" "benchmark/*.cc" "benchmark/*.cpp"
)
file(GLOB_RECURSE FILES_NEED_LINT "src/*.cc" "src/*.h" "src/*.hpp"
                                  "modules/*.cc" "modules/*.h"
                                  "python/*.cc" "python/*.h"
                                  "test/*.cc"
                                  "benchmark/*.h" "benchmark/*.cc" "benchmark/*.cpp"
)

foreach (file_path ${FILES_NEED_FORMAT})
    if (${file_path} MATCHES ".*vineyard.h" OR
        ${file_path} MATCHES ".*thirdparty.*")
        list(REMOVE_ITEM FILES_NEED_FORMAT ${file_path})
    endif ()
endforeach ()

foreach (file_path ${FILES_NEED_LINT})
    if (${file_path} MATCHES ".*vineyard.h" OR
        ${file_path} MATCHES ".*memcpy.h" OR
        ${file_path} MATCHES ".*thirdparty.*")
        list(REMOVE_ITEM FILES_NEED_LINT ${file_path})
    endif ()
endforeach ()

add_custom_target(vineyard_clformat
        COMMAND clang-format --style=file -i ${FILES_NEED_FORMAT}
        COMMENT "Running clang-format."
        VERBATIM)

add_custom_target(vineyard_cpplint
        COMMAND ${PROJECT_SOURCE_DIR}/misc/cpplint.py --root=vineyard ${FILES_NEED_LINT}
        COMMENT "Running cpplint check."
        VERBATIM)

add_custom_target(python_install
        COMMAND python3 -m pip install --user .
        COMMAND python3 setup_airflow.py install
        COMMAND python3 setup_ml.py install
        COMMAND python3 setup_dask.py install
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        VERBATIM)

# for python's setup.cfg
configure_file("${PROJECT_SOURCE_DIR}/setup.cfg.in"
               "${PROJECT_SOURCE_DIR}/setup.cfg" @ONLY
)
configure_file("${PROJECT_SOURCE_DIR}/python/vineyard/version.py.in"
               "${PROJECT_SOURCE_DIR}/python/vineyard/version.py" @ONLY
)
configure_file("${PROJECT_SOURCE_DIR}/src/common/util/config.h.in"
               "${PROJECT_SOURCE_DIR}/src/common/util/config.h" @ONLY
)

configure_file("${PROJECT_SOURCE_DIR}/vineyard-config.in.cmake"
               "${PROJECT_BINARY_DIR}/vineyard-config.cmake" @ONLY
)
configure_file("${PROJECT_SOURCE_DIR}/vineyard-config-version.in.cmake"
               "${PROJECT_BINARY_DIR}/vineyard-config-version.cmake" @ONLY
)
install(FILES "${PROJECT_BINARY_DIR}/vineyard-config.cmake"
              "${PROJECT_BINARY_DIR}/vineyard-config-version.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/FindArrow.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/CheckGCCABI.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/FindFUSE3.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/FindGFlags.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/FindGlog.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/FindGperftools.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/FindLibUnwind.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/FindRdkafka.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/FindPythonExecutable.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/GenerateVineyard.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/GenerateVineyardJava.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/DetermineImplicitIncludes.cmake"
              DESTINATION lib/cmake/vineyard
)
install(EXPORT vineyard-targets
        FILE vineyard-targets.cmake
        DESTINATION lib/cmake/vineyard
)

# build docs
find_program(doxygen_EXECUTABLE doxygen NO_CMAKE_SYSTEM_PATH)
find_program(sphinx_build_EXECUTABLE sphinx-build NO_CMAKE_SYSTEM_PATH)
if(doxygen_EXECUTABLE AND sphinx_build_EXECUTABLE)
    add_custom_target(vineyard_doc
        COMMAND ${CMAKE_COMMAND} -E make_directory _build
        COMMAND ${doxygen_EXECUTABLE}
        COMMAND ${sphinx_build_EXECUTABLE} . _build/html
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/docs
        VERBATIM
    )
else()
    if(NOT doxygen_EXECUTABLE)
        message(STATUS "Cannot find the doxygen executable.")
    endif()
    if(NOT sphinx_build_EXECUTABLE)
        message(STATUS "Cannot find the sphinx-build executable.")
    endif()
endif()
