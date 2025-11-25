cmake_minimum_required(VERSION 3.31)
project(Dead_Vector_2 C)

set(CMAKE_C_STANDARD 11)

# Sources
set(SRCS
        0Main.c 1Audio.c 2Utils.c 3Font.c
        gl.c wgl.c
)

# WIN32 removes the console window like -mwindows
add_executable(Dead_Vector_2 WIN32 ${SRCS})

target_include_directories(Dead_Vector_2 PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_compile_options(Dead_Vector_2 PRIVATE
        $<$<CONFIG:Release>:
        -Os -fmerge-constants -ffunction-sections -fdata-sections
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -fno-exceptions -fno-stack-protector
        -fomit-frame-pointer -fno-ident -fstack-usage
        >
)

# Linker flags
target_link_options(Dead_Vector_2 PRIVATE
        $<$<CONFIG:Release>:
        -Wl,--gc-sections -Wl,--strip-all
        >
)

# Libraries (MinGW toolchain)
target_link_libraries(Dead_Vector_2 PRIVATE winmm opengl32)

find_program(UPX_EXECUTABLE upx)
if (UPX_EXECUTABLE)
    add_custom_target(Pack_with_UPX
            COMMAND ${UPX_EXECUTABLE} --best --lzma $<TARGET_FILE:Dead_Vector_2>
            DEPENDS Dead_Vector_2
            COMMENT "Packing Dead_Vector_2 with UPX"
            VERBATIM
    )
endif()

add_custom_target(Compile_And_Run
        COMMAND $<TARGET_FILE:Dead_Vector_2>
        DEPENDS Dead_Vector_2
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Build then run Dead_Vector_2 (unpacked)"
)

if (UPX_EXECUTABLE)
    add_custom_target(Build_And_Run_Packed
            COMMAND $<TARGET_FILE:Dead_Vector_2>
            DEPENDS Pack_with_UPX
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Build, pack with UPX, then run"
    )
endif()
