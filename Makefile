NAME = svgviewer
TYPE = APP
APP_MIME_SIG = application/x-vnd.svg-viewer
SRCS = BSVGView.cpp main.cpp
RDEFS =
RSRCS =
LIBS = be tracker
LIBPATHS =
SYSTEM_INCLUDE_PATHS =
LOCAL_INCLUDE_PATHS = ./nanosvg_ext/src
OPTIMIZE := NONE
LOCALES =
DEFINES =
WARNINGS =
SYMBOLS :=
DEBUGGER :=
COMPILER_FLAGS =
LINKER_FLAGS =
APP_VERSION :=
DRIVER_PATH =

## Include the Makefile-Engine
DEVEL_DIRECTORY := \
	$(shell findpaths -r "makefile_engine" B_FIND_PATH_DEVELOP_DIRECTORY)
include $(DEVEL_DIRECTORY)/etc/makefile-engine
