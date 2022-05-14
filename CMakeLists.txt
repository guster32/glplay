cmake_minimum_required(VERSION 3.10)

#set(CMAKE_CXX_COMPILER /usr/bin/clang++) # Yocto does not suppurt clang natively
#set(CMAKE_CXX_COMPILER /usr/bin/g++)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17") # ToDebug with CLang add '-fstandalone-debug' option
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

if(POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
endif()


project(OpenGLTutorial)

find_package(PkgConfig)
find_package(PkgConfig REQUIRED)
find_package(OpenGLES2 REQUIRED)

set(ALL_LIBS
	${OPENGL_LIBRARY}
)

include_directories(${CMAKE_SOURCE_DIR})

add_definitions(
	-DTW_STATIC
	-DTW_NO_LIB_PRAGMA
	-DTW_NO_DIRECT3D
	-DGLEW_STATIC
	-D_CRT_SECURE_NO_WARNINGS
)

# Tutorial 1
add_executable(glplay 
glplay/glplay.cpp
)
target_link_libraries(glplay
	${ALL_LIBS}
)

install(TARGETS glplay RUNTIME DESTINATION bin)