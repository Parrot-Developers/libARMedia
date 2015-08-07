###############################################################################
# THE LIBRARIES TO BUILD
###############################################################################
ACLOCAL_AMFLAGS                                     =   -I m4

# The list of header files that belong to the library (to be installed later)

# The iOS dependency sources
HEADER_FILES                                        = ../Includes/libARMedia/ARMEDIA_VideoAtoms.h       \
                                                      ../Includes/libARMedia/ARMEDIA_Error.h            \
                                                      ../Includes/libARMedia/ARMEDIA_VideoEncapsuler.h  \
                                                      ../Includes/libARMedia/ARMedia.h
if HAVE_UIKIT
HEADER_FILES                                        += ../Includes/libARMedia/ARMEDIA_Manager.h     \
                                                       ../Includes/libARMedia/ARMEDIA_Object.h

endif

# The sources to add to the library and to add to the source distribution
SOURCE_FILES                                        =  $(HEADER_FILES)                                  \
                                                       ../gen/Sources/ARMEDIA_Error.c    				\
                                                       ../Sources/ARMEDIA_VideoEncapsuler.c    			\
                                                       ../Sources/ARMEDIA_VideoAtoms.c


# The iOS dependency sources
if HAVE_UIKIT
SOURCE_FILES                                        += ../Sources/ARMEDIA_Manager.m                  \
                                                       ../Sources/ALAssetRepresentation+VideoAtoms.h \
                                                       ../Sources/ALAssetRepresentation+VideoAtoms.m \
                                                       ../Sources/ARMEDIA_Object.m
endif

# The library names to build (note we are building static and shared libs)
if DEBUG_MODE
lib_LTLIBRARIES                                     =   libarmedia_dbg.la
libarmedia_dbg_ladir                                =   $(includedir)/libARMedia
libarmedia_dbg_la_HEADERS                           =   $(HEADER_FILES)
libarmedia_dbg_la_SOURCES                           =   $(SOURCE_FILES)
else
lib_LTLIBRARIES                                     =   libarmedia.la
libarmedia_ladir                                    =   $(includedir)/libARMedia
libarmedia_la_HEADERS                               =   $(HEADER_FILES)
libarmedia_la_SOURCES                               =   $(SOURCE_FILES)
endif

CLEAN_FILES                                         =   libarmedia.la       \
                                                        libarmedia_dbg.la

EXTRA_DIST                                          =   bootstrap

# Ensure the distribution of the doxygen configuration file
# Doxygen support
if HAVE_DOXYGEN
include doxygen.am
EXTRA_DIST                                          +=  doxygen.ac  \
                                                        doxygen.am  \
                                                        $(DX_CONFIG)
endif
