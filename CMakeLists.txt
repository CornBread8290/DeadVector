cmake_minimum_required(VERSION 3.31)
project(Dead_Vector_2 C)

set(CMAKE_C_STANDARD 11)

# Sources
set(SRCS
        0Main.c 2Audio.c 3Utils.c 4Font.c 5Speech.c 6Volume.c 7Physics.c 8Mesh.c
        gl.c wgl.c
)

# WIN32 removes the console window like -mwindows
add_executable(Dead_Vector_2 WIN32 ${SRCS})

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/shaders/*.vert
        ${CMAKE_SOURCE_DIR}/shaders/*.frag
        ${CMAKE_SOURCE_DIR}/shaders/*.comp
)

set(SHADER_HEADER ${CMAKE_BINARY_DIR}/generated/shaders_embed.h)
add_custom_command(
        OUTPUT ${SHADER_HEADER}
        COMMAND ${CMAKE_COMMAND}
        -DSHADER_DIR=${CMAKE_SOURCE_DIR}/shaders
        -DOUT_FILE=${SHADER_HEADER}
        -P ${CMAKE_SOURCE_DIR}/cmake/gen_shaders.cmake
        DEPENDS ${SHADER_SOURCES} ${CMAKE_SOURCE_DIR}/cmake/gen_shaders.cmake
        COMMENT "Baking shaders into shaders_embed.h"
        VERBATIM
)
add_custom_target(shaders_embed DEPENDS ${SHADER_HEADER})
add_dependencies(Dead_Vector_2 shaders_embed)

target_include_directories(Dead_Vector_2 PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(Dead_Vector_2 PRIVATE $<$<CONFIG:Release>:EMBED_SHADERS>)

add_custom_command(TARGET Dead_Vector_2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Release>,true,copy_directory>
        $<$<NOT:$<CONFIG:Release>>:${CMAKE_SOURCE_DIR}/shaders>
        $<$<NOT:$<CONFIG:Release>>:$<TARGET_FILE_DIR:Dead_Vector_2>/shaders>
        COMMENT "Staging shaders next to the executable (non-Release)"
        VERBATIM
)

target_link_options(Dead_Vector_2 PRIVATE
        $<$<CONFIG:Release>:-Wl,-Map=deadvec.map>
)

target_include_directories(Dead_Vector_2 PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_compile_options(Dead_Vector_2 PRIVATE
        $<$<CONFIG:Release>:
        -Os
        -ffunction-sections -fdata-sections
        -fmerge-constants
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -fno-stack-protector
        -fno-ident
        -flto
        -fstack-usage
        >
)

# Linker flags
target_link_options(Dead_Vector_2 PRIVATE
        $<$<CONFIG:Release>:
        -Wl,--gc-sections
        -Wl,--strip-all
        -Wl,--no-insert-timestamp
        -flto
        >
)

# Libraries (MinGW toolchain)
target_link_libraries(Dead_Vector_2 PRIVATE winmm opengl32)

find_program(UPX_EXECUTABLE
        NAMES upx upx.exe
        HINTS
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}
)

if (UPX_EXECUTABLE)
    add_custom_command(TARGET Dead_Vector_2 POST_BUILD
            COMMAND "${UPX_EXECUTABLE}" --best --lzma "$<TARGET_FILE:Dead_Vector_2>"
            COMMENT "Packing Dead_Vector_2 with UPX (Release)"
            VERBATIM
    )
endif()
