# created: 2015-11-21
# author: Tim Hoffmann

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD


include(synctex/synctex.pri)


HEADERS += \
    pdfviewer/PDFDocument.h \
    pdfviewer/PDFDocks.h \
    pdfviewer/pdfrenderengine.h \
    pdfviewer/pdfrendermanager.h \
    pdfviewer/PDFDocument_config.h \
    pdfviewer/pdfannotationdlg.h \
    pdfviewer/pdfannotation.h \
    pdfviewer/qsynctex.h

SOURCES += \
    pdfviewer/PDFDocument.cpp \
    pdfviewer/PDFDocks.cpp \
    pdfviewer/pdfrenderengine.cpp \
    pdfviewer/pdfrendermanager.cpp \
    pdfviewer/pdfannotationdlg.cpp \
    pdfviewer/pdfannotation.cpp \
    pdfviewer/qsynctex.cpp

FORMS += \
    pdfviewer/pdfannotationdlg.ui


# ################################
# Poppler PDF Preview, will only be used if NO_POPPLER_PREVIEW is not set
isEmpty(NO_POPPLER_PREVIEW) {
    win32 {
        !greaterThan(QT_MAJOR_VERSION, 4) { #Qt4
            INCLUDEPATH  += ./pdfviewer/include_win32
            LIBS += ./zlib1.dll ./libpoppler-qt4.dll
        } else { # Qt5
            INCLUDEPATH  += ./pdfviewer/include_win32_qt5
            LIBS += ./zlib1.dll ./libpoppler-qt5.dll
        }
        DEFINES += HAS_POPPLER_24
        DEFINES += HAS_POPPLER_31
        LIBS += -lshlwapi
    } else {
        macx { # PATH to pkgconfig needs to be present in build PATH
            QT_CONFIG -= no-pkg-config
        }
        poppler_qt_pkg = poppler-qt$${QT_MAJOR_VERSION}

        CONFIG += link_pkgconfig

        PKGCONFIG += $${poppler_qt_pkg}

        greaterThan(QT_MAJOR_VERSION,4){
            PKG_CONFIG_EXE = $$pkgConfigExecutable()
            isEmpty(PKG_CONFIG_EXE) {
                error("pkg-config not found. This tool is required if building with poppler. Please install it.")
            }
        }else{
            PKG_CONFIG_EXE = "pkg-config"
        }
	LIBS += -L/usr/local/Cellar/poppler/0.61.0-texworks/lib
        system($${PKG_CONFIG_EXE} --atleast-version=0.24 $${poppler_qt_pkg}):DEFINES += HAS_POPPLER_24
        system($${PKG_CONFIG_EXE} --atleast-version=0.31 $${poppler_qt_pkg}):DEFINES += HAS_POPPLER_31
    }
} else {
    DEFINES += NO_POPPLER_PREVIEW
    message("Internal pdf previewer disabled as you wish.")
}
