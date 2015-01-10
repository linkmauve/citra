cmake_minimum_required(VERSION 2.6)

project(libepoxy)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
add_definitions(-std=c99)

set(top_srcdir ${CMAKE_CURRENT_SOURCE_DIR}/libepoxy)
set(top_builddir ${CMAKE_CURRENT_BINARY_DIR})
set(srcdir ${top_srcdir}/src)
set(builddir ${top_builddir}/src)

make_directory(${top_builddir}/include/epoxy)
make_directory(${top_builddir}/src)

foreach(REGISTRY gl egl glx wgl)
    execute_process(COMMAND python3 ${srcdir}/gen_dispatch.py --dir ${top_builddir} ${top_srcdir}/registry/${REGISTRY}.xml)
endforeach()

include_directories(${top_srcdir}/include)

set(SRCS ${srcdir}/dispatch_common.c ${builddir}/gl_generated_dispatch.c)

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    if (WIN32)
        set(SRCS ${SRCS} ${srcdir}/dispatch_wgl.c ${builddir}/wgl_generated_dispatch.c)
    elseif (NOT APPLE)
        find_package(X11)
        if (NOT X11_FOUND)
            set(SRCS ${SRCS} ${srcdir}/dispatch_glx.c ${builddir}/glx_generated_dispatch.c)
        endif()
        set(SRCS ${SRCS} ${srcdir}/dispatch_egl.c ${builddir}/egl_generated_dispatch.c)
    endif()
endif()

add_library(epoxy STATIC ${SRCS})
