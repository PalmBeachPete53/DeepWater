# Install step run after macdeployqt (CMakeLists.txt › Install and packaging), on macOS only.
#
# With Homebrew's Qt, macdeployqt copies the frameworks but leaves QtWebEngineProcess (the helper app inside
# QtWebEngineCore.framework) linked to /opt/homebrew/opt/qt*/lib/...: on a Mac without Homebrew the article reader's
# web process wouldn't start, and on one with it, two copies of Qt would load. Every Mach-O file of the bundle that
# still names a library outside the system gets that name rewritten to @rpath (the helper's rpath reaches
# Contents/Frameworks, where the copy is). With Qt from the online installer (the CI build) there is nothing to do.
# The virtual keyboard input plugin, pulled in from Homebrew's plugin folder, needs QtVirtualKeyboard, which isn't
# deployed, and so does the NMEA positioning plugin with QtSerialPort: both are removed. Qt from the online installer
# is universal (arm64 + x86_64) while deepwater itself is built for one architecture (DEEPWATER_ARCH, set by
# CMakeLists.txt): the other half of every library is dropped, which halves the .dmg. The bundle is signed again
# afterwards (install_name_tool and lipo void signatures).

set(app "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/deepwater.app")
file(REMOVE "${app}/Contents/PlugIns/platforminputcontexts/libqtvirtualkeyboardplugin.dylib"
            "${app}/Contents/PlugIns/position/libqtposition_nmea.dylib")

file(GLOB_RECURSE files LIST_DIRECTORIES false "${app}/Contents/MacOS/*" "${app}/Contents/Frameworks/*"
     "${app}/Contents/PlugIns/*")
set(fixed 0)
set(thinned 0)
foreach(f IN LISTS files)
    if(IS_SYMLINK "${f}" OR f MATCHES "\\.(pak|dat|bin|plist|qm|json|png|icns|strings|nib|prl)$" OR f MATCHES "/Resources/")
        continue()
    endif()
    execute_process(COMMAND otool -L "${f}" OUTPUT_VARIABLE deps ERROR_QUIET RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0 OR deps MATCHES "is not an object file")
        continue()
    endif()
    if(DEEPWATER_ARCH)
        execute_process(COMMAND lipo -archs "${f}" OUTPUT_VARIABLE archs OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(archs MATCHES " " AND archs MATCHES "${DEEPWATER_ARCH}")
            execute_process(COMMAND lipo "${f}" -thin ${DEEPWATER_ARCH} -output "${f}" COMMAND_ERROR_IS_FATAL ANY)
            math(EXPR thinned "${thinned} + 1")
        endif()
    endif()
    string(REGEX MATCHALL "\n\t/[^\n]+ \\(compatibility" lines "${deps}")
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "^\n\t([^ ]+) \\(compatibility$" "\\1" dep "${line}")
        if(dep MATCHES "^/(System|usr/lib)/")
            continue()
        endif()
        if(dep MATCHES "/([^/]+\\.framework/.+)$")
            set(name "${CMAKE_MATCH_1}")
        else()
            get_filename_component(name "${dep}" NAME)
        endif()
        execute_process(COMMAND install_name_tool -change "${dep}" "@rpath/${name}" "${f}" ERROR_QUIET
                        COMMAND_ERROR_IS_FATAL ANY)
        math(EXPR fixed "${fixed} + 1")
    endforeach()
endforeach()
message(STATUS "Bundle: ${fixed} library path(s) outside the system rewritten to @rpath, ${thinned} file(s) thinned to ${DEEPWATER_ARCH}")

execute_process(COMMAND codesign --force --deep --sign - "${app}" COMMAND_ERROR_IS_FATAL ANY)
