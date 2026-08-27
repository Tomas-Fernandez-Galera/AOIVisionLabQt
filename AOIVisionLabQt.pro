QT += core gui widgets

CONFIG += c++17
CONFIG -= debug_and_release debug_and_release_target
TEMPLATE = app
TARGET = AOIVisionLabQt

# OpenCV is installed locally by the vcpkg manifest. Keeping it inside the
# project makes the toolchain reproducible without changing the system PATH.
OPENCV_ROOT = $$PWD/vcpkg_installed/x64-mingw-dynamic
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4 \
               $$PWD/src

CONFIG(debug, debug|release) {
    LIBS += -L$$OPENCV_ROOT/debug/lib \
        -lopencv_calib3d4d \
        -lopencv_features2d4d \
        -lopencv_imgcodecs4d \
        -lopencv_imgproc4d \
        -lopencv_flann4d \
        -lopencv_core4d
} else {
    LIBS += -L$$OPENCV_ROOT/lib \
        -lopencv_calib3d4 \
        -lopencv_features2d4 \
        -lopencv_imgcodecs4 \
        -lopencv_imgproc4 \
        -lopencv_flann4 \
        -lopencv_core4
}

SOURCES += \
    src/main.cpp \
    src/inspectionengine.cpp \
    src/zoomimagelabel.cpp \
    src/mainwindow.cpp

HEADERS += \
    src/inspectionengine.h \
    src/zoomimagelabel.h \
    src/mainwindow.h

FORMS += \
    forms/mainwindow.ui

OTHER_FILES += \
    README.md \
    automation/aoi_mcp_server.py \
    automation/mcp-config-example.json \
    vcpkg.json \
    .gitignore
