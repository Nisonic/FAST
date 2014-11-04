#include "SurfaceExtraction.hpp"
#include "DeviceManager.hpp"
#include "Image.hpp"
#include "Utility.hpp"
#include "HelperFunctions.hpp"
#include "SceneGraph.hpp"

namespace fast {

void SurfaceExtraction::setThreshold(float threshold) {
    mThreshold = threshold;
    mIsModified = true;
}

void SurfaceExtraction::setInput(ImageData::pointer input) {
    mInput = input;
    setParent(mInput);
    if(input->isDynamicData()) {
        mOutput = DynamicSurface::New();
        DynamicSurface::pointer(mOutput)->setStreamer(DynamicImage::pointer(mInput)->getStreamer());
    } else {
        mOutput = Surface::New();
        input->retain(mDevice);
    }
    mOutput->setSource(mPtr.lock());
    mIsModified = true;
}

SurfaceData::pointer SurfaceExtraction::getOutput() {
    if(!mOutput.isValid()) {
        throw Exception("Must call setInput before getOutput in GaussianSmoothingFilter");
    }
    return mOutput;
}

void SurfaceExtraction::setDevice(OpenCLDevice::pointer device) {
    mDevice = device;
}

inline unsigned int getRequiredHistogramPyramidSize(Image::pointer input) {
    unsigned int largestSize = fast::max(fast::max(input->getWidth(), input->getHeight()), input->getDepth());
    int i = 1;
    while(largestSize > pow(2,i)) {
        i++;
    }
    return (unsigned int)pow(2,i);
}

void SurfaceExtraction::execute() {
    if(!mInput.isValid())
        throw Exception("No input was given to SurfaceRenderer");

    Image::pointer input;
    if(mInput->isDynamicData()) {
        input = DynamicImage::pointer(mInput)->getNextFrame();
    } else {
        input = mInput;
    }

    if(input->getDimensions() != 3)
        throw Exception("The SurfaceExtraction object only supports 3D images");

#if defined(__APPLE__) || defined(__MACOSX)
    const bool writingTo3DTextures = false;
#else
    const bool writingTo3DTextures = mDevice->getDevice().getInfo<CL_DEVICE_EXTENSIONS>().find("cl_khr_3d_image_writes") != std::string::npos;
#endif
    cl::Context clContext = mDevice->getContext();
    const unsigned int SIZE = getRequiredHistogramPyramidSize(input);

    if(mHPSize != SIZE) {
        // Have to recreate the HP
        images.clear();
        buffers.clear();
        std::string kernelFilename;
        // create new HP (if necessary)
        if(writingTo3DTextures) {
            kernelFilename = "SurfaceExtraction.cl";
            // Create images for the HistogramPyramid
            int bufferSize = SIZE;
            cl_channel_order order1, order2;
            if(mDevice->isImageFormatSupported(CL_R, CL_UNSIGNED_INT8, CL_MEM_OBJECT_IMAGE3D) && mDevice->isImageFormatSupported(CL_RG, CL_UNSIGNED_INT8, CL_MEM_OBJECT_IMAGE3D)) {
            	order1 = CL_R;
            	order2 = CL_RG;
            } else {
            	order1 = CL_RGBA;
            	order2 = CL_RGBA;
            }

            // Make the two first buffers use INT8
            images.push_back(cl::Image3D(clContext, CL_MEM_READ_WRITE, cl::ImageFormat(order2, CL_UNSIGNED_INT8), input->getWidth(), input->getHeight(), input->getDepth()));
            bufferSize /= 2;
            images.push_back(cl::Image3D(clContext, CL_MEM_READ_WRITE, cl::ImageFormat(order1, CL_UNSIGNED_INT8), bufferSize, bufferSize, bufferSize));
            bufferSize /= 2;
            // And the third, fourth and fifth INT16
            images.push_back(cl::Image3D(clContext, CL_MEM_READ_WRITE, cl::ImageFormat(order1, CL_UNSIGNED_INT16), bufferSize, bufferSize, bufferSize));
            bufferSize /= 2;
            images.push_back(cl::Image3D(clContext, CL_MEM_READ_WRITE, cl::ImageFormat(order1, CL_UNSIGNED_INT16), bufferSize, bufferSize, bufferSize));
            bufferSize /= 2;
            images.push_back(cl::Image3D(clContext, CL_MEM_READ_WRITE, cl::ImageFormat(order1, CL_UNSIGNED_INT16), bufferSize, bufferSize, bufferSize));
            bufferSize /= 2;
            // The rest will use INT32
            for(int i = 5; i < (log2((float)SIZE)); i ++) {
                if(bufferSize == 1)
                    bufferSize = 2; // Image cant be 1x1x1
                images.push_back(cl::Image3D(clContext, CL_MEM_READ_WRITE, cl::ImageFormat(order1, CL_UNSIGNED_INT32), bufferSize, bufferSize, bufferSize));
                bufferSize /= 2;
            }

            // If writing to 3D textures is not supported we to create buffers to write to
       } else {
            kernelFilename = "SurfaceExtraction_no_3d_write.cl";
            int bufferSize = SIZE*SIZE*SIZE;
            buffers.push_back(cl::Buffer(clContext, CL_MEM_READ_WRITE, sizeof(char)*bufferSize));
            bufferSize /= 8;
            buffers.push_back(cl::Buffer(clContext, CL_MEM_READ_WRITE, sizeof(char)*bufferSize));
            bufferSize /= 8;
            buffers.push_back(cl::Buffer(clContext, CL_MEM_READ_WRITE, sizeof(short)*bufferSize));
            bufferSize /= 8;
            buffers.push_back(cl::Buffer(clContext, CL_MEM_READ_WRITE, sizeof(short)*bufferSize));
            bufferSize /= 8;
            buffers.push_back(cl::Buffer(clContext, CL_MEM_READ_WRITE, sizeof(short)*bufferSize));
            bufferSize /= 8;
            for(int i = 5; i < (log2((float)SIZE)); i ++) {
                buffers.push_back(cl::Buffer(clContext, CL_MEM_READ_WRITE, sizeof(int)*bufferSize));
                bufferSize /= 8;
            }

            cubeIndexesBuffer = cl::Buffer(clContext, CL_MEM_WRITE_ONLY, sizeof(char)*SIZE*SIZE*SIZE);
            cubeIndexesImage = cl::Image3D(clContext, CL_MEM_READ_ONLY,
                    cl::ImageFormat(CL_R, CL_UNSIGNED_INT8),
                    SIZE, SIZE, SIZE);
        }

        // Compile program
        mHPSize = SIZE;
        char buffer[255];
        sprintf(buffer,"-DSIZE=%d", SIZE);
        std::string buildOptions(buffer);
        if(input->getDataType() == TYPE_FLOAT) {
            buildOptions += " -DTYPE_FLOAT";
        } else if(input->getDataType() == TYPE_INT8 || input->getDataType() == TYPE_INT16) {
            buildOptions += " -DTYPE_INT";
        } else {
            buildOptions += " -DTYPE_UINT";
        }
#if defined(__APPLE__) || defined(__MACOSX)
        buildOptions += " -DMAC_HACK";
#endif
        int programNr = mDevice->createProgramFromSource(std::string(FAST_SOURCE_DIR) + "/Algorithms/SurfaceExtraction/" + kernelFilename, buildOptions);
        program = mDevice->getProgram(programNr);
    }

    cl::Kernel constructHPLevelKernel = cl::Kernel(program, "constructHPLevel");
    cl::Kernel classifyCubesKernel = cl::Kernel(program, "classifyCubes");
    cl::Kernel traverseHPKernel = cl::Kernel(program, "traverseHP");

    OpenCLImageAccess3D access = input->getOpenCLImageAccess3D(ACCESS_READ, mDevice);
    cl::Image3D* clImage = access.get();

    // update scalar field
    if(writingTo3DTextures) {
        classifyCubesKernel.setArg(0, images[0]);
        classifyCubesKernel.setArg(1, *clImage);
        classifyCubesKernel.setArg(2, mThreshold);
        mDevice->getCommandQueue().enqueueNDRangeKernel(
                classifyCubesKernel,
                cl::NullRange,
                cl::NDRange(SIZE, SIZE, SIZE),
                cl::NullRange
        );
    } else {
        classifyCubesKernel.setArg(0, buffers[0]);
        classifyCubesKernel.setArg(1, cubeIndexesBuffer);
        classifyCubesKernel.setArg(2, *clImage);
        classifyCubesKernel.setArg(3, mThreshold);
        mDevice->getCommandQueue().enqueueNDRangeKernel(
                classifyCubesKernel,
                cl::NullRange,
                cl::NDRange(SIZE, SIZE, SIZE),
                cl::NullRange
        );

        cl::size_t<3> offset;
        offset[0] = 0;
        offset[1] = 0;
        offset[2] = 0;
        cl::size_t<3> region;
        region[0] = SIZE;
        region[1] = SIZE;
        region[2] = SIZE;

        // Copy buffer to image
        mDevice->getCommandQueue().enqueueCopyBufferToImage(cubeIndexesBuffer, cubeIndexesImage, 0, offset, region);
    }

    // Construct HP
    cl::CommandQueue queue = mDevice->getCommandQueue();

    if(writingTo3DTextures) {
        // Run base to first level
        constructHPLevelKernel.setArg(0, images[0]);
        constructHPLevelKernel.setArg(1, images[1]);

        queue.enqueueNDRangeKernel(
            constructHPLevelKernel,
            cl::NullRange,
            cl::NDRange(SIZE/2, SIZE/2, SIZE/2),
            cl::NullRange
        );

        int previous = SIZE / 2;
        // Run level 2 to top level
        for(int i = 1; i < log2((float)SIZE)-1; i++) {
            constructHPLevelKernel.setArg(0, images[i]);
            constructHPLevelKernel.setArg(1, images[i+1]);
            previous /= 2;
            queue.enqueueNDRangeKernel(
                constructHPLevelKernel,
                cl::NullRange,
                cl::NDRange(previous, previous, previous),
                cl::NullRange
            );
        }
    } else {
        cl::Kernel constructHPLevelCharCharKernel(program, "constructHPLevelCharChar");
        cl::Kernel constructHPLevelCharShortKernel(program, "constructHPLevelCharShort");
        cl::Kernel constructHPLevelShortShortKernel(program, "constructHPLevelShortShort");
        cl::Kernel constructHPLevelShortIntKernel(program, "constructHPLevelShortInt");

        // Run base to first level
        constructHPLevelCharCharKernel.setArg(0, buffers[0]);
        constructHPLevelCharCharKernel.setArg(1, buffers[1]);

        queue.enqueueNDRangeKernel(
            constructHPLevelCharCharKernel,
            cl::NullRange,
            cl::NDRange(SIZE/2, SIZE/2, SIZE/2),
            cl::NullRange
        );

        int previous = SIZE / 2;

        constructHPLevelCharShortKernel.setArg(0, buffers[1]);
        constructHPLevelCharShortKernel.setArg(1, buffers[2]);

        queue.enqueueNDRangeKernel(
            constructHPLevelCharShortKernel,
            cl::NullRange,
            cl::NDRange(previous/2, previous/2, previous/2),
            cl::NullRange
        );

        previous /= 2;

        constructHPLevelShortShortKernel.setArg(0, buffers[2]);
        constructHPLevelShortShortKernel.setArg(1, buffers[3]);

        queue.enqueueNDRangeKernel(
            constructHPLevelShortShortKernel,
            cl::NullRange,
            cl::NDRange(previous/2, previous/2, previous/2),
            cl::NullRange
        );

        previous /= 2;

        constructHPLevelShortShortKernel.setArg(0, buffers[3]);
        constructHPLevelShortShortKernel.setArg(1, buffers[4]);

        queue.enqueueNDRangeKernel(
            constructHPLevelShortShortKernel,
            cl::NullRange,
            cl::NDRange(previous/2, previous/2, previous/2),
            cl::NullRange
        );

        previous /= 2;

        constructHPLevelShortIntKernel.setArg(0, buffers[4]);
        constructHPLevelShortIntKernel.setArg(1, buffers[5]);

        queue.enqueueNDRangeKernel(
            constructHPLevelShortIntKernel,
            cl::NullRange,
            cl::NDRange(previous/2, previous/2, previous/2),
            cl::NullRange
        );

        previous /= 2;

        // Run level 2 to top level
        for(int i = 5; i < log2((float)SIZE)-1; i++) {
            constructHPLevelKernel.setArg(0, buffers[i]);
            constructHPLevelKernel.setArg(1, buffers[i+1]);
            previous /= 2;
            queue.enqueueNDRangeKernel(
                constructHPLevelKernel,
                cl::NullRange,
                cl::NDRange(previous, previous, previous),
                cl::NullRange
            );
        }
    }

    // Create VBO using the sum
    // Read top of histoPyramid an use this size to allocate VBO below
    unsigned int totalSum = 0;
    if(writingTo3DTextures) {
    	int* sum;
        if(mDevice->isImageFormatSupported(CL_R, CL_UNSIGNED_INT32, CL_MEM_OBJECT_IMAGE3D)) {
            sum = new int[8];
        } else {
        	// A 4 channel texture/image has been used
        	sum = new int[8*4];
        }
        cl::size_t<3> origin = oul::createOrigoRegion();
        cl::size_t<3> region = oul::createRegion(2,2,2);
        queue.enqueueReadImage(images[images.size()-1], CL_TRUE, origin, region, 0, 0, sum);
        if(mDevice->isImageFormatSupported(CL_R, CL_UNSIGNED_INT8, CL_MEM_OBJECT_IMAGE3D)) {
            totalSum = sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5] + sum[6] + sum[7] ;
        } else {
            totalSum = sum[0] + sum[4] + sum[8] + sum[12] + sum[16] + sum[20] + sum[24] + sum[28] ;
        }
    } else {
        int* sum = new int[8];
        queue.enqueueReadBuffer(buffers[buffers.size()-1], CL_TRUE, 0, sizeof(int)*8, sum);
        totalSum = sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5] + sum[6] + sum[7] ;
    }

    if(totalSum == 0) {
        std::cout << "No triangles were extracted. Check isovalue." << std::endl;
        return;
    }
    std::cout << "Sum of triangles is " << totalSum << std::endl;

    Surface::pointer output;
    if(mInput->isDynamicData()) {
        output = Surface::New();
        DynamicSurface::pointer(mOutput)->addFrame(output);
    } else {
        output = Surface::pointer(mOutput);
    }
    output->create(totalSum);

    // Traverse HP to create triangles and put them in the VBO
    // Make OpenCL buffer from OpenGL buffer
    unsigned int i = 0;
    if(writingTo3DTextures) {
        traverseHPKernel.setArg(0, *clImage);
        for(i = 0; i < images.size(); i++) {
            traverseHPKernel.setArg(i+1, images[i]);
        }
        i += 1;
    } else {
        traverseHPKernel.setArg(0, *clImage);
        traverseHPKernel.setArg(1, cubeIndexesImage);
        for(i = 0; i < buffers.size(); i++) {
            traverseHPKernel.setArg(i+2, buffers[i]);
        }
        i += 2;
    }

    VertexBufferObjectAccess VBOaccess = output->getVertexBufferObjectAccess(ACCESS_READ_WRITE, mDevice);
    GLuint* VBO_ID = VBOaccess.get();
    cl::BufferGL VBOBuffer = cl::BufferGL(mDevice->getContext(), CL_MEM_WRITE_ONLY, *VBO_ID);
    traverseHPKernel.setArg(i, VBOBuffer);
    traverseHPKernel.setArg(i+1, mThreshold);
    traverseHPKernel.setArg(i+2, totalSum);
    // TODO should set spacing to this kernel here
    //cl_event syncEvent = clCreateEventFromGLsyncKHR((cl_context)context(), (cl_GLsync)glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0), 0);
    //glFinish();
    std::vector<cl::Memory> v;
    v.push_back(VBOBuffer);
    //vector<Event> events;
    //Event e;
    //events.push_back(Event(syncEvent));
    queue.enqueueAcquireGLObjects(&v);

    // Increase the global_work_size so that it is divideable by 64
    int global_work_size = totalSum + 64 - (totalSum - 64*(totalSum / 64));
    // Run a NDRange kernel over this buffer which traverses back to the base level
    queue.enqueueNDRangeKernel(traverseHPKernel, cl::NullRange, cl::NDRange(global_work_size), cl::NDRange(64));

    cl::Event traversalEvent;
    queue.enqueueReleaseGLObjects(&v, 0, &traversalEvent);
//  traversalSync = glCreateSyncFromCLeventARB((cl_context)context(), (cl_event)traversalEvent(), 0); // Need the GL_ARB_cl_event extension
    queue.finish();
    mOutput->updateModifiedTimestamp();
    SceneGraph::getInstance().setParentNode(output, input);
    BoundingBox box = mInput->getBoundingBox();
    output->setBoundingBox(box);

    if(!mInput->isDynamicData())
        mInput->release(mDevice);
}

SurfaceExtraction::SurfaceExtraction() {
    mDevice = DeviceManager::getInstance().getDefaultComputationDevice();
    mThreshold = 0.0f;
    mHPSize = 0;
    mOutput = Surface::New();
}


} // end namespace fast
