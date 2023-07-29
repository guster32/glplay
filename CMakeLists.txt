cmake_minimum_required(VERSION 3.10)

#set(CMAKE_CXX_COMPILER /usr/bin/clang++) # Yocto does not support clang natively
# set(CMAKE_C_COMPILER /usr/bin/clang)
#set(CMAKE_CXX_COMPILER /usr/bin/g++)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17") # ToDebug with CLang add '-fstandalone-debug' option
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

if(POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
endif()


project(Glplay)

find_package(PkgConfig)
find_package(PkgConfig REQUIRED)
find_package(OpenGLES REQUIRED)
find_package(DRM REQUIRED)
find_package(GBM REQUIRED)

set(ALL_LIBS
	${DRM_LIBRARY}
	${GBM_LIBRARIES}
	${EGL_LIBRARIES}
	${GLES_LIB}
)

add_definitions(
	-d__GBM__
	-DTW_STATIC
	-DTW_NO_LIB_PRAGMA
	-DTW_NO_DIRECT3D
	-DGLEW_STATIC
	-D_CRT_SECURE_NO_WARNINGS
)

file(GLOB GLPLAY_SOURCES ${PROJECT_SOURCE_DIR}/src/glplay/*.cpp)
file(GLOB DRM_SOURCES ${PROJECT_SOURCE_DIR}/src/drm/*.cpp)
file(GLOB KMS_SOURCES ${PROJECT_SOURCE_DIR}/src/kms/*.cpp)
file(GLOB NIX_SOURCES ${PROJECT_SOURCE_DIR}/src/nix/*.cpp)
file(GLOB GBM_SOURCES ${PROJECT_SOURCE_DIR}/src/gbm/*.cpp)
file(GLOB EGL_SOURCES ${PROJECT_SOURCE_DIR}/src/egl/*.cpp)
file(GLOB GSL_INCLUDES ${PROJECT_SOURCE_DIR}/third-party/gsl/*)

add_executable(glplay
	${GLPLAY_SOURCES}
	${DRM_SOURCES}
	${KMS_SOURCES}
	${NIX_SOURCES}
	${GBM_SOURCES}
	${EGL_SOURCES}
	${GSL_INCLUDES}
)
target_link_libraries(glplay ${ALL_LIBS})

install(TARGETS glplay RUNTIME DESTINATION bin)