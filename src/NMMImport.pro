#-------------------------------------------------
#
# Project created by QtCreator 2012-11-16T16:51:50
#
#-------------------------------------------------

TARGET = NMMImport
TEMPLATE = lib

QT += xml

CONFIG += plugins
CONFIG += dll

DEFINES += NMMIMPORT_LIBRARY

SOURCES += nmmimport.cpp \
    modselectiondialog.cpp \
    modedialog.cpp \
    nmmpathsdialog.cpp

HEADERS += nmmimport.h \
    modselectiondialog.h \
    modedialog.h \
    nmmpathsdialog.h

RESOURCES += \
    nmmimport.qrc

FORMS += \
    modselectiondialog.ui \
    modedialog.ui \
    nmmpathsdialog.ui

INCLUDEPATH += ../../archive

LIBS += -ladvapi32

include(../plugin_template.pri)
