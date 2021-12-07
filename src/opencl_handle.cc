#include "opencl_handle.h"

#include <fstream>
#include <iostream>
#include <vector>

OpenCLContext::Ptr OpenCLContext::instance_ = {};

OpenCLContext::OpenCLHandle::OpenCLHandle() : opencl_ptr_(), kernels_() {}

OpenCLContext::OpenCLHandle::~OpenCLHandle() {
  std::cout << "Destructing OpenCLHandle..." << std::endl;
  CleanupAllKernels();
  OpenCLContext::CleanupInstance();
}

bool OpenCLContext::OpenCLHandle::IsValid() const {
  auto context = opencl_ptr_.lock();
  if (!context) {
    return false;
  }

  return context->IsValid();
}

bool OpenCLContext::OpenCLHandle::CreateKernelFromSource(
    const std::string &kernel_fn, const std::string &kernel_name) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  } else if (HasKernel(kernel_name)) {
    std::cout
        << "ERROR: OpenCLContext already has kernel with given kernel_name \""
        << kernel_name << '"' << std::endl;
    return false;
  }

  cl_int err_num;
  KernelInfo kernel_info = {nullptr, nullptr, {}};

  OpenCLContext::Ptr context_ptr = opencl_ptr_.lock();
  if (!context_ptr) {
    std::cout << "ERROR: OpenCLHandle: OpenCLContext is not initialized"
              << std::endl;
    return false;
  }

  const char *source_c_str = kernel_fn.c_str();
  kernel_info.program_ = clCreateProgramWithSource(
      context_ptr->context_, 1, &source_c_str, nullptr, &err_num);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle: Failed to create program from source"
              << std::endl;
    return false;
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
    return false;
  }

  kernel_info.kernel_ =
      clCreateKernel(kernel_info.program_, kernel_name.c_str(), &err_num);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle: Failed to create kernel object from "
              << "source" << std::endl;
    clReleaseProgram(kernel_info.program_);
    return false;
  }

  kernels_.insert({kernel_name, kernel_info});

  return true;
}

bool OpenCLContext::OpenCLHandle::CreateKernelFromSource(
    const char *kernel_fn, const std::string &kernel_name) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  } else if (HasKernel(kernel_name)) {
    std::cout
        << "ERROR: OpenCLContext already has kernel with given kernel_name \""
        << kernel_name << '"' << std::endl;
    return false;
  }
  return CreateKernelFromSource(std::string(kernel_fn), kernel_name);
}

bool OpenCLContext::OpenCLHandle::CreateKernelFromFile(
    const std::string &filename, const std::string &kernel_name) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  } else if (HasKernel(kernel_name)) {
    std::cout
        << "ERROR: OpenCLContext already has kernel with given kernel_name \""
        << kernel_name << '"' << std::endl;
    return false;
  }
  std::string source;
  {
    char buf[1024];
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
      return false;
    }

    while (ifs.good()) {
      ifs.read(buf, 1024);
      source.append(buf, ifs.gcount());
    }

    if (source.empty()) {
      return false;
    }
  }
  return CreateKernelFromSource(source, kernel_name);
}

bool OpenCLContext::OpenCLHandle::CreateKernelFromFile(
    const char *filename, const std::string &kernel_name) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  } else if (HasKernel(kernel_name)) {
    std::cout
        << "ERROR: OpenCLContext already has kernel with given kernel_name \""
        << kernel_name << '"' << std::endl;
    return false;
  }
  return CreateKernelFromFile(std::string(filename), kernel_name);
}

bool OpenCLContext::OpenCLHandle::CreateKernelBuffer(
    const std::string &kernel_name, cl_mem_flags flags, std::size_t buf_size,
    void *host_ptr, const std::string &buffer_name) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLHandle::CreateKernelBuffer: OpenCLContext is "
                 "not initialized"
              << std::endl;
    return false;
  }

  auto kernel_info_iter = kernels_.find(kernel_name);
  if (kernel_info_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::CreateKernelBuffer: Given kernel \""
              << kernel_name << "\" doesn't exist" << std::endl;
    return false;
  }

  auto *buffer_map = &kernel_info_iter->second.mem_objects_;
  {
    auto buffer_info_iter = buffer_map->find(buffer_name);
    if (buffer_info_iter != buffer_map->end()) {
      std::cout
          << "ERROR: OpenCLHandle::CreateKernelBuffer: Buffer with name \""
          << buffer_name << "\" already exists" << std::endl;
      return false;
    }
  }

  auto opencl_context = opencl_ptr_.lock();
  if (!opencl_context) {
    std::cout << "ERROR: OpenCLHandle::CreateKernelBuffer: OpenCLContext is "
                 "not initialized"
              << std::endl;
    return false;
  }

  cl_int err_num;
  cl_mem mem_object;

  mem_object = clCreateBuffer(opencl_context->context_, flags, buf_size,
                              host_ptr, &err_num);
  if (err_num != CL_SUCCESS) {
    std::cout
        << "ERROR: OpenCLHandle::CreateKernelBuffer: Failed to create buffer"
        << std::endl;
    return false;
  }

  buffer_map->insert({buffer_name, {mem_object, buf_size}});

  return true;
}

