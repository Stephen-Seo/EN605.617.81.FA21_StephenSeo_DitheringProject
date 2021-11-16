#include "opencl_handle.h"

#include <fstream>
#include <iostream>
#include <vector>

#include <CL/cl.h>

OpenCLContext::Ptr OpenCLContext::instance_ = {};

OpenCLContext::OpenCLHandle::OpenCLHandle()
    : opencl_ptr_(), kernels_(), kernel_counter_(0) {}

OpenCLContext::OpenCLHandle::~OpenCLHandle() {
  CleanupAllKernels();
  OpenCLContext::CheckRefCount();
}

bool OpenCLContext::OpenCLHandle::IsValid() const {
  auto context = opencl_ptr_.lock();
  if (!context) {
    return false;
  }

  return context->IsValid();
}

KernelID OpenCLContext::OpenCLHandle::CreateKernelFromSource(
    const std::string &kernel_fn, const char *kernel_name) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return 0;
  }

  cl_int err_num;
  KernelInfo kernel_info = {nullptr, nullptr, {}, 0};

  OpenCLContext::Ptr context_ptr = opencl_ptr_.lock();
  if (!context_ptr) {
    std::cout << "ERROR: OpenCLHandle: OpenCLContext is not initialized"
              << std::endl;
    return 0;
  }

  const char *source_c_str = kernel_fn.c_str();
  kernel_info.program_ = clCreateProgramWithSource(
      context_ptr->context_, 1, &source_c_str, nullptr, &err_num);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle: Failed to create program from source"
              << std::endl;
    return 0;
  }

  err_num = clBuildProgram(kernel_info.program_, 0, nullptr, nullptr, nullptr,
                           nullptr);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle: Failed to compile kernel" << std::endl;
    std::vector<char> build_log;
    build_log.resize(16384);
    build_log.at(16383) = 0;
    clGetProgramBuildInfo(kernel_info.program_, context_ptr->device_id_,
                          CL_PROGRAM_BUILD_LOG, build_log.size(),
                          build_log.data(), nullptr);
    std::cout << build_log.data();
    clReleaseProgram(kernel_info.program_);
    return 0;
  }

  kernel_info.kernel_ =
      clCreateKernel(kernel_info.program_, kernel_name, &err_num);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle: Failed to create kernel object from "
              << "source" << std::endl;
    clReleaseProgram(kernel_info.program_);
    return 0;
  }

  KernelID id;
  do {
    id = ++kernel_counter_;
  } while (id == 0 || kernels_.find(id) != kernels_.end());

  kernels_.insert({id, kernel_info});

  return id;
}

KernelID OpenCLContext::OpenCLHandle::CreateKernelFromSource(
    const char *kernel_fn, const char *kernel_name) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return 0;
  }
  return CreateKernelFromSource(std::string(kernel_fn), kernel_name);
}

KernelID OpenCLContext::OpenCLHandle::CreateKernelFromFile(
    const std::string &filename, const char *kernel_name) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return 0;
  }
  std::string source;
  {
    char buf[1024];
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
      return 0;
    }

    while (ifs.good()) {
      ifs.read(buf, 1024);
      source.append(buf, ifs.gcount());
    }

    if (source.empty()) {
      return 0;
    }
  }
  return CreateKernelFromSource(source, kernel_name);
}

KernelID OpenCLContext::OpenCLHandle::CreateKernelFromFile(
    const char *filename, const char *kernel_name) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return 0;
  }
  return CreateKernelFromFile(std::string(filename), kernel_name);
}

BufferID OpenCLContext::OpenCLHandle::CreateKernelBuffer(KernelID kernel_id,
                                                         cl_mem_flags flags,
                                                         std::size_t buf_size,
                                                         void *host_ptr) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLHandle::CreateKernelBuffer: OpenCLContext is "
                 "not initialized"
              << std::endl;
    return 0;
  }

  auto kernel_info_iter = kernels_.find(kernel_id);
  if (kernel_info_iter == kernels_.end()) {
    std::cout
        << "ERROR: OpenCLHandle::CreateKernelBuffer: Got Invalid kernel_id"
        << std::endl;
    return 0;
  }

  auto opencl_context = opencl_ptr_.lock();
  if (!opencl_context) {
    std::cout << "ERROR: OpenCLHandle::CreateKernelBuffer: OpenCLContext is "
                 "not initialized"
              << std::endl;
    return 0;
  }

  cl_int err_num;
  cl_mem mem_object;

  mem_object = clCreateBuffer(opencl_context->context_, flags, buf_size,
                              host_ptr, &err_num);
  if (err_num != CL_SUCCESS) {
    std::cout
        << "ERROR: OpenCLHandle::CreateKernelBuffer: Failed to create buffer"
        << std::endl;
    return 0;
  }

  BufferID buffer_id;
  auto *buffer_map = &kernel_info_iter->second.mem_objects_;
  do {
    buffer_id = ++kernel_info_iter->second.buffer_id_counter_;
  } while (buffer_id == 0 || buffer_map->find(buffer_id) != buffer_map->end());

  buffer_map->insert({buffer_id, {mem_object, buf_size}});

  return buffer_id;
}

