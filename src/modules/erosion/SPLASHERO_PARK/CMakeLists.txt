LINK_DIRECTORIES(${PROJECT_BINARY_DIR})
INCLUDE_DIRECTORIES(../../../base/util ../../../base/module_setting)
SET(SRC_LIST ../../../base/util/SimulationModule.cpp SplashEro_Park.cpp api.cpp)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
ADD_LIBRARY(SPLASHERO_PARK SHARED ${SRC_LIST})
TARGET_LINK_LIBRARIES(SPLASHERO_PARK util module_setting)
#TARGET_LINK_LIBRARIES(SPLASHERO_PARK libutil.so)
SET_TARGET_PROPERTIES( SPLASHERO_PARK PROPERTIES COMPILE_FLAGS "/openmp")
SET_PROPERTY(TARGET SPLASHERO_PARK PROPERTY FOLDER "modules/erosion")