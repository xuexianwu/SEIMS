LINK_DIRECTORIES(${PROJECT_BINARY_DIR})
INCLUDE_DIRECTORIES(../../../base/util ../../../base/module_setting)
SET(SRC_LIST ../../../base/util/SimulationModule.cpp PER_STR.cpp api.cpp)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
ADD_LIBRARY(PER_STR SHARED ${SRC_LIST})
#TARGET_LINK_LIBRARIES(PER_STR libutil.a)
TARGET_LINK_LIBRARIES(PER_STR util module_setting)
SET_TARGET_PROPERTIES( PER_STR PROPERTIES COMPILE_FLAGS "/openmp")
SET_PROPERTY(TARGET PER_STR PROPERTY FOLDER "modules/hydrology_longterm")