bool OpenCLContext::OpenCLHandle::SetKernelBufferData(KernelID kernel_id,
                                                      BufferID buffer_id,
                                                      std::size_t data_size,
                                                      void *data_ptr) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }
  auto kernel_info_iter = kernels_.find(kernel_id);
  if (kernel_info_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::SetKernelBufferData: Invalid KernelID"
              << std::endl;
    return false;
  }

  auto buffer_info_iter = kernel_info_iter->second.mem_objects_.find(buffer_id);
  if (buffer_info_iter == kernel_info_iter->second.mem_objects_.end()) {
    std::cout << "ERROR: OpenCLHandle::SetKernelBufferData: Invalid BufferID"
              << std::endl;
    return false;
  }

  if (buffer_info_iter->second.size < data_size) {
    std::cout
        << "ERROR: OpenCLHandle::SetKernelBufferData: device buffer has size "
        << buffer_info_iter->second.size << ", but given data_size is "
        << data_size << " (error due to larger size)" << std::endl;
    return false;
  } else if (buffer_info_iter->second.size > data_size) {
    std::cout
        << "WARNING: OpenCLHandle::SetKernelBufferData: device buffer has size "
        << buffer_info_iter->second.size << ", but given data_size is "
        << data_size << " (warning due to smaller size)" << std::endl;
  }

  auto context_ptr = opencl_ptr_.lock();
  if (!context_ptr) {
    std::cout << "ERROR: OpenCLHandle::SetKernelBufferData: OpenCLContext not "
                 "initialized"
              << std::endl;
    return false;
  }

  cl_int err_num = clEnqueueWriteBuffer(
      context_ptr->queue_, buffer_info_iter->second.mem, CL_TRUE, 0,
      buffer_info_iter->second.size, data_ptr, 0, nullptr, nullptr);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle::SetKernelBufferData: Failed to assign "
                 "data to device buffer"
              << std::endl;
    return false;
  }

  return true;
}

bool OpenCLContext::OpenCLHandle::AssignKernelBuffer(KernelID kernel_id,
                                                     unsigned int idx,
                                                     BufferID buffer_id) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }
  auto kernel_iter = kernels_.find(kernel_id);
  if (kernel_iter == kernels_.end()) {
    std::cout
        << "ERROR: OpenCLHandle::AssignKernelBuffer: no kernel with given id"
        << std::endl;
    return false;
  }

  auto *buffer_map = &kernel_iter->second.mem_objects_;
  auto buffer_info_iter = buffer_map->find(buffer_id);
  if (buffer_info_iter == buffer_map->end()) {
    std::cout << "ERROR: OpenCLHandle::AssignKernelBuffer: no buffer in "
                 "kernel_info with given id"
              << std::endl;
    return false;
  }

  cl_int err_num;

  err_num = clSetKernelArg(kernel_iter->second.kernel_, idx, sizeof(cl_mem),
                           &buffer_info_iter->second.mem);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle::AssignKernelBuffer: failed to assign "
                 "buffer to kernel argument"
              << std::endl;
    return false;
  }

  return true;
}

bool OpenCLContext::OpenCLHandle::AssignKernelArgument(KernelID kernel_id,
                                                       unsigned int idx,
                                                       std::size_t data_size,
                                                       const void *data_ptr) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }
  auto iter = kernels_.find(kernel_id);
  if (iter == kernels_.end()) {
    std::cout
        << "ERROR: OpenCLHandle::AssignKernelArgument: no kernel with given id"
        << std::endl;
    return false;
  }

  auto context_ptr = opencl_ptr_.lock();
  if (!context_ptr) {
    std::cout << "ERROR: OpenCLHandle::AssignKernelArgument: OpenCLContext not "
                 "initialized"
              << std::endl;
    return false;
  }

  cl_int err_num;

  err_num = clSetKernelArg(iter->second.kernel_, idx, data_size, data_ptr);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle::AssignKernelArgument: Failure to set "
                 "kernel arg"
              << std::endl;
    return false;
  }

  return true;
}