bool OpenCLContext::OpenCLHandle::SetKernelBufferData(
    const std::string &kernel_name, const std::string &buffer_name,
    std::size_t data_size, void *data_ptr) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }
  auto kernel_info_iter = kernels_.find(kernel_name);
  if (kernel_info_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::SetKernelBufferData: Kernel with name \""
              << kernel_name << "\" doesn't exist" << std::endl;
    return false;
  }

  auto *buffer_map = &kernel_info_iter->second.mem_objects_;
  auto buffer_info_iter = buffer_map->find(buffer_name);
  if (buffer_info_iter == buffer_map->end()) {
    std::cout << "ERROR: OpenCLHandle::SetKernelBufferData: Buffer with name \""
              << buffer_name << "\" doesn't exist" << std::endl;
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

bool OpenCLContext::OpenCLHandle::AssignKernelBuffer(
    const std::string &kernel_name, unsigned int idx,
    const std::string &buffer_name) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }
  auto kernel_iter = kernels_.find(kernel_name);
  if (kernel_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::AssignKernelBuffer: Kernel with name \""
              << kernel_name << "\" doesn't exist" << std::endl;
    return false;
  }

  auto *buffer_map = &kernel_iter->second.mem_objects_;
  auto buffer_info_iter = buffer_map->find(buffer_name);
  if (buffer_info_iter == buffer_map->end()) {
    std::cout << "ERROR: OpenCLHandle::AssignKernelBuffer: buffer with name \""
              << buffer_name << "\" doesn't exist" << std::endl;
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

bool OpenCLContext::OpenCLHandle::AssignKernelArgument(
    const std::string &kernel_name, unsigned int idx, std::size_t data_size,
    const void *data_ptr) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }
  auto iter = kernels_.find(kernel_name);
  if (iter == kernels_.end()) {
    std::cout
        << "ERROR: OpenCLHandle::AssignKernelArgument: Kernel with name \""
        << kernel_name << "\" doesn't exist" << std::endl;
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
    const std::string &kernel_name) {
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

  auto kernel_iter = kernels_.find(kernel_name);
  if (kernel_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::GetGlobalWorkSize: Kernel with name \""
              << kernel_name << "\" doesn't exist" << std::endl;
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

std::size_t OpenCLContext::OpenCLHandle::GetWorkGroupSize(
    const std::string &kernel_name) {
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

  auto kernel_iter = kernels_.find(kernel_name);
  if (kernel_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::GetWorkGroupSize: Kernel with name \""
              << kernel_name << "\" doesn't exist" << std::endl;
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

std::size_t OpenCLContext::OpenCLHandle::GetDeviceMaxWorkGroupSize() {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return 0;
  }
  auto context_ptr = opencl_ptr_.lock();
  if (!context_ptr) {
    std::cout << "ERROR: OpenCLHandle::GetDeviceMaxWorkGroupSize: "
                 "OpenCLContext is not initialized"
              << std::endl;
    return 0;
  }
  std::size_t value;
  cl_int err_num =
      clGetDeviceInfo(context_ptr->device_id_, CL_DEVICE_MAX_WORK_GROUP_SIZE,
                      sizeof(std::size_t), &value, nullptr);
  if (err_num != CL_SUCCESS) {
    std::cout << "ERROR: OpenCLHandle::GetDeviceMaxWorkGroupSize: "
                 "Failed to get max work group size"
              << std::endl;
  }

  return value;
}

bool OpenCLContext::OpenCLHandle::ExecuteKernel(const std::string &kernel_name,
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

  auto kernel_iter = kernels_.find(kernel_name);
  if (kernel_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::ExecuteKernel: Kernel with name \""
              << kernel_name << "\" doesn't exist" << std::endl;
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

bool OpenCLContext::OpenCLHandle::ExecuteKernel2D(
    const std::string &kernel_name, std::size_t global_work_size_0,
    std::size_t global_work_size_1, std::size_t local_work_size_0,
    std::size_t local_work_size_1, bool is_blocking) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }
  auto context_ptr = opencl_ptr_.lock();
  if (!context_ptr) {
    std::cout << "ERROR: OpenCLHandle::ExecuteKernel2D: OpenCLContext is not "
                 "initialized"
              << std::endl;
    return false;
  }

  auto kernel_iter = kernels_.find(kernel_name);
  if (kernel_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::ExecuteKernel2D: Kernel with name \""
              << kernel_name << "\" doesn't exist" << std::endl;
    return false;
  }

  std::size_t global_work_size[2] = {global_work_size_0, global_work_size_1};
  std::size_t local_work_size[2] = {local_work_size_0, local_work_size_1};
  cl_event event;
  cl_int err_num = clEnqueueNDRangeKernel(
      context_ptr->queue_, kernel_iter->second.kernel_, 2, nullptr,
      global_work_size, local_work_size, 0, nullptr, &event);
  if (err_num != CL_SUCCESS) {
    std::cout
        << "ERROR: OpenCLHandle::ExecuteKernel2D: Failed to execute kernel"
        << " (" << err_num << ")" << std::endl;
    return false;
  }

  if (is_blocking) {
    err_num = clWaitForEvents(1, &event);
    if (err_num != CL_SUCCESS) {
      std::cout << "WARNING: OpenCLHandle::ExecuteKernel2D: Explicit wait on "
                   "kernel failed"
                << " (" << err_num << ")" << std::endl;
      clReleaseEvent(event);
      return false;
    }
  }

  clReleaseEvent(event);

  return true;
}

bool OpenCLContext::OpenCLHandle::GetBufferData(const std::string &kernel_name,
                                                const std::string &buffer_name,
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

  auto kernel_iter = kernels_.find(kernel_name);
  if (kernel_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::GetBufferData: Kernel with name \""
              << kernel_name << "\" doesn't exist" << std::endl;
    return false;
  }

  auto buffer_iter = kernel_iter->second.mem_objects_.find(buffer_name);
  if (buffer_iter == kernel_iter->second.mem_objects_.end()) {
    std::cout << "ERROR: OpenCLHandle::GetBufferData: Buffer with name \""
              << buffer_name << "\" doesn't exist" << std::endl;
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

bool OpenCLContext::OpenCLHandle::HasKernel(
    const std::string &kernel_name) const {
  return kernels_.find(kernel_name) != kernels_.end();
}

bool OpenCLContext::OpenCLHandle::HasBuffer(
    const std::string &kernel_name, const std::string &buffer_name) const {
  auto kernel_iter = kernels_.find(kernel_name);
  if (kernel_iter == kernels_.end()) {
    return false;
  }

  auto *buffer_map = &kernel_iter->second.mem_objects_;
  auto buffer_iter = buffer_map->find(buffer_name);
  if (buffer_iter == buffer_map->end()) {
    return false;
  }

  return true;
}

std::size_t OpenCLContext::OpenCLHandle::GetBufferSize(
    const std::string &kernel_name, const std::string &buffer_name) const {
  auto kernel_iter = kernels_.find(kernel_name);
  if (kernel_iter == kernels_.end()) {
    return 0;
  }

  auto *buffer_map = &kernel_iter->second.mem_objects_;
  auto buffer_iter = buffer_map->find(buffer_name);
  if (buffer_iter == buffer_map->end()) {
    return 0;
  }

  std::size_t size = 0;

  cl_int err = clGetMemObjectInfo(buffer_iter->second.mem, CL_MEM_SIZE,
                                  sizeof(std::size_t), &size, nullptr);
  if (err != CL_SUCCESS) {
    std::cout << "ERROR: Failed to query size of device buffer \""
              << buffer_name << "\" with kernel \"" << kernel_name << '"'
              << std::endl;
    return 0;
  }

  return size;
}

bool OpenCLContext::OpenCLHandle::CleanupBuffer(
    const std::string &kernel_name, const std::string &buffer_name) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }
  auto kernel_iter = kernels_.find(kernel_name);
  if (kernel_iter == kernels_.end()) {
    std::cout << "ERROR: OpenCLHandle::CleanupBuffer: Kernel with name \""
              << kernel_name << "\" doesn't exist" << std::endl;
    return false;
  }

  auto buffer_iter = kernel_iter->second.mem_objects_.find(buffer_name);
  if (buffer_iter == kernel_iter->second.mem_objects_.end()) {
    std::cout << "ERROR: OpenCLHandle::CleanupBuffer: Buffer with name \""
              << buffer_name << "\" doesn't exist" << std::endl;
    return false;
  }

  clReleaseMemObject(buffer_iter->second.mem);
  kernel_iter->second.mem_objects_.erase(buffer_iter);

  return true;
}

bool OpenCLContext::OpenCLHandle::CleanupKernel(
    const std::string &kernel_name) {
  if (!IsValid()) {
    std::cout << "ERROR: OpenCLContext is not initialized" << std::endl;
    return false;
  }
  auto iter = kernels_.find(kernel_name);
  if (iter == kernels_.end()) {
    std::cout << "WARNING CleanupKernel: Kernel with name \"" << kernel_name
              << "\" doesn't exist" << std::endl;
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
      CL_CONTEXT_PLATFORM,
      reinterpret_cast<cl_context_properties>(first_platform_id), 0};

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
  std::cout << "Destructing OpenCLContext..." << std::endl;
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

void OpenCLContext::CleanupInstance() {
  // OpenCLHandle is destructing, cleanup context by calling destructor
  instance_.reset();
}

bool OpenCLContext::IsValid() const { return context_ && queue_; }
