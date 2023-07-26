#-------------------------------------------------------------------
# This file is stolen from part of the CMake build system for OGRE (Object-oriented Graphics Rendering Engine) http://www.ogre3d.org/
#
# The contents of this file are placed in the public domain. Feel
# free to make use of it in any way you like.
#-------------------------------------------------------------------

# - Try to find OpenGLES and EGL
# Once done this will define
#
#  OPENGLES3_FOUND        - system has OpenGLES
#  OPENGLES3_INCLUDE_DIR  - the GL include directory
#  OPENGLES3_LIBRARIES    - Link these to use OpenGLES
#
#  EGL_FOUND        - system has EGL
#  EGL_INCLUDE_DIR  - the EGL include directory
#  EGL_LIBRARIES    - Link these to use EGL

# Win32, Apple, and Android are not tested!
# Linux tested and works

if(WIN32)
	if(CYGWIN)
		find_path(OPENGLES3_INCLUDE_DIR GLES3/gl3.h)
		find_library(OPENGLES2_LIBRARY libGLESv2)
	else()
		if(BORLAND)
			set(OPENGLES2_LIBRARY import32 CACHE STRING "OpenGL ES 3.x library for Win32")
		else()
			# TODO
			# set(OPENGLES_LIBRARY ${SOURCE_DIR}/Dependencies/lib/release/libGLESv3.lib CACHE STRING "OpenGL ES 3.x library for win32"
		endif()
	endif()
elseif(APPLE)
	create_search_paths(/Developer/Platforms)
	findpkg_framework(OpenGLES3)
	set(OPENGLES2_LIBRARY "-framework OpenGLES")
else()
	find_path(OPENGLES3_INCLUDE_DIR GLES3/gl3.h
		PATHS /usr/openwin/share/include
			/opt/graphics/OpenGL/include
			/opt/vc/include
			/usr/X11R6/include
			/usr/include
	)

	find_library(OPENGLES2_LIBRARY ## GLES 3 api is provided by glesv2 lib
		NAMES GLESv2
		PATHS /opt/graphics/OpenGL/lib
			/usr/openwin/lib
			/usr/shlib /usr/X11R6/lib
			/opt/vc/lib
			/usr/lib/aarch64-linux-gnu
			/usr/lib/arm-linux-gnueabihf
			/usr/lib
	)

	if(NOT BUILD_ANDROID)
		find_path(EGL_INCLUDE_DIR EGL/egl.h
			PATHS /usr/openwin/share/include
				/opt/graphics/OpenGL/include
				/opt/vc/include
				/usr/X11R6/include
				/usr/include
		)

		find_library(EGL_LIBRARY
			NAMES EGL
			PATHS /opt/graphics/OpenGL/lib
				/usr/openwin/lib
				/usr/shlib
				/usr/X11R6/lib
				/opt/vc/lib
				/usr/lib/aarch64-linux-gnu
				/usr/lib/arm-linux-gnueabihf
				/usr/lib
		)

		# On Unix OpenGL usually requires X11.
		# It doesn't require X11 on OSX.

		# if(OPENGLES2_LIBRARY)
		# 	if(NOT X11_FOUND)
		# 		include(FindX11)
		# 	endif()
		# 	if(X11_FOUND)
		# 		set(OPENGLES3_LIBRARIES ${X11_LIBRARIES})
		# 	endif()
		# endif()
	endif()
endif()

set(OPENGLES3_LIBRARIES ${OPENGLES3_LIBRARIES} ${OPENGLES2_LIBRARY} ${OPENGLES1_gl_LIBRARY})

if(BUILD_ANDROID)
	if(OPENGLES2_LIBRARY)
		set(EGL_LIBRARIES)
		set(OPENGLES3_FOUND TRUE)
	endif()
else()
	if(OPENGLES2_LIBRARY AND EGL_LIBRARY)
		set(OPENGLES3_LIBRARIES ${OPENGLES2_LIBRARY} ${OPENGLES3_LIBRARIES})
		set(EGL_LIBRARIES ${EGL_LIBRARY} ${EGL_LIBRARIES})
		set(OPENGLES3_FOUND TRUE)
	endif()
endif()

mark_as_advanced(
	OPENGLES3_INCLUDE_DIR
	OPENGLES2_LIBRARY
	# OPENGLES1_gl_LIBRARY
	EGL_INCLUDE_DIR
	EGL_LIBRARY
)

if(OPENGLES3_FOUND)
	message(STATUS "Found system OpenGL ES 3 library: ${OPENGLES3_LIBRARIES}")
	#MARK_AS_ADVANCED(OPENGLES3_FOUND)
else()
	set(OPENGLES3_LIBRARIES "")
endif()

# Handle the results of the library search
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenGLES3 DEFAULT_MSG OPENGLES2_LIBRARY OPENGLES3_INCLUDE_DIR EGL_INCLUDE_DIR EGL_LIBRARY)
