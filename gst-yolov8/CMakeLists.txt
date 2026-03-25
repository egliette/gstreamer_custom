cmake_minimum_required(VERSION 3.16)
project(gst-yolov8 CXX)
set(CMAKE_CXX_STANDARD 17)

add_library(gstyolov8infer SHARED
    src/plugin.cpp 
    src/gstyolov8infer.cpp 
)

target_compile_definitions(gstyolov8infer PRIVATE
    PACKAGE="gst-yolov8"
    VERSION="1.0.0"
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GST_VID REQUIRED gstreamer-video-1.0)
find_package(OpenCV REQUIRED)

set(ORT_ROOT "/usr/local")

target_include_directories(gstyolov8infer PRIVATE
    ${GST_INCLUDE_DIRS}
    ${GST_VID_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${ORT_ROOT}/include
    include
)

target_link_libraries(gstyolov8infer PRIVATE 
    ${GST_LIBRARIES}
    ${GST_VID_LIBRARIES}
    ${OpenCV_LIBS}
    ${ORT_ROOT}/lib/libonnxruntime.so
)

execute_process(
    COMMAND pkg-config --variable=pluginsdir gstreamer-1.0
    OUTPUT_VARIABLE GST_PLUGIN_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
install(TARGETS gstyolov8infer DESTINATION ${GST_PLUGIN_DIR})