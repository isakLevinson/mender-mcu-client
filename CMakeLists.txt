# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

#set(PROJECT_VER "0.1.0.1")

set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/system/console/advanced/components
	)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pnu)


add_custom_target(format
    COMMAND astyle --suffix=none --formatted --options=${CMAKE_SOURCE_DIR}/.astyle --recursive "${CMAKE_SOURCE_DIR}/main/*.c,.h" "${CMAKE_SOURCE_DIR}/components/*.c,.h"

    COMMENT "Running Astyle for code formatting"
)

# add_dependencies(app format)
