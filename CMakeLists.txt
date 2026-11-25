cmake_minimum_required(VERSION 3.24)

project(SpriteEd VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SPRITED_BUILD_APP "Build the Dear ImGui desktop application" ON)
option(SPRITED_ENABLE_VIEWPORTS "Enable Dear ImGui multi-viewports by default" ON)
option(SPRITED_USE_FETCHCONTENT "Download missing third-party dependencies with FetchContent" ON)

include(CTest)
if(BUILD_TESTING)
    add_executable(GridExtractorTests
        tests/grid_extractor_test.cpp
        src/image_ops.cpp
        src/sprite_sheet_extractor.cpp
    )
    target_include_directories(GridExtractorTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann_json/include
    )
    add_executable(SpriteDeleteTests
        tests/sprite_delete_test.cpp
    )
    add_executable(AnimationPreviewTests
        tests/animation_preview_test.cpp
    )
    add_executable(AnimationExportTests
        tests/animation_export_test.cpp
        src/animation_export.cpp
        src/image_ops.cpp
    )
    target_include_directories(AnimationExportTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/gif-h
    )
    add_executable(SpriteExportTests
        tests/sprite_export_test.cpp
        src/image_ops.cpp
    )
    add_executable(HoverPixelTests
        tests/hover_pixel_test.cpp
    )
    add_executable(CanvasInfoOverlayTests
        tests/canvas_info_overlay_test.cpp
    )
    add_executable(SpriteSheetBuilderTests
        tests/sprite_sheet_builder_test.cpp
        src/sprite_sheet_builder.cpp
    )
    add_executable(SpriteLayoutTests
        tests/sprite_layout_test.cpp
        src/sprite_layout.cpp
        src/sprite_sheet_builder.cpp
        src/image_ops.cpp
    )
    if(MSVC)
        target_compile_options(GridExtractorTests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(SpriteDeleteTests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(AnimationPreviewTests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(AnimationExportTests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(SpriteExportTests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(HoverPixelTests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(CanvasInfoOverlayTests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(SpriteSheetBuilderTests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(SpriteLayoutTests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(GridExtractorTests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(SpriteDeleteTests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(AnimationPreviewTests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(AnimationExportTests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(SpriteExportTests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(HoverPixelTests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(CanvasInfoOverlayTests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(SpriteSheetBuilderTests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(SpriteLayoutTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME GridExtractorTests COMMAND GridExtractorTests)
    add_test(NAME SpriteDeleteTests COMMAND SpriteDeleteTests)
    add_test(NAME AnimationPreviewTests COMMAND AnimationPreviewTests)
    add_test(NAME AnimationExportTests COMMAND AnimationExportTests)
    add_test(NAME SpriteExportTests COMMAND SpriteExportTests)
    add_test(NAME HoverPixelTests COMMAND HoverPixelTests)
    add_test(NAME CanvasInfoOverlayTests COMMAND CanvasInfoOverlayTests)
    add_test(NAME SpriteSheetBuilderTests COMMAND SpriteSheetBuilderTests)
    add_test(NAME SpriteLayoutTests COMMAND SpriteLayoutTests)
endif()

if(SPRITED_BUILD_APP)
    include(FetchContent)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    set(SPRITED_EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)
    set(SPRITED_LOCAL_GLFW_DIR ${SPRITED_EXTERNAL_DIR}/glfw)
    set(SPRITED_LOCAL_JSON_DIR ${SPRITED_EXTERNAL_DIR}/nlohmann_json)
    set(SPRITED_LOCAL_IMGUI_DIR ${SPRITED_EXTERNAL_DIR}/imgui)
    set(SPRITED_LOCAL_STB_DIR ${SPRITED_EXTERNAL_DIR}/stb)

    if(EXISTS ${SPRITED_LOCAL_GLFW_DIR}/CMakeLists.txt)
        message(STATUS "Using local GLFW: ${SPRITED_LOCAL_GLFW_DIR}")
        add_subdirectory(${SPRITED_LOCAL_GLFW_DIR} external/glfw-build)
    else()
        if(NOT SPRITED_USE_FETCHCONTENT)
            message(FATAL_ERROR "Missing external/glfw. Enable SPRITED_USE_FETCHCONTENT or place GLFW source at external/glfw.")
        endif()
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
        )
        FetchContent_MakeAvailable(glfw)
    endif()

    if(EXISTS ${SPRITED_LOCAL_JSON_DIR}/CMakeLists.txt)
        message(STATUS "Using local nlohmann_json: ${SPRITED_LOCAL_JSON_DIR}")
        add_subdirectory(${SPRITED_LOCAL_JSON_DIR} external/nlohmann_json-build)
    else()
        if(NOT SPRITED_USE_FETCHCONTENT)
            message(FATAL_ERROR "Missing external/nlohmann_json. Enable SPRITED_USE_FETCHCONTENT or place nlohmann/json source at external/nlohmann_json.")
        endif()
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
        )
        FetchContent_MakeAvailable(nlohmann_json)
    endif()

    if(EXISTS ${SPRITED_LOCAL_IMGUI_DIR}/imgui.cpp)
        message(STATUS "Using local Dear ImGui: ${SPRITED_LOCAL_IMGUI_DIR}")
        set(imgui_SOURCE_DIR ${SPRITED_LOCAL_IMGUI_DIR})
    else()
        if(NOT SPRITED_USE_FETCHCONTENT)
            message(FATAL_ERROR "Missing external/imgui. Enable SPRITED_USE_FETCHCONTENT or place Dear ImGui source at external/imgui.")
        endif()
        FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.91.9b-docking
        )
        FetchContent_GetProperties(imgui)
        if(NOT imgui_POPULATED)
            FetchContent_Populate(imgui)
        endif()
    endif()

    if(EXISTS ${SPRITED_LOCAL_STB_DIR}/stb_image.h)
        message(STATUS "Using local stb: ${SPRITED_LOCAL_STB_DIR}")
        set(stb_SOURCE_DIR ${SPRITED_LOCAL_STB_DIR})
    else()
        if(NOT SPRITED_USE_FETCHCONTENT)
            message(FATAL_ERROR "Missing external/stb. Enable SPRITED_USE_FETCHCONTENT or place stb headers at external/stb.")
        endif()
        FetchContent_Declare(
            stb
            GIT_REPOSITORY https://github.com/nothings/stb.git
            GIT_TAG master
        )
        FetchContent_GetProperties(stb)
        if(NOT stb_POPULATED)
            FetchContent_Populate(stb)
        endif()
    endif()

    find_package(OpenGL REQUIRED)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )

    target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

    add_executable(SpriteEd
        src/main.cpp
        src/animation_export.cpp
        src/image_ops.cpp
        src/sprite_layout.cpp
        src/sprite_sheet_builder.cpp
        src/sprite_sheet_extractor.cpp
    )

    target_compile_definitions(SpriteEd PRIVATE
        $<$<BOOL:${SPRITED_ENABLE_VIEWPORTS}>:SPRITED_ENABLE_VIEWPORTS=1>
    )

    target_include_directories(SpriteEd PRIVATE
        ${stb_SOURCE_DIR}
        ${SPRITED_EXTERNAL_DIR}/gif-h
    )

    target_link_libraries(SpriteEd PRIVATE
        imgui
        glfw
        OpenGL::GL
        nlohmann_json::nlohmann_json
    )

    if(WIN32)
        target_link_libraries(SpriteEd PRIVATE
            comdlg32
        )
    endif()

    if(MSVC)
        target_compile_options(SpriteEd PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(SpriteEd PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
