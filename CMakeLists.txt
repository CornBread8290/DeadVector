cmake_minimum_required(VERSION 3.31)
project(Dead_Vector_2 C)

set(CMAKE_C_STANDARD 11)

# Sources
set(SRCS
        0Main.c 2Audio.c 3Utils.c 4Font.c
        gl.c wgl.c
        1Mesh.c
        1Mesh.h
)

# WIN32 removes the console window like -mwindows
add_executable(Dead_Vector_2 WIN32 ${SRCS}
        1Mesh.c
        1Mesh.h)

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
