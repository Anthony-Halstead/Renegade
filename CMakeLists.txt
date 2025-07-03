cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME PPIV)
project(${PROJECT_NAME})

# Precompiled headers for faster builds
set(PRE_COMPILED
    ./Source/precompiled.h 
)

# Explicitly specify critical files first to guarantee correct static initialization order
set(EXPLICIT_SOURCES
    ./Source/CCL.cpp
    ./Source/Audio/AudioManager.cpp
)

# Glob all remaining source files
file(GLOB_RECURSE OTHER_SOURCE_FILES CONFIGURE_DEPENDS
    ./Source/*.h
    ./Source/*.hpp
    ./Source/*.cpp
    ./Shaders/*.hlsl
    ./gateware-main/Gateware.h
    ./entt-3.13.1/single_include/entt/entt.hpp
    ./defaults.ini
    ./inifile-cpp-master/include/inicpp.h
)

# Remove explicitly listed files to prevent duplicates
list(REMOVE_ITEM OTHER_SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/CCL.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Audio/AudioManager.cpp"
)

# Now, explicitly add the critical files first, followed by the rest
add_executable(${PROJECT_NAME}
    ${EXPLICIT_SOURCES}
    ${OTHER_SOURCE_FILES}
)

# Force Unicode for some libraries on Windows
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

if (WIN32)
    # Set the default startup project
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

    source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${EXPLICIT_SOURCES} ${OTHER_SOURCE_FILES})

    target_include_directories(${PROJECT_NAME} PUBLIC $ENV{VULKAN_SDK}/Include/)
    target_link_directories(${PROJECT_NAME} PUBLIC $ENV{VULKAN_SDK}/Lib/)

    target_compile_options(${PROJECT_NAME} PRIVATE "/MD")
endif(WIN32)

if(UNIX AND NOT APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lX11 -lshaderc_combined")
    find_package(X11)
    find_package(Vulkan REQUIRED)
    link_libraries(${X11_LIBRARIES})
    include_directories(${X11_INCLUDE_DIR})
    include_directories(${Vulkan_INCLUDE_DIR}) 

    link_libraries(${Vulkan_LIBRARIES})
    link_libraries(/usr/lib/x86_64-linux-gnu/libshaderc_combined.a)

    source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${EXPLICIT_SOURCES} ${OTHER_SOURCE_FILES})
endif(UNIX AND NOT APPLE)

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fmodules -fcxx-modules")
    set(Architecture ${CMAKE_OSX_ARCHITECTURES})
    find_package(Vulkan REQUIRED)
    include_directories(${Vulkan_INCLUDE_DIR}) 

    link_libraries(${Vulkan_LIBRARIES})
    link_libraries(/usr/local/lib/libshaderc_combined.a)

    source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${EXPLICIT_SOURCES} ${OTHER_SOURCE_FILES})
endif(APPLE)

# EnTT requires C++17
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# Add precompiled headers
target_precompile_headers(${PROJECT_NAME} PRIVATE ${PRE_COMPILED})

# Disable shader compilation as they may contain Vulkan specifics
file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
    ./Shaders/*.hlsl
)
set_source_files_properties(${SHADER_FILES} PROPERTIES
    VS_SHADER_TYPE Vertex
    VS_SHADER_MODEL 5.1
    VS_SHADER_ENTRYPOINT main
    VS_TOOL_OVERRIDE "None"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM USES_TERMINAL
)
