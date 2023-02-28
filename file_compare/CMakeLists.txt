cmake_minimum_required(VERSION 3.6)

# set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "build type")

set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_BUILD_TYPE RELEASE)
#project(wfrest_example LANGUAGES C CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

if (NOT "$ENV{LIBRARY_PATH}" STREQUAL "")
	string(REPLACE ":" ";" LIBRARY_PATH $ENV{LIBRARY_PATH})
	set(CMAKE_SYSTEM_LIBRARY_PATH ${LIBRARY_PATH};${CMAKE_SYSTEM_LIBRARY_PATH})
endif ()

if (NOT "$ENV{CPLUS_INCLUDE_PATH}" STREQUAL "")
	string(REPLACE ":" ";" INCLUDE_PATH $ENV{CPLUS_INCLUDE_PATH})
	set(CMAKE_SYSTEM_INCLUDE_PATH ${INCLUDE_PATH};${CMAKE_SYSTEM_INCLUDE_PATH})
endif ()

find_package(OpenSSL REQUIRED)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/workflow/workflow-config.cmake.in")
	find_package(Workflow REQUIRED CONFIG HINTS ../workflow)
endif ()

find_package(ZLIB REQUIRED)
find_package(wfrest REQUIRED CONFIG HINTS ..)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(
	${OPENSSL_INCLUDE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${WORKFLOW_INCLUDE_DIR}
	${WFREST_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/api
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/compare
    ${CMAKE_CURRENT_SOURCE_DIR}/content
    ${CMAKE_CURRENT_SOURCE_DIR}/diff
	${CMAKE_CURRENT_SOURCE_DIR}/internal
)

set(SYS_LIB pthread m ssl crypto)
link_directories(${WFREST_LIB_DIR} ${WORKFLOW_LIB_DIR})

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -fPIC -pipe -std=gnu90")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -pipe -std=c++11 -fno-exceptions")

if (APPLE)
	set(WFREST_LIB wfrest workflow pthread OpenSSL::SSL OpenSSL::Crypto protobuf z)
else ()
	set(WFREST_LIB wfrest pthread)
endif ()

set(EXAMPLE_LIST
    main
    test_client
)

set(SRC
./common/dc_common_assert.cc
./common/dc_common_error.cc
./common/dc_common_log.cc
./common/cJSON.cc
./compare/dc_compare.cc
./api/dc_api_task.cc
./content/dc_content.cc
./diff/dc_diff.cc
./diff/dc_diff_content.cc
./internal/dc_internal_task.cc
)

add_executable(main
./main.cc
${SRC}
)
target_link_libraries(main ${SYS_LIB} ${WFREST_LIB})

add_executable(test_post
    ./test_post.cc
)
target_link_libraries(test_post ${SYS_LIB} ${WFREST_LIB})


add_executable(test_get
    ./test_get.cc
)
target_link_libraries(test_get ${SYS_LIB} ${WFREST_LIB})

add_executable(test_file_sha1
test_file_sha1.cc
${SRC}
)
target_link_libraries(test_file_sha1 ${SYS_LIB} ${WFREST_LIB})

add_executable(test_internal_task
internal/test_dc_internal_task.cc
${SRC}
)
target_link_libraries(test_internal_task ${SYS_LIB} ${WFREST_LIB})
