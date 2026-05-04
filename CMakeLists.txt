cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    $ENV{ADF_PATH}/components
)

include_directories(${CMAKE_SOURCE_DIR}/main/include)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(IDF_COMPONENT_MANAGER_VERBOSE 1)


project(atlascube)

# spiffs_create_partition_image(storage spiffs_image/web FLASH_IN_PROJECT)