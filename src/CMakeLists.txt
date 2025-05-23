option(WINDOW "Define WINDOW macro, showing the intro in a small window instead of full screen" OFF)
option(OPENGL_DEBUG "Show OpenGL errors in a message box" OFF)
option(VALIDATE_SHADERS "Validate shaders using glslangValidator" ON)
option(SYNC "Build version for rocket syncing" OFF)
option(CAPTURE "Build version for capturing video" OFF)
set(COMPRESS_LEVEL "medium" CACHE STRING "Crinkler compression level: off, light, medium, heavy")
set_property(CACHE COMPRESS_LEVEL PROPERTY STRINGS OFF LIGHT MEDIUM HEAVY)
if (NOT XRES)
    set(XRES 1920)
endif()
if (NOT YRES)
    set(YRES 1080)
endif()
 
message(STATUS "Configuration options (configure with cmake -D...): WINDOW=${WINDOW} OPENGL_DEBUG=${OPENGL_DEBUG} VALIDATE_SHADERS=${VALIDATE_SHADERS} COMPRESS_LEVEL=${COMPRESS_LEVEL} XRES=${XRES} YRES=${YRES} SYNC=${SYNC} CAPTURE=${CAPTURE}")

# Compile the Sointu song
find_program(GO NAMES go)
if(NOT GO)
  message(FATAL_ERROR "Go not found. Get Go from: https://golang.org")
else()
  message(STATUS "Go found at: ${GO}")
endif()

set(ARCH "386") # force 32-bit output, even on 64-bit Windows
set(SONG_ASM song.asm)
set(SONG_YML ../data/song.yml)
set(SOINTU_CMD ${GO} run "github.com/vsariola/sointu/cmd/sointu-compile@v0.4.0")
add_custom_command(
    OUTPUT ${SONG_ASM} 
    COMMAND ${SOINTU_CMD} -arch=${ARCH} -o ${CMAKE_CURRENT_BINARY_DIR}/${SONG_ASM} ${CMAKE_CURRENT_SOURCE_DIR}/${SONG_YML}
    DEPENDS ${SONG_YML}
)

# Generate the minified inlined shaders        

find_program(SHADER_MINIFIER NAMES shader_minifier)
if(NOT SHADER_MINIFIER)
  message(FATAL_ERROR "shader_minifier not found. Get shader_minifier from: https://github.com/laurentlb/Shader_Minifier")
else()
  message(STATUS "shader_minifier found at: ${SHADER_MINIFIER}")
endif()

set(SHADER_FRAG ../data/shader.frag)    
set(SHADER_CONFIGURED_FRAG shader_configured.frag)    
set(SHADER_SYNC_FRAG shader_sync.frag)    
set(SHADER_MINIFIED_INL shader.inl)    
configure_file(
    ${SHADER_FRAG}
    ${SHADER_CONFIGURED_FRAG}
)    
add_custom_command(
    OUTPUT ${SHADER_MINIFIED_INL}
    COMMAND ${SHADER_MINIFIER} -o ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_MINIFIED_INL} ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_SYNC_FRAG}
    DEPENDS ${SHADER_SYNC_FRAG}
)    
set(SHADERS ${SHADER_MINIFIED_INL})
set(SHADER_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR})    

# Generate rocket .asm

find_program(PYTHON NAMES python python3)
if(NOT PYTHON)
  message(FATAL_ERROR "Python not found. Get Python from: https://www.python.org/")
else()
  message(STATUS "Python found at: ${PYTHON}")
endif()

set(SYNCS_ROCKET data/syncs.rocket)    
set(ROCKET_OUTPUT minirocket.h minirocket.inc ${SHADER_SYNC_FRAG} minirocket_tracknames.h)    
add_custom_command(
    OUTPUT ${ROCKET_OUTPUT}
    COMMAND ${PYTHON} ${PROJECT_SOURCE_DIR}/data/convert.py -o ${CMAKE_CURRENT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/${SYNCS_ROCKET} ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_CONFIGURED_FRAG}
    DEPENDS ${PROJECT_SOURCE_DIR}/${SYNCS_ROCKET} ${SHADER_CONFIGURED_FRAG}
)    

# intro

