#include "opencl_handle.h"

std::unique_ptr<OpenCLContext> OpenCLContext::instance_ = {};

OpenCLHandle::OpenCLHandle() {
  // TODO
}

OpenCLHandle::~OpenCLHandle() { OpenCLContext::CheckRefCount(); }

OpenCLContext::OpenCLContext() {
  // TODO
}

OpenCLContext::~OpenCLContext() {
  // TODO
}

OpenCLHandle::Ptr OpenCLContext::GetHandle() {
  if (!instance_) {
    // cannot use make_unique due to private constructor
    instance_ = std::unique_ptr<OpenCLContext>(new OpenCLContext());
  }

  auto strong_handle = instance_->weak_handle_.lock();
  if (strong_handle) {
    return strong_handle;
  }
  // cannot use make_shared due to private constructor
  strong_handle = std::shared_ptr<OpenCLHandle>(new OpenCLHandle());
  instance_->weak_handle_ = strong_handle;

  return strong_handle;
}

void OpenCLContext::CheckRefCount() {
  if (instance_) {
    if (instance_->weak_handle_.use_count() <= 1) {
      // Last shared_ptr is destructing, cleanup context by calling destructor
      instance_.reset();
    }
  }
}
