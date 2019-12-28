CMAKE_MINIMUM_REQUIRED(VERSION 3.2)

project(vadpro C CXX)

set(TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(KALDI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../kaldi)

include_directories(
    ${TOP_DIR}
    ${KALDI_DIR}/src
    ${KALDI_DIR}/tools/openfst/include
    )

IF (WIN32)

link_directories(
    ${TOP_DIR}
    )
ELSEIF (UNIX)
link_directories(
    ${KALDI_DIR}/src/feat
    )
ENDIF ()

IF (WIN32)
add_definitions(-DNOMINMAX)
set(CMAKE_CXX_FLAGS_RELEASE "/MT /Zi /DEBUG /O2")
set(CMAKE_CXX_FLAGS_DEBUG "/MT /Zi /DEBUG /Od")
ELSE ()
set(CXX_IGN_WARNINGS 
  " -Wno-write-strings\
    -Wno-unused-but-set-variable\
    -Wno-writable-strings\
    -Wno-unused-local-typedefs\
    -Wno-sign-compare\
    -Wno-comment ")
set(CMAKE_C_FLAGS "-Wall -g ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-Wall -g -std=c++11 ${CXX_IGN_WARNINGS} ${CMAKE_CXX_FLAGS}")
ENDIF ()

IF (UNIX)
#set(CUDALIBS -L/usr/local/cuda/lib64 -lcublas -lcusolver -lcudart -lcufft -lcurand)
#set(LDADD -L${TOP_DIR}/../../../cyTts_now -lcyTts -ltensorflow_framework)
ENDIF ()


################# cyVoice_LIB ######################
set(LIBASR asr)
set(libasr_SOURCES
    ${TOP_DIR}/libasr.cpp
    ${TOP_DIR}/nnet3-decoding.cpp
)

add_library(${LIBASR} SHARED ${libasr_SOURCES})

IF (WIN32)

ELSE ()
set(LDADD_STATIC 
  ${KALDI_DIR}/src/feat/kaldi-feat.a
  ${KALDI_DIR}/src/matrix/kaldi-matrix.a
  ${KALDI_DIR}/src/cudamatrix/kaldi-cudamatrix.a
  ${KALDI_DIR}/src/base/kaldi-base.a
  ${KALDI_DIR}/src/util/kaldi-util.a
  ${KALDI_DIR}/src/ivector/kaldi-ivector.a
  ${KALDI_DIR}/src/nnet3/kaldi-nnet3.a
  ${KALDI_DIR}/src/decoder/kaldi-decoder.a
  ${KALDI_DIR}/src/lat/kaldi-lat.a
  ${KALDI_DIR}/src/hmm/kaldi-hmm.a
  ${KALDI_DIR}/src/fstext/kaldi-fstext.a
  ${KALDI_DIR}/src/transform/kaldi-transform.a
  ${KALDI_DIR}/src/online2/kaldi-online2.a
  ${KALDI_DIR}/src/gmm/kaldi-gmm.a
  ${KALDI_DIR}/src/tree/kaldi-tree.a
  #${KALDI_DIR}/tools/openfst/lib/libfst.a
  -L${KALDI_DIR}/tools/openfst/lib  ### TODO !!!
  -lfst
  #dynamic
  -lpthread
  )

IF(CMAKE_HOST_APPLE)
find_library(Accelerate_UNIT Accelerate)
if (NOT Accelerate_UNIT)
    message(FATAL_ERROR "Accelerate not found")
endif()
set(ATLASLIBS 
  ${Accelerate_UNIT} -ldl
  )
target_compile_options(${LIBASR} PUBLIC -fPIC)
ELSE()
set(ATLASLIBS 
    /usr/lib/libatlas.so.3 
    /usr/lib/libf77blas.so.3 
    /usr/lib/libcblas.so.3 
    /usr/lib/liblapack_atlas.so.3 
    -Wl,-rpath=/usr/lib
    )
target_compile_options(${LIBASR} PUBLIC -shared -fPIC)
ENDIF()
target_link_libraries(${LIBASR} ${LDADD_STATIC} ${ATLASLIBS})
ENDIF ()

################# LIBTEST_APP ######################
set(LIBTEST asrtest)
set(asrtest_SOURCES
    ${TOP_DIR}/libtest.cpp    
)

add_executable(${LIBTEST} ${asrtest_SOURCES})

IF (WIN32)

ELSE ()  ## UNIX
IF(CMAKE_HOST_APPLE)
target_compile_options(${LIBTEST} PUBLIC -fPIC)
target_link_libraries(${LIBTEST} ${LIBASR})
ELSE()
include_directories(
  /usr/local/cuda/include
  )
set(CMAKE_CXX_FLAGS "-DHAVE_CUDA=1 ${CMAKE_CXX_FLAGS}")
target_compile_options(${LIBTEST} PUBLIC -shared -fPIC)
set(LDADD_CUDA
  -L/usr/local/cuda/lib64
  -lcublas
  -lcurand
  -lcudart
  -lcusparse
  )
target_link_libraries(${LIBTEST} ${LIBASR} ${LDADD_CUDA})
ENDIF()
ENDIF ()


################# CONSOLE_APP ######################
set(CONSOLE_APP vadtool)

set(vad_SOURCES
    ${TOP_DIR}/main.cpp
   )

IF (WIN32)

ELSE ()
add_executable(${CONSOLE_APP} ${vad_SOURCES})
#target_compile_options(${BASE_LIB} PUBLIC -shared -fPIC)

set(LDADD 
  -L${KALDI_DIR}/src/lib 
  -lkaldi-feat 
  -lkaldi-matrix
  -lkaldi-base
  -lkaldi-util
  -lkaldi-ivector
  -lkaldi-nnet3
  -lkaldi-cudamatrix
  -lkaldi-decoder
  -lkaldi-lat
  -lkaldi-hmm
  -lkaldi-fstext
  -lkaldi-transform
  -lkaldi-online2
  -lkaldi-gmm
  -lkaldi-tree
  -L${KALDI_DIR}/tools/openfst/lib
  -lfst
  -lpthread
  )

target_link_libraries(${CONSOLE_APP} ${LDADD})
ENDIF ()

################# TESTSDK_APP ######################
set(TESTSDK sdktest)
set(sdktest_SOURCES
    ${TOP_DIR}/test/test_sdk.cpp 
)

add_executable(${TESTSDK} ${sdktest_SOURCES})

IF (WIN32)

ELSE ()  ## UNIX
set(LDTIEG -L${TOP_DIR}/build -ltieg)
IF(CMAKE_HOST_APPLE)
target_compile_options(${TESTSDK} PUBLIC -fPIC)
target_link_libraries(${TESTSDK} ${LIBASR} ${LDTIEG})
ELSE()
include_directories(
  ${TOP_DIR}/test
  /usr/local/cuda/include
  )
set(CMAKE_CXX_FLAGS "-DHAVE_CUDA=1 ${CMAKE_CXX_FLAGS}")
target_compile_options(${TESTSDK} PUBLIC -shared -fPIC)
set(LDADD_CUDA
  -L/usr/local/cuda/lib64
  -lcublas
  -lcurand
  -lcudart
  -lcusparse
  )
target_link_libraries(${TESTSDK} ${LIBASR} ${LDADD_CUDA} ${LDTIEG})
ENDIF()
ENDIF ()
