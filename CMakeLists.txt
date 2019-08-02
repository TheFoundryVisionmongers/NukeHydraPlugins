cmake_minimum_required(VERSION 3.13)

project(NukeToHydra)


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                      ${CMAKE_CURRENT_SOURCE_DIR}/cmake
                      $ENV{CMAKE_MODULE_PATH})


set(BOOST_PYTHON_COMPONENT_NAME "python27"
    CACHE STRING
    "The Boost component name to pass to the `find_package` command.")

set(HDNUKE_LIB_NAME HdNuke)
set(HDNUKETEXTURE_LIB_NAME HdNukeTexture)

find_package(Nuke REQUIRED)
find_package(USD 0.20.2 REQUIRED)

find_package(Boost REQUIRED
    COMPONENTS ${BOOST_PYTHON_LIBNAME})


include_directories(SYSTEM
    ${Boost_INCLUDE_DIRS}
    ${PYTHON_INCLUDE_DIRS}
    ${TBB_INCLUDE_DIRS})

link_directories(${USD_LIBRARY_DIR})


set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
  add_compile_options(-DNOMINMAX -permissive-)
else()
  add_compile_options(-fPIC -msse -Wall -Wno-deprecated)
endif()

# If this is a Metal version of USD on macOS, enable use of the Metal version of hdStorm
if(APPLE AND NOT PXR_METAL_SUPPORT_ENABLED AND EXISTS "${USD_ROOT}/include/pxr/imaging/hgiInterop")
  message(STATUS "Hydra Plugins: Using Metal hdStorm")
  set(PXR_METAL_SUPPORT_ENABLED 1)
endif()

if(PXR_METAL_SUPPORT_ENABLED)
  add_definitions(
    -DPXR_METAL_SUPPORT_ENABLED=1
  )
endif()

# For USD 20.08 with Metal support, we need to compile with the Objective-C compiler
# as some USD header files include Objective-C Metal header files.
if(APPLE)
  add_compile_options(-x objective-c++)
endif()

add_subdirectory(src/hdNuke)
add_subdirectory(src/ops)

if (BUILD_HDNUKE_UNITTESTS)
  find_package(Catch2 REQUIRED)
  find_package(GMock REQUIRED)

  if (NOT COMMAND add_hdnuke_unittest)
    function(add_hdnuke_unittest TARGET_NAME)
      add_executable(${TARGET_NAME} "tests/main.cpp" ${ARGN})
      target_link_libraries(${TARGET_NAME} PUBLIC Catch2::Catch2 GTest::GMock)
      add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
    endfunction()
  endif()

  add_hdnuke_unittest(NukeHydraPlugins.UT
    "tests/hdNuke/adapterFactoryTest.cpp"
    "tests/hdNuke/adapterManagerTest.cpp"
  )

  target_link_libraries(NukeHydraPlugins.UT
    PRIVATE
      ${HDNUKE_LIB_NAME}
      ${arg_USD_LINK_LIBS}
      ${PXR_LIBRARIES}
  )
endif ()

