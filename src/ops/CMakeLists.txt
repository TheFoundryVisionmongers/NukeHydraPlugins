# This is pretty basic, but it will do for now...
function(add_hdnuke_op PLUGIN_NAME)
    set(multiValueArgs
        USD_LINK_LIBS)

    cmake_parse_arguments(arg
        ""
        ""
        ${multiValueArgs}
        ${ARGN})

    add_library(${PLUGIN_NAME} SHARED
        "${PLUGIN_NAME}.cpp")

    target_include_directories(${PLUGIN_NAME}
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        ${USD_INCLUDE_DIR})

    target_link_libraries(${PLUGIN_NAME}
        ${HDNUKE_LIB_NAME}
        Nuke::NDK
        ${arg_USD_LINK_LIBS})

    set_target_properties(${PLUGIN_NAME}
        PROPERTIES
            PREFIX ""
            INSTALL_RPATH "$ORIGIN/../lib"
    )


    install(TARGETS ${PLUGIN_NAME} DESTINATION plugins COMPONENT Nuke)
endfunction()


add_definitions(-DUSE_GLEW)


# TODO: DistantLight, GeometryLight
set(HDNUKE_OPS
    HydraRender
    HydraScene
    HydraUSDStage
    # Lights
    HydraCylinderLight
    HydraDiskLight
    HydraDomeLight
    HydraRectLight
    HydraSphereLight
)

foreach(OP_NAME ${HDNUKE_OPS})
    add_hdnuke_op(${OP_NAME})
endforeach()
