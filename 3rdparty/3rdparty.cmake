include(3rdparty/libossia.cmake)

add_subdirectory(3rdparty/QCodeEditor)
disable_qt_plugins(QCodeEditor)

include(3rdparty/dspfilters.cmake)
include(3rdparty/gamma.cmake)
# include(3rdparty/r8brain.cmake)

if(SCORE_USE_SYSTEM_LIBRARIES)
  find_package(mimalloc GLOBAL CONFIG)
  find_package(Snappy GLOBAL CONFIG)
  find_package(SndFile GLOBAL CONFIG)
  if(EMSCRIPTEN)
    return()
  endif()
  find_package(shmdata GLOBAL)
else()
  include(3rdparty/mimalloc.cmake)
  include(3rdparty/snappy.cmake)

  if(EMSCRIPTEN)
    return()
  endif()
  include(3rdparty/shmdata.cmake)
  include(3rdparty/sndfile.cmake)
endif()
