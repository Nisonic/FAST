#include "OpenCLBufferAccess.hpp"
#include <iostream>
#include "FAST/Data/Image.hpp"
#include "FAST/ExecutionDevice.hpp"

namespace fast {

cl::Buffer* OpenCLBufferAccess::get() const {
    return mBuffer;
}

OpenCLBufferAccess::OpenCLBufferAccess(cl::Buffer* buffer, SharedPointer<OpenCLDevice> device, SharedPointer<Image> image) {
    // Copy the image
    mBuffer = new cl::Buffer(*buffer);
    mIsDeleted = false;
    mImage = image;
    mDevice = device;
}

void OpenCLBufferAccess::release() {
    if(!mIsDeleted) {
        delete mBuffer;
        mBuffer = new cl::Buffer(); // assign a new blank object
        mIsDeleted = true;
    }
	mImage->OpenCLBufferAccessFinished(mDevice);
}

OpenCLBufferAccess::~OpenCLBufferAccess() {
    if(!mIsDeleted)
        release();
}

}
