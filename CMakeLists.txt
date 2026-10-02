cmake_minimum_required(VERSION 3.25)

project(KerrLab VERSION 0.1.0 LANGUAGES CXX)

option(KERR_BUILD_TESTS "Build deterministic physics tests" ON)
option(KERR_BUILD_RENDERER "Build the windowed Vulkan presentation diagnostic" OFF)
option(KERR_BUILD_VULKAN "Build headless Vulkan metric conformance tools" OFF)
option(KERR_REQUIRE_GPU_VALIDATION "Require Khronos validation for the GPU CTest" OFF)
option(KERR_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)
option(KERR_BUILD_TOOLS "Build CPU tracer and reproducible benchmark" ON)
option(KERR_ENABLE_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(kerr_set_project_warnings target)
    if(KERR_ENABLE_SANITIZERS)
        if(MSVC)
            message(FATAL_ERROR "The combined ASan/UBSan preset currently requires Clang or GCC.")
        endif()
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc /fp:strict)
        if(KERR_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wdouble-promotion
            -Wformat=2
            -fno-fast-math
            -ffp-contract=off
        )
        if(KERR_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(kerr_set_executable_runtime target)
    # Keep local LLVM-MinGW executables runnable from CTest or Explorer without
    # relying on the caller's PATH. Other toolchains need no special handling.
    if(MINGW)
        get_filename_component(compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set(runtime_libraries libc++.dll libunwind.dll libwinpthread-1.dll)
        if(KERR_ENABLE_SANITIZERS)
            list(APPEND runtime_libraries libclang_rt.asan_dynamic-x86_64.dll)
        endif()
        foreach(runtime IN LISTS runtime_libraries)
            if(EXISTS "${compiler_bin}/${runtime}")
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${compiler_bin}/${runtime}" "$<TARGET_FILE_DIR:${target}>"
                    VERBATIM)
            endif()
        endforeach()
    endif()
endfunction()

add_library(kerr_physics STATIC
    src/physics/KerrSpacetime.cpp
    src/physics/Geodesic.cpp
    src/physics/RayTracer.cpp
    src/physics/Observer.cpp
    src/physics/Transport.cpp
    src/physics/Connection.cpp
    src/physics/ObserverTracer.cpp
    src/physics/StationaryObserver.cpp
    src/physics/Units.cpp
)
add_library(KerrLab::Physics ALIAS kerr_physics)

target_compile_features(kerr_physics PUBLIC cxx_std_20)
target_include_directories(kerr_physics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
kerr_set_project_warnings(kerr_physics)

add_library(kerr_app STATIC src/app/ObserverSession.cpp)
add_library(KerrLab::App ALIAS kerr_app)
target_link_libraries(kerr_app PUBLIC KerrLab::Physics)
kerr_set_project_warnings(kerr_app)

if(KERR_BUILD_TOOLS)
    add_executable(kerr_trace src/app/trace_main.cpp)
    target_link_libraries(kerr_trace PRIVATE KerrLab::Physics)
    kerr_set_project_warnings(kerr_trace)
    kerr_set_executable_runtime(kerr_trace)
    add_executable(kerr_worldline src/app/worldline_main.cpp)
    target_link_libraries(kerr_worldline PRIVATE KerrLab::Physics)
    kerr_set_project_warnings(kerr_worldline)
    kerr_set_executable_runtime(kerr_worldline)
    add_executable(kerr_session src/app/session_main.cpp)
    target_link_libraries(kerr_session PRIVATE KerrLab::App)
    kerr_set_project_warnings(kerr_session)
    kerr_set_executable_runtime(kerr_session)
    add_executable(kerr_benchmark benchmarks/cpu_integrators.cpp)
    target_link_libraries(kerr_benchmark PRIVATE KerrLab::Physics)
    kerr_set_project_warnings(kerr_benchmark)
    kerr_set_executable_runtime(kerr_benchmark)
endif()

if(KERR_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(kerr_physics_tests
        tests/physics_tests.cpp
    )
    target_compile_features(kerr_physics_tests PRIVATE cxx_std_20)
    target_link_libraries(kerr_physics_tests PRIVATE KerrLab::Physics)
    kerr_set_project_warnings(kerr_physics_tests)
    kerr_set_executable_runtime(kerr_physics_tests)

    add_test(NAME physics_foundation COMMAND kerr_physics_tests)

    add_executable(kerr_geodesic_tests tests/geodesic_tests.cpp)
    target_link_libraries(kerr_geodesic_tests PRIVATE KerrLab::Physics)
    kerr_set_project_warnings(kerr_geodesic_tests)
    kerr_set_executable_runtime(kerr_geodesic_tests)
    add_test(NAME geodesic_reference COMMAND kerr_geodesic_tests)

    foreach(suite observer timelike transport adaptive_transport stationary bound_orbit)
        add_executable(kerr_${suite}_tests tests/${suite}_tests.cpp)
        target_link_libraries(kerr_${suite}_tests PRIVATE KerrLab::Physics)
        kerr_set_project_warnings(kerr_${suite}_tests)
        kerr_set_executable_runtime(kerr_${suite}_tests)
        add_test(NAME ${suite}_reference COMMAND kerr_${suite}_tests)
    endforeach()
    add_executable(kerr_session_tests tests/session_tests.cpp)
    target_link_libraries(kerr_session_tests PRIVATE KerrLab::App)
    kerr_set_project_warnings(kerr_session_tests)
    kerr_set_executable_runtime(kerr_session_tests)
    add_test(NAME session_reference COMMAND kerr_session_tests)
endif()

if(KERR_BUILD_VULKAN OR KERR_BUILD_RENDERER)
    find_path(KERR_VULKAN_INCLUDE_DIR vulkan/vulkan_core.h
        HINTS "${CMAKE_CURRENT_SOURCE_DIR}/.tools/Vulkan-Headers-2cd90f9d20df57eac214c148f3aed885372ddcfe/include"
              "$ENV{VULKAN_SDK}/Include" "$ENV{VULKAN_SDK}/include"
        REQUIRED)
    file(STRINGS "${KERR_VULKAN_INCLUDE_DIR}/vulkan/vulkan_core.h" kerr_swapchain_header
        REGEX "^#define VK_KHR_swapchain_maintenance1 1")
    if(NOT kerr_swapchain_header)
        message(FATAL_ERROR "Vulkan headers with VK_KHR_swapchain_maintenance1 are required; use the pinned bootstrap or a newer SDK.")
    endif()
    find_program(KERR_GLSLANG NAMES glslang glslangValidator
        HINTS "${CMAKE_CURRENT_SOURCE_DIR}/.tools/glslang-build/StandAlone"
              "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
        REQUIRED)
    set(metric_shader "${CMAKE_CURRENT_BINARY_DIR}/shaders/metric_v1.spv")
    add_custom_command(OUTPUT "${metric_shader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${KERR_GLSLANG}" -V --target-env vulkan1.3 -Od
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/metric_v1.comp" -o "${metric_shader}"
        DEPENDS shaders/metric_v1.comp VERBATIM)
    add_custom_target(kerr_metric_shader DEPENDS "${metric_shader}")

    add_library(kerr_gpu_physics STATIC src/gpu_physics/MetricLayout.cpp)
    target_compile_features(kerr_gpu_physics PUBLIC cxx_std_20)
    target_include_directories(kerr_gpu_physics PUBLIC include)
    kerr_set_project_warnings(kerr_gpu_physics)
    add_library(kerr_vulkan STATIC src/renderer/vulkan/ComputeContext.cpp)
    target_compile_features(kerr_vulkan PUBLIC cxx_std_20)
    target_include_directories(kerr_vulkan PUBLIC include)
    target_include_directories(kerr_vulkan SYSTEM PRIVATE "${KERR_VULKAN_INCLUDE_DIR}")
    target_link_libraries(kerr_vulkan PRIVATE ${CMAKE_DL_LIBS})
    kerr_set_project_warnings(kerr_vulkan)
    add_executable(kerr_gpu src/app/gpu_main.cpp)
    target_link_libraries(kerr_gpu PRIVATE kerr_vulkan kerr_gpu_physics KerrLab::Physics)
    target_compile_definitions(kerr_gpu PRIVATE KERR_METRIC_SHADER="${metric_shader}")
    add_dependencies(kerr_gpu kerr_metric_shader)
    kerr_set_project_warnings(kerr_gpu)
    kerr_set_executable_runtime(kerr_gpu)
    if(KERR_BUILD_TESTS)
        add_test(NAME gpu_shader_abi COMMAND kerr_gpu --abi-test)
        if(KERR_REQUIRE_GPU_VALIDATION)
            add_test(NAME gpu_metric_conformance COMMAND kerr_gpu --validation on)
        else()
            add_test(NAME gpu_metric_conformance COMMAND kerr_gpu)
        endif()
        set_tests_properties(gpu_metric_conformance PROPERTIES LABELS "gpu" TIMEOUT 60)
    endif()
    if(KERR_BUILD_RENDERER)
        if(MINGW AND NOT CMAKE_C_COMPILER)
            get_filename_component(compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
            set(CMAKE_C_COMPILER "${compiler_bin}/clang.exe" CACHE FILEPATH "C compiler for GLFW")
        endif()
        enable_language(C)
        set(KERR_GLFW_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.tools/glfw-7b6aead9fb88b3623e3b3725ebb42670cbe4c579" CACHE PATH "Pinned GLFW 3.4 source")
        if(NOT EXISTS "${KERR_GLFW_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "GLFW source missing. Run tools/bootstrap-vulkan.ps1 -Window or set KERR_GLFW_SOURCE_DIR.")
        endif()
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
        add_subdirectory("${KERR_GLFW_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/third_party/glfw" EXCLUDE_FROM_ALL)
        add_library(kerr_platform STATIC src/platform/Window.cpp)
        target_compile_features(kerr_platform PUBLIC cxx_std_20)
        target_include_directories(kerr_platform PUBLIC include)
        target_include_directories(kerr_platform SYSTEM PRIVATE "${KERR_VULKAN_INCLUDE_DIR}")
        target_link_libraries(kerr_platform PRIVATE glfw)
        kerr_set_project_warnings(kerr_platform)
        target_sources(kerr_vulkan PRIVATE src/renderer/vulkan/WindowRenderer.cpp)
        set(presentation_shader "${CMAKE_CURRENT_BINARY_DIR}/shaders/presentation_test.spv")
        add_custom_command(OUTPUT "${presentation_shader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
            COMMAND "${KERR_GLSLANG}" -V --target-env vulkan1.3 -Od
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/presentation_test.comp" -o "${presentation_shader}"
            DEPENDS shaders/presentation_test.comp VERBATIM)
        add_custom_target(kerr_presentation_shader DEPENDS "${presentation_shader}")
        add_executable(kerr_window src/app/window_main.cpp)
        target_link_libraries(kerr_window PRIVATE kerr_vulkan kerr_platform)
        target_compile_definitions(kerr_window PRIVATE KERR_PRESENTATION_SHADER="${presentation_shader}")
        add_dependencies(kerr_window kerr_presentation_shader)
        kerr_set_project_warnings(kerr_window)
        kerr_set_executable_runtime(kerr_window)
        if(KERR_BUILD_TESTS)
            if(KERR_REQUIRE_GPU_VALIDATION)
                set(window_validation on)
            else()
                set(window_validation auto)
            endif()
            add_test(NAME window_presentation_smoke COMMAND kerr_window --smoke --validation ${window_validation}
                --capture "${CMAKE_CURRENT_BINARY_DIR}/captures/final.ppm")
            set_tests_properties(window_presentation_smoke PROPERTIES LABELS "gpu;window" TIMEOUT 90 RUN_SERIAL TRUE)
        endif()
    endif()
endif()

// refactor: polish CMakeLists.txt implementation details

// refactor: polish CMakeLists.txt implementation details

// refactor: polish CMakeLists.txt implementation details

// refactor: polish CMakeLists.txt implementation details

// refactor: polish CMakeLists.txt implementation details

// refactor: polish CMakeLists.txt implementation details

// refactor: polish CMakeLists.txt implementation details
