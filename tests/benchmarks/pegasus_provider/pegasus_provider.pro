CONFIG += testcase

QT += qml testlib
CONFIG += c++11 warn_on exceptions_off

TARGET = bench_PegasusProvider
SOURCES = $${TARGET}.cpp
DEFINES *= $${COMMON_DEFINES}

include($${TOP_SRCDIR}/src/link_to_backend.pri)

RESOURCES += \
    data/data.qrc