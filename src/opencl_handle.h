#ifndef IGPUP_DITHERING_PROJECT_OPENCL_H_
#define IGPUP_DITHERING_PROJECT_OPENCL_H_

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

class OpenCLContext {
 public:
  typedef std::shared_ptr<OpenCLContext> Ptr;
  typedef std::weak_ptr<OpenCLContext> WeakPtr;

  /*!
   * \brief A simplified handle to OpenCL.
   *
   * This class can only be obtained by a call to OpenCLContext::GetHandle().
   *
   * OpenCL is automatically cleaned up when all shared ptrs of OpenCLHandle are
   * destructed.
   */
  class OpenCLHandle {
   public:
    typedef std::shared_ptr<OpenCLHandle> Ptr;
    typedef std::weak_ptr<OpenCLHandle> WeakPtr;

    ~OpenCLHandle();

    // no copy
    OpenCLHandle(const OpenCLHandle &other) = delete;
    OpenCLHandle &operator=(const OpenCLHandle &other) = delete;

    // no move
    OpenCLHandle(OpenCLHandle &&other) = delete;
    OpenCLHandle &operator=(OpenCLHandle &&other) = delete;

    bool IsValid() const;

    /*!
     * \brief Compiles a kernel from source that can be referenced with the
     * given kernel name.
     *
     * The created kernel can be free'd with a call to CleanupKernel().
     *
     * \return True on success.
     */
    bool CreateKernelFromSource(const std::string &kernel_fn,
                                const std::string &kernel_name);

    /*!
     * \brief Compiles a kernel from source that can be referenced with the
     * given kernel name.
     *
     * The created kernel can be free'd with a call to CleanupKernel().
     *
     * \return True on success.
     */
    bool CreateKernelFromSource(const char *kernel_fn,
                                const std::string &kernel_name);

    /*!
     * \brief Compiles a kernel from a file that can be referenced with the
     * given kernel name.
     *
     * The created kernel can be free'd with a call to CleanupKernel().
     *
     * \return True on success.
     */
    bool CreateKernelFromFile(const std::string &filename,
                              const std::string &kernel_name);

    /*!
     * \brief Compiles a kernel from a file that can be referenced with the
     * given kernel name.
     *
     * The created kernel can be free'd with a call to CleanupKernel().
     *
     * \return True on success.
     */
    bool CreateKernelFromFile(const char *filename,
                              const std::string &kernel_name);

    /*!
     * \brief Creates a cl_mem buffer that can be referenced with the given
     * buffer_name.
     *
     * Note that the created buffer is stored with the specified kernel's data.
     * This means that the created buffer can only be accessed with the kernel
     * that was used to create it.
     *
     * If host_ptr set to nullptr, then the created buffer will be
     * uninitialized.
     *
     * \return True on success.
     */
    bool CreateKernelBuffer(const std::string &kernel_name, cl_mem_flags flags,
                            std::size_t buf_size, void *host_ptr,
                            const std::string &buffer_name);

    /*!
     * \brief Assign host data to existing device buffer
     *
     * The kernel referenced by kernel_name must exist, and the buffer
     * referenced by buffer_name must also exist.
     *
     * \return True on success.
     */
    bool SetKernelBufferData(const std::string &kernel_name,
                             const std::string &buffer_name,
                             std::size_t data_size, void *data_ptr);

    /*!
     * \brief Assign a previously created buffer to a kernel function's
     * parameter.
     *
     * \return true on success.
     */
    bool AssignKernelBuffer(const std::string &kernel_name, unsigned int idx,
                            const std::string &buffer_name);

    /*!
     * \brief Assign data to a kernel function's parameter.
     *
     * idx refers to the parameter index for the kernel function.
     *
     * \return true on success.
     */
    bool AssignKernelArgument(const std::string &kernel_name, unsigned int idx,
                              std::size_t data_size, const void *data_ptr);

    /*!
     * \brief Gets the sizes associated with CL_KERNEL_GLOBAL_WORK_SIZE.
     *
     * \return {0, 0, 0} on failure.
     */
    std::array<std::size_t, 3> GetGlobalWorkSize(
        const std::string &kernel_name);

    /*!
     * \brief Gets the size associated with CL_KERNEL_WORK_GROUP_SIZE.
     *
     * \return 0 on failure.
     */
    std::size_t GetWorkGroupSize(const std::string &kernel_name);

    std::size_t GetDeviceMaxWorkGroupSize();

    /*!
     * \brief Executes the kernel with the given kernel_name.
     *
     * \return true on success.
     */
    bool ExecuteKernel(const std::string &kernel_name,
                       std::size_t global_work_size,
                       std::size_t local_work_size, bool is_blocking);

    /*!
     * \brief Executes the kernel with the given kernel_name.
     *
     * \return true on success.
     */
    bool ExecuteKernel2D(const std::string &kernel_name,
                         std::size_t global_work_size_0,
                         std::size_t global_work_size_1,
                         std::size_t local_work_size_0,
                         std::size_t local_work_size_1, bool is_blocking);

    /*!
     * \brief Copies device memory to data_out.
     *
     * \return True on success.
     */
    bool GetBufferData(const std::string &kernel_name,
                       const std::string &buffer_name, std::size_t out_size,
                       void *data_out);

    /// Returns true if the kernel exists
    bool HasKernel(const std::string &kernel_name) const;

    /// Returns true if the buffer exists with the kernel
    bool HasBuffer(const std::string &kernel_name,
                   const std::string &buffer_name) const;

    /// Returns the buffer size in bytes, or 0 if error
    std::size_t GetBufferSize(const std::string &kernel_name,
                              const std::string &buffer_name) const;

    /*!
     * \brief Cleans up a mem buffer.
     *
     * If using CleanupKernel(), there is no need to call this function with the
     * same kernel_id as it will cleanup the associated mem buffers.
     *
     * \return true if clean has occurred.
     */
    bool CleanupBuffer(const std::string &kernel_name,
                       const std::string &buffer_name);

    /*!
     * \brief Cleans up a kernel object and its associated data (including mem
     * buffers).
     *
     * \return true if cleanup has occurred.
     */
    bool CleanupKernel(const std::string &kernel_name);

    /*!
     * \brief Cleans up all Kernel data (including mem buffers).
     */
    void CleanupAllKernels();

   private:
    friend class OpenCLContext;

    struct BufferInfo {
      cl_mem mem;
      std::size_t size;
    };

    struct KernelInfo {
      cl_kernel kernel_;
      cl_program program_;
      std::unordered_map<std::string, BufferInfo> mem_objects_;
    };

    OpenCLHandle();

    OpenCLContext::WeakPtr opencl_ptr_;

    std::unordered_map<std::string, KernelInfo> kernels_;
  };

  ~OpenCLContext();

  // no copy
  OpenCLContext(const OpenCLContext &other) = delete;
  OpenCLContext &operator=(const OpenCLContext &other) = delete;

  // no move
  OpenCLContext(OpenCLContext &&other) = delete;
  OpenCLContext &operator=(OpenCLContext &&other) = delete;

  /// Returns a OpenCLHandle wrapped in a std::shared_ptr
  static OpenCLHandle::Ptr GetHandle();

 private:
  OpenCLContext();

  static Ptr instance_;
  OpenCLHandle::WeakPtr weak_handle_;

  cl_context context_;
  cl_command_queue queue_;
  cl_device_id device_id_;

  static void CleanupInstance();

  bool IsValid() const;
};

typedef OpenCLContext::OpenCLHandle OpenCLHandle;

#endif