std::array<std::size_t, 3> OpenCLContext::OpenCLHandle::GetGlobalWorkSize(
    KernelID kernel_id) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return {0, 0, 0};
  }
  std::array<std::size_t, 3> sizes = {0, 0, 0};

  auto context_ptr = opencl_ptr_.lock();
  if (!context_ptr) {
    std::cout << "ERROR: OpenCLHandle::GetGlobalWorkSize: OpenCLContext is not "
                 "initialized"
              << std::endl;
    return sizes;
  }

  auto kernel_iter = kernels_.find(kernel_id);
  if (kernel_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::GetGlobalWorkSize: Invalid kernel_id"
              << std::endl;
    return sizes;
  }

  cl_int err_num = clGetKernelWorkGroupInfo(
      kernel_iter->second.kernel_, context_ptr->device_id_,
      CL_KERNEL_GLOBAL_WORK_SIZE, sizeof(std::size_t) * 3, sizes.data(),
      nullptr);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle::GetGlobalWorkSize: Failed to query "
                 "CL_KERNEL_GLOBAL_WORK_SIZE"
              << std::endl;
    return {0, 0, 0};
  }

  return sizes;
}

std::size_t OpenCLContext::OpenCLHandle::GetWorkGroupSize(KernelID kernel_id) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return 0;
  }
  auto context_ptr = opencl_ptr_.lock();
  if (!context_ptr) {
    std::cout << "ERROR: OpenCLHandle::GetWorkGroupSize: OpenCLContext is not "
                 "initialized"
              << std::endl;
    return 0;
  }

  auto kernel_iter = kernels_.find(kernel_id);
  if (kernel_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::GetWorkGroupSize: Invalid kernel_id"
              << std::endl;
    return 0;
  }

  std::size_t size;
  cl_int err_num = clGetKernelWorkGroupInfo(
      kernel_iter->second.kernel_, context_ptr->device_id_,
      CL_KERNEL_WORK_GROUP_SIZE, sizeof(std::size_t), &size, nullptr);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle::GetWorkGroupSize: Failed to query "
                 "CL_KERNEL_WORK_GROUP_SIZE"
              << std::endl;
    return 0;
  }

  return size;
}

bool OpenCLContext::OpenCLHandle::ExecuteKernel(KernelID kernel_id,
                                                std::size_t global_work_size,
                                                std::size_t local_work_size,
                                                bool is_blocking) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }
  auto context_ptr = opencl_ptr_.lock();
  if (!context_ptr) {
    std::cout << "ERROR: OpenCLHandle::ExecuteKernel: OpenCLContext is not "
                 "initialized"
              << std::endl;
    return false;
  }

  auto kernel_iter = kernels_.find(kernel_id);
  if (kernel_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::ExecuteKernel: Invalid kernel_id"
              << std::endl;
    return false;
  }

  cl_event event;
  cl_int err_num = clEnqueueNDRangeKernel(
      context_ptr->queue_, kernel_iter->second.kernel_, 1, nullptr,
      &global_work_size, &local_work_size, 0, nullptr, &event);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle::ExecuteKernel: Failed to execute kernel"
              << std::endl;
    return false;
  }

  if (is_blocking) {
    err_num = clWaitForEvents(1, &event);
    if (err_num != CL_SUCCESS) {
      std::cout << "WARNING: OpenCLHandle::ExecuteKernel: Explicit wait on "
                   "kernel failed"
                << std::endl;
    }
  }

  clReleaseEvent(event);

  return true;
}

bool OpenCLContext::OpenCLHandle::GetBufferData(KernelID kernel_id,
                                                BufferID buffer_id,
                                                std::size_t out_size,
                                                void *data_out) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }

  auto context_ptr = opencl_ptr_.lock();
  if (!context_ptr) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
  }

  auto kernel_iter = kernels_.find(kernel_id);
  if (kernel_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::GetBufferData: Invalid kernel_id"
              << std::endl;
    return false;
  }

  auto buffer_iter = kernel_iter->second.mem_objects_.find(buffer_id);
  if (buffer_iter == kernel_iter->second.mem_objects_.end()) {
    std::cout << "ERROR: OpenCLHandle::GetBufferData: Invalid buffer_id"
              << std::endl;
    return false;
  }

  std::size_t size;
  if (buffer_iter->second.size > out_size) {
    std::cout << "WARNING: device memory size (" << buffer_iter->second.size
              << ") is greater than given size (" << out_size
              << "), defaulting to smaller of the two sizes" << std::endl;
    size = out_size;
  } else if (buffer_iter->second.size < out_size) {
    std::cout << "WARNING: device memory size (" << buffer_iter->second.size
              << ") is smaller than given size (" << out_size
              << "), defaulting to smaller of the two sizes" << std::endl;
    size = buffer_iter->second.size;
  } else {
    size = out_size;
  }

  cl_int err_num =
      clEnqueueReadBuffer(context_ptr->queue_, buffer_iter->second.mem, CL_TRUE,
                          0, size, data_out, 0, nullptr, nullptr);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle::GetBufferData: Failed to get device data"
              << std::endl;
    return false;
  }

  return true;
}

