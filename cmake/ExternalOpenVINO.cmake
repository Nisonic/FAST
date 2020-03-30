# Download and build OpenVINO

include(${PROJECT_SOURCE_DIR}/cmake/Externals.cmake)

if(WIN32)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

message("---------------------------------------------------------------------------")
message("   CMAKE_BUILD_TYPE = " ${CMAKE_BUILD_TYPE})
message("---------------------------------------------------------------------------")

if (CMAKE_BUILD_TYPE)
    if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        set(INFERENCE_ENGINE_LIB inference_engined.lib)
        set(INFERENCE_ENGINE_DLL inference_engined.dll)
        set(CLDNNPLUGIN_DLL clDNNPlugind.dll)
        set(MKLDNNPLUGIN_DLL MKLDNNPlugind.dll)
        set(MYRIADPLUGIN_DLL myriadPlugind.dll)
        set(NGRAPH_DLL ngraphd.dll)
    else()
        set(INFERENCE_ENGINE_LIB inference_engine.lib)
        set(INFERENCE_ENGINE_DLL inference_engine.dll)
        set(CLDNNPLUGIN_DLL clDNNPlugin.dll)
        set(MKLDNNPLUGIN_DLL MKLDNNPlugin.dll)
        set(MYRIADPLUGIN_DLL myriadPlugin.dll)
        set(NGRAPH_DLL ngraph.dll)
    endif()
endif()    

ExternalProject_Add(OpenVINO
        PREFIX ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO
        GIT_REPOSITORY "https://github.com/opencv/dldt.git"
        GIT_TAG "2020.1"
        UPDATE_COMMAND
            git submodule update --init --recursive
        CMAKE_ARGS
            -DENABLE_BEH_TESTS=OFF
            -DENABLE_C=OFF
            -DENABLE_FUNCTIONAL_TESTS=OFF
            -DENABLE_OPENCV:BOOL=OFF
            -DENABLE_PROFILING_ITT:BOOL=OFF
            -DENABLE_SAMPLES:BOOL=OFF
            -DENABLE_CPPCHECK:BOOL=OFF
            -DENABLE_CPPLINT:BOOL=OFF
            -DBUILD_TESTING:BOOL=OFF
            #-DBUILD_SHARED_LIBS:BOOL=ON
            -DENABLE_VPU:BOOL=ON
        CMAKE_CACHE_ARGS
            -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
            -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
            -DCMAKE_INSTALL_MESSAGE:BOOL=LAZY
            -DCMAKE_INSTALL_PREFIX:STRING=${FAST_EXTERNAL_INSTALL_DIR}
        BUILD_COMMAND
            ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target inference_engine COMMAND
            ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target clDNNPlugin COMMAND
            ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target myriadPlugin COMMAND
            ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target MKLDNNPlugin
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/${CMAKE_BUILD_TYPE}/${INFERENCE_ENGINE_DLL} ${FAST_EXTERNAL_INSTALL_DIR}/bin/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/${CMAKE_BUILD_TYPE}/${INFERENCE_ENGINE_LIB} ${FAST_EXTERNAL_INSTALL_DIR}/lib/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/${CMAKE_BUILD_TYPE}/${CLDNNPLUGIN_DLL} ${FAST_EXTERNAL_INSTALL_DIR}/bin/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/${CMAKE_BUILD_TYPE}/${MKLDNNPLUGIN_DLL} ${FAST_EXTERNAL_INSTALL_DIR}/bin/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/${CMAKE_BUILD_TYPE}/${MYRIADPLUGIN_DLL} ${FAST_EXTERNAL_INSTALL_DIR}/bin/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/${CMAKE_BUILD_TYPE}/${NGRAPH_DLL} ${FAST_EXTERNAL_INSTALL_DIR}/bin/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/${CMAKE_BUILD_TYPE}/plugins.xml ${FAST_EXTERNAL_INSTALL_DIR}/bin/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/inference-engine/temp/tbb/bin/tbb.dll ${FAST_EXTERNAL_INSTALL_DIR}/bin/ COMMAND
            ${CMAKE_COMMAND} -E copy_directory ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/inference-engine/include/ ${FAST_EXTERNAL_INSTALL_DIR}/include/openvino/
        )
else()
ExternalProject_Add(OpenVINO
        PREFIX ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO
        GIT_REPOSITORY "https://github.com/opencv/dldt.git"
        GIT_TAG "2020.1"
        UPDATE_COMMAND
            git submodule update --init --recursive
        CMAKE_ARGS
            -DENABLE_BEH_TESTS=OFF
            -DENABLE_C=OFF
            -DENABLE_FUNCTIONAL_TESTS=OFF
            -DENABLE_OPENCV:BOOL=OFF
            -DENABLE_PROFILING_ITT:BOOL=OFF
            -DENABLE_SAMPLES:BOOL=OFF
            -DENABLE_CPPCHECK:BOOL=OFF
            -DENABLE_CPPLINT:BOOL=OFF
            -DBUILD_TESTING:BOOL=OFF
            #-DBUILD_SHARED_LIBS:BOOL=ON
            -DENABLE_VPU:BOOL=ON
        CMAKE_CACHE_ARGS
            -DCMAKE_BUILD_TYPE:STRING=	
            -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
            -DCMAKE_INSTALL_MESSAGE:BOOL=LAZY
            -DCMAKE_INSTALL_PREFIX:STRING=${FAST_EXTERNAL_INSTALL_DIR}
        BUILD_COMMAND
            make -j4 inference_engine clDNNPlugin myriadPlugin MKLDNNPlugin
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/Release/lib/libinference_engine.so ${FAST_EXTERNAL_INSTALL_DIR}/lib/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/Release/lib/libclDNNPlugin.so ${FAST_EXTERNAL_INSTALL_DIR}/lib/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/Release/lib/libMKLDNNPlugin.so ${FAST_EXTERNAL_INSTALL_DIR}/lib/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/Release/lib/libmyriadPlugin.so ${FAST_EXTERNAL_INSTALL_DIR}/lib/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/Release/lib/libngraph.so ${FAST_EXTERNAL_INSTALL_DIR}/lib/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/bin/intel64/Release/lib/plugins.xml ${FAST_EXTERNAL_INSTALL_DIR}/lib/ COMMAND
            #${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/inference-engine/bin/intel64/Release/lib/libGNAPlugin.so ${FAST_EXTERNAL_INSTALL_DIR}/lib/ COMMAND
            #${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/inference-engine/bin/intel64/Release/lib/libHeteroPlugin.so ${FAST_EXTERNAL_INSTALL_DIR}/lib/ COMMAND
            ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/inference-engine/temp/tbb/lib/libtbb.so.2 ${FAST_EXTERNAL_INSTALL_DIR}/lib/ COMMAND
            ${CMAKE_COMMAND} -E copy_directory ${FAST_EXTERNAL_BUILD_DIR}/OpenVINO/src/OpenVINO/inference-engine/include/ ${FAST_EXTERNAL_INSTALL_DIR}/include/openvino/
        )
endif()

list(APPEND FAST_INCLUDE_DIRS ${FAST_EXTERNAL_INSTALL_DIR}/include/openvino/)
