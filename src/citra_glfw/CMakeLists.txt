set(SRCS
            emu_window/emu_window_glfw.cpp
            citra.cpp
            config.cpp
            )
set(HEADERS
            emu_window/emu_window_glfw.h
            config.h
            default_ini.h
            )

create_directory_groups(${SRCS} ${HEADERS})

include_directories(${GLFW_INCLUDE_DIRS})

add_executable(citra-glfw ${SRCS} ${HEADERS})
target_link_libraries(citra-glfw core video_core audio_core common)
target_link_libraries(citra-glfw ${GLFW_LIBRARIES} ${OPENGL_gl_LIBRARY} inih glad)
target_link_libraries(citra-glfw ${PLATFORM_LIBRARIES} Threads::Threads)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|OpenBSD|NetBSD")
    install(TARGETS citra-glfw RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()
