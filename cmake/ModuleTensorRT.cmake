if(FAST_MODULE_TensorRT)
    message("-- Enabling TensorRT inference engine module")
    find_package(TensorRT 7 REQUIRED)
    find_package(CUDA REQUIRED)
endif()