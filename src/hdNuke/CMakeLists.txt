# Build the hdNuke library
add_library(${HDNUKE_LIB_NAME} SHARED
    adapterFactory.cpp
    adapterManager.cpp
    instancedGeoAdapter.cpp
    environmentLightAdapter.cpp
    delegateConfig.cpp
    geoAdapter.cpp
    hydraOpManager.cpp
    instancerAdapter.cpp
    knobFactory.cpp
    lightAdapter.cpp
    lightOp.cpp
    materialAdapter.cpp
    nukeTexturePlugin.cpp
    opBases.cpp
    particleSpriteAdapter.cpp
    renderStack.cpp
    sceneDelegate.cpp
    tokens.cpp
    utils.cpp
    vtValueKnobCache.cpp)

target_include_directories(${HDNUKE_LIB_NAME}
    PRIVATE
    ${USD_INCLUDE_DIR})

target_link_libraries(${HDNUKE_LIB_NAME}
    Nuke::NDK
    ${PXR_LIBRARIES})

install(TARGETS ${HDNUKE_LIB_NAME} DESTINATION lib COMPONENT Nuke)

# Build the texture bridge plugins
add_library(${HDNUKETEXTURE_LIB_NAME} SHARED
    hdNukeTexture.cpp)

target_include_directories(${HDNUKETEXTURE_LIB_NAME}
    PRIVATE
    ${USD_INCLUDE_DIR})

target_link_libraries(${HDNUKETEXTURE_LIB_NAME}
    Nuke::NDK
    ${HDNUKE_LIB_NAME}
    ${PXR_LIBRARIES})

# We must make symbols visible for the plugin as otherwise dynamic_cast to HioImageFactoryBase will fail
# due to multiple copies of the typeinfo for it.
set_target_properties(${HDNUKETEXTURE_LIB_NAME} PROPERTIES CXX_VISIBILITY_PRESET default)

install(TARGETS ${HDNUKETEXTURE_LIB_NAME} DESTINATION lib COMPONENT Nuke)

set(HDNUKE_HEADER_FILES
  adapterManager.h
  instancedGeoAdapter.h
  environmentLightAdapter.h
  adapter.h
  adapterFactory.h
  delegateConfig.h
  geoAdapter.h
  hydraOpManager.h
  instancerAdapter.h
  knobFactory.h
  lightAdapter.h
  lightOp.h
  materialAdapter.h
  nukeTexturePlugin.h
  opBases.h
  particleSpriteAdapter.h
  renderStack.h
  sceneDelegate.h
  sharedState.h
  tokens.h
  types.h
  utils.h
  vtValueKnobCache.h
)

install(FILES ${HDNUKE_HEADER_FILES} DESTINATION include/hdNuke COMPONENT Nuke)

install(FILES plugInfo.json DESTINATION lib COMPONENT Nuke)

# We have six variations on the pluginInfo.json depending on platforms and USD version.
# The library name varies between platforms
if(EXISTS "${PXR_CMAKE_DIR}/include/pxr/imaging/hio/image.h")
  set(PLUGINFO_BASE "HioImage")
elseif(EXISTS "${PXR_CMAKE_DIR}/include/pxr/imaging/garch/image.h")
  set(PLUGINFO_BASE "GarchImage")
else()
  set(PLUGINFO_BASE "GlfImage")
endif()
if(APPLE)
  set(PLUGINFO_DYLIB_NAME "libHdNukeTexture.dylib")
elseif(UNIX AND NOT APPLE)
  set(PLUGINFO_DYLIB_NAME "libHdNukeTexture.so")
else()
  set(PLUGINFO_DYLIB_NAME "HdNukeTexture.dll")
endif()

# pluginInfo configuration and install
function(_configure_plugInfo plugInfoPath pluginBasename)
    configure_file(
        ${plugInfoPath}
        ${CMAKE_CURRENT_BINARY_DIR}/${pluginBasename}
    )
endfunction() # _configure_plugInfo

set(PLUGINFO_PATH "${CMAKE_CURRENT_LIST_DIR}/hdNukeTexture/resources/plugInfo.json")
get_filename_component(pluginBasename ${PLUGINFO_PATH} NAME)
_configure_plugInfo(${PLUGINFO_PATH} ${pluginBasename})
install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${pluginBasename}
    DESTINATION
    lib/hdNukeTexture/resources
    COMPONENT Nuke
)
