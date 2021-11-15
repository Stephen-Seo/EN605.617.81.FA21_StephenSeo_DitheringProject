#ifndef IGPUP_DITHERING_PROJECT_OPENCL_H_
#define IGPUP_DITHERING_PROJECT_OPENCL_H_

#include <memory>

class OpenCLHandle {
 public:
  typedef std::shared_ptr<OpenCLHandle> Ptr;
  typedef std::weak_ptr<OpenCLHandle> WeakPtr;

  ~OpenCLHandle();

  // no copy
  OpenCLHandle(const OpenCLHandle &other) = delete;
  OpenCLHandle &operator=(const OpenCLHandle &other) = delete;

  // allow move
  OpenCLHandle(OpenCLHandle &&other) = default;
  OpenCLHandle &operator=(OpenCLHandle &&other) = default;

  // TODO add functions here that allow creating/deleting/using kernel function
  // programs

 private:
  friend class OpenCLContext;

  OpenCLHandle();
};

class OpenCLContext {
 public:
  ~OpenCLContext();

  // no copy
  OpenCLContext(const OpenCLContext &other) = delete;
  OpenCLContext &operator=(const OpenCLContext &other) = delete;

  // no move
  OpenCLContext(OpenCLContext &&other) = delete;
  OpenCLContext &operator=(OpenCLContext &&other) = delete;

  OpenCLHandle::Ptr GetHandle();

 private:
  friend class OpenCLHandle;

  OpenCLContext();

  static std::unique_ptr<OpenCLContext> instance_;
  OpenCLHandle::WeakPtr weak_handle_;

  static void CheckRefCount();
};

#endif