add_executable(intro WIN32 main.cpp sync.cpp sync.h minirocket.asm ${SONG_ASM} ${SHADERS} minirocket.h minirocket.inc minirocket_tracknames.h bitmap.h bitmap.cpp)
set_source_files_properties(minirocket.asm OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/minirocket.inc)
set_property(TARGET intro APPEND PROPERTY LINK_FLAGS " /ENTRY:entrypoint")                      
target_compile_definitions(intro PUBLIC XRES=${XRES} YRES=${YRES})
target_link_libraries(intro rkt_sync opengl32.lib winmm.lib libucrt.lib dsound.lib ws2_32.lib ucrt.lib)
target_include_directories(intro PUBLIC ${SHADER_INCLUDE_DIR}) # for the shaders
target_include_directories(intro PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}) # for nasm to find 4klang
target_include_directories(intro PUBLIC ${CMAKE_CURRENT_BINARY_DIR}) # for the sync header

set(exesuffix "")
if (NOT YRES EQUAL "1080")
    set(exesuffix "${exesuffix}-${YRES}")
endif()
if (WINDOW)
    set(exesuffix "${exesuffix}-window")
    message("Defining WINDOW macro")
    target_compile_definitions(intro PUBLIC WINDOW)    
endif()
if (DEBUG)    
    set(exesuffix "${exesuffix}-debug")
    message("Defining DEBUG_INTRO macro")
    target_compile_definitions(intro PUBLIC DEBUG_INTRO)        
endif()
if (SYNC)    
    set(exesuffix "${exesuffix}-sync")
    message("Defining SYNC macro")
    add_compile_definitions(SYNC)
    set(COMPRESS_LEVEL OFF)
    set(CAPTURE OFF)    
endif()
if (CAPTURE)    
    set(exesuffix "${exesuffix}-capture")
    message("Defining CAPTURE macro")
    add_compile_definitions(CAPTURE)
    set(COMPRESS_LEVEL OFF)
endif()

set_property(TARGET intro APPEND PROPERTY COMPILE_FLAGS " /Os /O1 /Ob1 /Oi /Oy /GS- /arch:IA32 /fp:fast")
if(NOT COMPRESS_LEVEL STREQUAL OFF)    
    find_program(CRINKLER NAMES Crinkler)
    if (NOT CRINKLER)
        message(WARNING "Crinkler not found. Cannot compress executable; using default linker. Get Crinkler from https://github.com/runestubbe/Crinkler & put it in path (as Crinkler.exe)")            
        set(COMPRESS_LEVEL OFF)
    endif()
endif()

if (NOT COMPRESS_LEVEL STREQUAL OFF)
    message(STATUS "Crinkler found at: ${CRINKLER}")      
    set(CRINKLER_FLAGS "/ENTRY:entrypoint /RANGE:opengl32 /PROGRESSGUI /UNSAFEIMPORT /UNALIGNCODE /HASHSIZE:100 /REPORT:crinkler_report.html /EXPORT:NvOptimusEnablement=1 /EXPORT:AmdPowerXpressRequestHighPerformance=1")
    # TBD: do we add /SATURATE
    if (COMPRESS_LEVEL STREQUAL LIGHT) 
        set(CRINKLER_FLAGS "${CRINKLER_FLAGS} /HASHTRIES:100 /COMPMODE:INSTANT /ORDERTRIES:2000")
        set(exesuffix "${exesuffix}-light")
    elseif (COMPRESS_LEVEL STREQUAL HEAVY)
        set(CRINKLER_FLAGS "${CRINKLER_FLAGS} /HASHTRIES:1000 /COMPMODE:VERYSLOW /ORDERTRIES:30000")              
    else()
        set(CRINKLER_FLAGS "${CRINKLER_FLAGS} /HASHTRIES:300 /COMPMODE:SLOW /ORDERTRIES:9000")
        set(exesuffix "${exesuffix}-medium")
    endif()   
    
    # we drop the whole manifest creation from the front; did not find a way to disable it from CMake otherwise        
    set (CMAKE_CXX_LINK_EXECUTABLE "${CRINKLER} <OBJECTS> /out:<TARGET> ${CRINKLER_FLAGS} <LINK_LIBRARIES>")
else()
    set(exesuffix "${exesuffix}-uncompressed")
    target_compile_definitions(intro PUBLIC UNCOMPRESSED)
endif()

add_custom_command(TARGET intro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:intro> ${PROJECT_SOURCE_DIR}/dist/dipping-deep-dub-drop${exesuffix}.exe
)

if (VALIDATE_SHADERS)
    find_program(GLSLANGVALIDATOR NAMES glslangValidator)
    if (NOT GLSLANGVALIDATOR)
        message(WARNING "glslangValidator not found. Cannot validate shaders. Get glslangValidator from: https://github.com/KhronosGroup/glslang")            
        set(VALIDATE_SHADERS OFF)
    endif()
endif()

if (VALIDATE_SHADERS)
    add_custom_command(TARGET intro POST_BUILD COMMAND ${GLSLANGVALIDATOR} ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_SYNC_FRAG})    
endif()
