TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        MediaContext.cpp \
        main.cpp

DESTDIR = $$PWD

INCLUDEPATH += $$PWD/FFmpeg/include
INCLUDEPATH += $$PWD/SDL/include

LIBS += -L$$PWD/FFmpeg/lib -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lpostproc -lswresample -lswscale
LIBS += -L$$PWD/SDL/lib -lSDL2 -lSDL2main -lSDL2test

HEADERS += \
    FrameQueue.h \
    MediaContext.h
