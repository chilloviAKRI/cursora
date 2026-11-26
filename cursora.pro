QT += core gui qml quick quickcontrols2
CONFIG += c++17 release link_pkgconfig
PKGCONFIG += xcursor
TARGET = cursora
TEMPLATE = app
HEADERS += src/backend.h src/theme.h src/translator.h src/cursorimages.h
SOURCES += src/main.cpp src/backend.cpp src/theme.cpp src/translator.cpp src/cursorimages.cpp src/omarchycursor.cpp
RESOURCES += src/resources.qrc
