cmake_minimum_required(VERSION 3.5.0)
project(multivox VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(DRIVER_SRC_DIR ${SRC_DIR}/driver)
set(PLATFORM_SRC_DIR ${SRC_DIR}/platform)

set(BUILD_DIR ${CMAKE_BINARY_DIR})


function(set_gadget GADGET_NAME)
    set(HEADER_PATH "${DRIVER_SRC_DIR}/gadgets/gadget_${GADGET_NAME}.h")
    set(OUTPUT_HEADER "${BUILD_DIR}/generated/gadget.h")
    if(EXISTS ${HEADER_PATH})
        message(STATUS "Using gadget header: ${HEADER_PATH}")
        configure_file(${HEADER_PATH} ${OUTPUT_HEADER} COPYONLY)
    else()
        message(FATAL_ERROR "Gadget header ${HEADER_PATH} does not exist.")
    endif()
endfunction()

set(MULTIVOX_GADGET "vortex" CACHE STRING "Gadget-specific header file")
set_gadget(${MULTIVOX_GADGET})


include_directories(
    ${DRIVER_SRC_DIR}
    ${PLATFORM_SRC_DIR}
    ${BUILD_DIR}/generated
)

file(GLOB DRIVER_SRC ${DRIVER_SRC_DIR}/*.c)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMATHC_USE_UNIONS -O3 -Wall")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DMATHC_USE_UNIONS -Og -Wall")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DMATHC_USE_UNIONS -O3 -Wall")

add_executable(vortex
    ${DRIVER_SRC}
    ${PLATFORM_SRC_DIR}/mathc.c
    ${PLATFORM_SRC_DIR}/input.c
)
target_link_libraries(vortex PRIVATE m rt pthread)


file(GLOB PLATFORM_SRC ${PLATFORM_SRC_DIR}/*.c)
add_library(platform STATIC ${PLATFORM_SRC})

function(add_toy toy)
    if(EXISTS ${SRC_DIR}/toys/${toy}.c)
        add_executable(${toy} ${SRC_DIR}/toys/${toy}.c)
    else()
        file(GLOB TOY_SRC "${SRC_DIR}/toys/${toy}/*.c")
        add_executable(${toy} ${TOY_SRC})
    endif()
    target_link_libraries(${toy} PRIVATE platform m)
endfunction()

add_toy(viewer)
add_toy(zander)
add_toy(tesseract)
add_toy(flight)
add_toy(fireworks)

