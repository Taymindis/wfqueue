cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(gtest-wfqueue CXX)

###############################################################################
## file globbing ##############################################################
###############################################################################

# file glob the sources_test and resources file
# `sources_test` and `resources`
file(GLOB_RECURSE sources_test WfqueueGoogleTest.cpp)
# file(GLOB_RECURSE resources resources/*)


link_directories(/usr/local/lib /usr/lib)

if(NOT ${CMAKE_VERSION} LESS 3.2)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
else()
    message(STATUS "Checking compiler flags for C++11 support.")
    # Set C++11 support flags for various compilers
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-std=c++11" COMPILER_SUPPORTS_CXX11)
    check_cxx_compiler_flag("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
    if(COMPILER_SUPPORTS_CXX11)
        message(STATUS "C++11 is supported.")
        if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
        endif()
    elseif(COMPILER_SUPPORTS_CXX0X)
        message(STATUS "C++0x is supported.")
        if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -stdlib=libc++")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
        endif()
    else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
    endif()
endif()


# this is copy the resources to build directory
# file(COPY ${resources} DESTINATION ${PROJECT_BINARY_DIR})


# set the default libpath
# SET(LIBPATH /usr/lib /usr/local/lib)
include_directories(../ /usr/local/include /usr/include)


################################
# GTest
################################
ADD_SUBDIRECTORY (googletest-release-1.8.1)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})


################################
# Google Unit Tests
################################
add_executable(wfqueue_google_test ${sources_test})
target_link_libraries(wfqueue_google_test gtest gtest_main)
#target_include_directories(wfqueue_google_test PUBLIC ${GTEST_INCLUDE_DIRS})
add_test(wfq_gtest wfqueue_google_test)

add_custom_command(TARGET wfqueue_google_test
        POST_BUILD
        COMMAND "./wfqueue_google_test"
        )
