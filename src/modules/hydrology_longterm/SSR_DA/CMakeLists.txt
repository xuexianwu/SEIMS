LINK_DIRECTORIES(${PROJECT_BINARY_DIR})
INCLUDE_DIRECTORIES(../../../base/util ../../../base/module_setting)
SET(SRC_LIST ../../../base/util/SimulationModule.cpp SSR_DA.cpp api.cpp)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
ADD_LIBRARY(SSR_DA SHARED ${SRC_LIST})
#TARGET_LINK_LIBRARIES(SSR_DA libutil.a)
TARGET_LINK_LIBRARIES(SSR_DA util module_setting)
SET_TARGET_PROPERTIES( SSR_DA PROPERTIES COMPILE_FLAGS "/openmp")
SET_PROPERTY(TARGET SSR_DA PROPERTY FOLDER "modules/hydrology_longterm")