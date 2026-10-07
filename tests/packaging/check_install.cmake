# The package installs exactly the applet plugin (L-7): no static libraries, tests or headers.
# Run with -DBUILD_DIR=<build> -DSTAGE=<empty folder>.
file(REMOVE_RECURSE "${STAGE}")
execute_process(COMMAND ${CMAKE_COMMAND} -E env DESTDIR=${STAGE} ${CMAKE_COMMAND} --install "${BUILD_DIR}"
                RESULT_VARIABLE result OUTPUT_QUIET)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "cmake --install failed: ${result}")
endif()
file(GLOB_RECURSE installed RELATIVE "${STAGE}" "${STAGE}/*")
list(FILTER installed EXCLUDE REGEX "\\.debug$")
set(expected "usr/lib/qt6/plugins/plasma/applets/org.contribusage.widget.so")
if(NOT installed STREQUAL expected)
    message(FATAL_ERROR "installed files:\n  ${installed}\nexpected:\n  ${expected}")
endif()
foreach(required "packaging/arch/PKGBUILD" "packaging/arch/.SRCINFO")
    if(NOT EXISTS "${SOURCE_DIR}/${required}")
        message(FATAL_ERROR "missing ${required}")
    endif()
endforeach()