bool OpenCLContext::OpenCLHandle::CleanupBuffer(KernelID kernel_id,
                                                BufferID buffer_id) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }
  auto kernel_iter = kernels_.find(kernel_id);
  if (kernel_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::CleanupBuffer: Invalid kernel_id"
              << std::endl;
    return false;
  }

  auto buffer_iter = kernel_iter->second.mem_objects_.find(buffer_id);
  if (buffer_iter == kernel_iter->second.mem_objects_.end()) {
    std::cout << "ERROR: OpenCLHandle::CleanupBuffer: Invalid buffer_id"
              << std::endl;
    return false;
  }

  clReleaseMemObject(buffer_iter->second.mem);
  kernel_iter->second.mem_objects_.erase(buffer_iter);

  return true;
}

bool OpenCLContext::OpenCLHandle::CleanupKernel(KernelID id) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }
  auto iter = kernels_.find(id);
  if (iter == kernels_.end()) {
    return false;
  }

  for (auto buffer_iter = iter->second.mem_objects_.begin();
       buffer_iter != iter->second.mem_objects_.end(); ++buffer_iter) {
    clReleaseMemObject(buffer_iter->second.mem);
  }

  clReleaseKernel(iter->second.kernel_);
  clReleaseProgram(iter->second.program_);
  kernels_.erase(iter);
  return true;
}

void OpenCLContext::OpenCLHandle::CleanupAllKernels() {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return;
  }
  for (auto kernel_iter = kernels_.begin(); kernel_iter != kernels_.end();
       ++kernel_iter) {
    for (auto buffer_iter = kernel_iter->second.mem_objects_.begin();
         buffer_iter != kernel_iter->second.mem_objects_.end(); ++buffer_iter) {
      clReleaseMemObject(buffer_iter->second.mem);
    }
    clReleaseKernel(kernel_iter->second.kernel_);
    clReleaseProgram(kernel_iter->second.program_);
  }

  kernels_.clear();
}

OpenCLContext::OpenCLContext() : context_(nullptr), queue_(nullptr) {
  //////////////////// set up cl_context
  cl_int err_num;
  cl_uint num_platforms;
  cl_platform_id first_platform_id;

  err_num = clGetPlatformIDs(1, &first_platform_id, &num_platforms);
  if (err_num != CL_SUCCESS || num_platforms == 0) {
    std::cout << "ERROR: OpenCLContext: Failed to find any OpenCL platforms"
              << std::endl;
    return;
  }

  cl_context_properties context_properties[] = {
      CL_CONTEXT_PLATFORM, (cl_context_properties)first_platform_id, 0};

  context_ = clCreateContextFromType(context_properties, CL_DEVICE_TYPE_GPU,
                                     nullptr, nullptr, &err_num);

  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLContext: Failed to create GPU context, "
              << "trying CPU..." << std::endl;
    context_ = clCreateContextFromType(context_properties, CL_DEVICE_TYPE_CPU,
                                       nullptr, nullptr, &err_num);
    if (err_num != CL_SUCCESS) {
      std::cout << "ERROR: OpenCLContext: Failed to create CPU context"
                << std::endl;
      context_ = nullptr;
      return;
    }
  }
  //////////////////// end set up cl context

  //////////////////// set up command queue
  std::vector<cl_device_id> devices;
  std::size_t device_buffer_size = -1;

  err_num = clGetContextInfo(context_, CL_CONTEXT_DEVICES, 0, nullptr,
                             &device_buffer_size);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLContext: Failed to get device count"
              << std::endl;
    clReleaseContext(context_);
    context_ = nullptr;
    return;
  } else if (device_buffer_size == 0) {
    std::cout << "ERROR: OpenCLContext: No devices available" << std::endl;
    clReleaseContext(context_);
    context_ = nullptr;
    return;
  }

  devices.resize(device_buffer_size);
  err_num = clGetContextInfo(context_, CL_CONTEXT_DEVICES, device_buffer_size,
                             devices.data(), nullptr);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLContext: Failed to get devices" << std::endl;
    clReleaseContext(context_);
    context_ = nullptr;
    return;
  }

  // uses first available device
  queue_ = clCreateCommandQueue(context_, devices.at(0), 0, &err_num);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLContext: Failed to create command queue"
              << std::endl;
    clReleaseContext(context_);
    context_ = nullptr;
    return;
  }

  device_id_ = devices.at(0);
  //////////////////// end set up command queue
}

OpenCLContext::~OpenCLContext() {
  if (queue_) {
    clReleaseCommandQueue(queue_);
  }
  if (context_) {
    clReleaseContext(context_);
  }
}

OpenCLContext::OpenCLHandle::Ptr OpenCLContext::GetHandle() {
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
  strong_handle->opencl_ptr_ = instance_;
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

bool OpenCLContext::IsValid() const { return context_ && queue_; }
