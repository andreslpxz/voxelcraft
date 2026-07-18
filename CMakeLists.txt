cmake_minimum_required(VERSION 3.22.1)

project(VoxelCraft LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------
# Librerías del sistema
# ------------------------------------------------------------
find_library(log-lib log REQUIRED)
find_library(android-lib android REQUIRED)
find_library(vulkan-lib vulkan REQUIRED)
find_library(nativewindow-lib nativewindow REQUIRED)

# ------------------------------------------------------------
# Native App Glue
# ------------------------------------------------------------
set(NATIVE_APP_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")

if(NOT EXISTS "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c")
    message(FATAL_ERROR
        "No se encontró android_native_app_glue en:\n${NATIVE_APP_GLUE_DIR}")
endif()

message(STATUS "Native app glue encontrado en: ${NATIVE_APP_GLUE_DIR}")

add_library(native_app_glue STATIC)

target_sources(native_app_glue PRIVATE
    ${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c
)

target_include_directories(native_app_glue PUBLIC
    ${NATIVE_APP_GLUE_DIR}
)

set_source_files_properties(
    ${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c
    PROPERTIES LANGUAGE C
)

# ------------------------------------------------------------
# Includes
# ------------------------------------------------------------
target_include_directories(native_app_glue PUBLIC
    ${NATIVE_APP_GLUE_DIR}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/third_party
    ${NATIVE_APP_GLUE_DIR}
)

# ------------------------------------------------------------
# Código fuente
# ------------------------------------------------------------
file(GLOB_RECURSE ENGINE_SRC CONFIGURE_DEPENDS src/engine/*.cpp)
file(GLOB_RECURSE WORLD_SRC  CONFIGURE_DEPENDS src/world/*.cpp)
file(GLOB_RECURSE GAME_SRC   CONFIGURE_DEPENDS src/game/*.cpp)
file(GLOB_RECURSE NOISE_SRC  CONFIGURE_DEPENDS src/noise/*.cpp)
file(GLOB_RECURSE MATH_SRC   CONFIGURE_DEPENDS src/math/*.cpp)

set(MAIN_SRC
    src/main.cpp
)

add_library(voxelcraft SHARED
    ${MAIN_SRC}
    ${ENGINE_SRC}
    ${WORLD_SRC}
    ${GAME_SRC}
    ${NOISE_SRC}
    ${MATH_SRC}
)

# ------------------------------------------------------------
# Opciones de compilación
# ------------------------------------------------------------
target_compile_options(voxelcraft PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O2
    -fvisibility=hidden
)

target_link_options(voxelcraft PRIVATE
    "-Wl,--export-dynamic"
)

# ------------------------------------------------------------
# Shaders
# ------------------------------------------------------------
find_program(GLSLC glslc)

if(GLSLC)

    set(SHADER_DIR ${CMAKE_SOURCE_DIR}/src/shaders)
    set(GEN_DIR ${CMAKE_BINARY_DIR}/generated_shaders)

    file(MAKE_DIRECTORY ${GEN_DIR})

    # Tus add_custom_command() aquí

    target_include_directories(voxelcraft PRIVATE
        ${GEN_DIR}
    )

    target_compile_definitions(voxelcraft PRIVATE
        VOXELCRAFT_EMBEDDED_SHADERS=1
    )

endif()

# ------------------------------------------------------------
# Enlace
# ------------------------------------------------------------
target_link_libraries(voxelcraft
    PRIVATE

    "-Wl,--whole-archive"
    native_app_glue
    "-Wl,--no-whole-archive"

    ${vulkan-lib}
    ${android-lib}
    ${nativewindow-lib}
    ${log-lib}
    EGL
)

# ------------------------------------------------------------
# Assets
# ------------------------------------------------------------
file(COPY
    ${CMAKE_SOURCE_DIR}/assets
    DESTINATION ${CMAKE_BINARY_DIR}
)
