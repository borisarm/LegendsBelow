# Stock arm64-windows (dynamic libraries, dynamic CRT), except for the libraries linked into
# LBRenderingSystem (DirectXTK12, and fastgltf with its simdjson dependency): those are static, so
# no extra DLLs ship with the game or the tests.
# Picked up through vcpkg-configuration.json (overlay-triplets).
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

if(PORT MATCHES "^(directxtk12|fastgltf|simdjson)$")
    set(VCPKG_LIBRARY_LINKAGE static)
endif()

# The game links Release with /guard:ehcont, which needs every static library compiled with
# it (DirectXTK12's own build already is)
if(PORT MATCHES "^(fastgltf|simdjson)$")
    set(VCPKG_C_FLAGS "/guard:cf /guard:ehcont")
    set(VCPKG_CXX_FLAGS "/guard:cf /guard:ehcont")
endif()
