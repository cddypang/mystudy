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
set(CMAKE_C_FLAGS "-Wall -g ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-Wall -g -std=c++11 -Wno-unused-local-typedefs -Wno-sign-compare ${CMAKE_CXX_FLAGS}")
ENDIF ()

IF (UNIX)
#set(CUDALIBS -L/usr/local/cuda/lib64 -lcublas -lcusolver -lcudart -lcufft -lcurand)
#set(LDADD -L${TOP_DIR}/../../../cyTts_now -lcyTts -ltensorflow_framework)
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
  -L${KALDI_DIR}/tools/openfst/lib
  -lfst
  -lpthread
  )

target_link_libraries(${CONSOLE_APP} ${LDADD})
ENDIF ()
