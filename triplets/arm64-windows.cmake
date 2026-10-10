# Stock arm64-windows (dynamic libraries, dynamic CRT) with one exception: DirectXTK12 links
# statically into LBRenderingSystem, so no DirectXTK12.dll ships with the game or the tests.
# Picked up through vcpkg-configuration.json (overlay-triplets).
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

if(PORT STREQUAL "directxtk12")
    set(VCPKG_LIBRARY_LINKAGE static)
endif()
