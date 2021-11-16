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

typedef unsigned int KernelID;
typedef unsigned int BufferID;

class OpenCLContext {
 public:
  typedef std::shared_ptr<OpenCLContext> Ptr;
  typedef std::weak_ptr<OpenCLContext> WeakPtr;

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

    bool IsValid() const;

    /*!
     * \brief Returns the KernelID, to be used with other fns in OpenCLHandle
     *
     * The created kernel can be free'd with a call to CleanupKernel(KernelID)
     *
     * \return KernelID with value 0 on failure, non-zero otherwise
     */
    KernelID CreateKernelFromSource(const std::string &kernel_fn,
                                    const char *kernel_name);
    /*!
     * \brief Returns the KernelID, to be used with other fns in OpenCLHandle
     *
     * The created kernel can be free'd with a call to CleanupKernel(KernelID)
     *
     * \return KernelID with value 0 on failure, non-zero otherwise
     */
    KernelID CreateKernelFromSource(const char *kernel_fn,
                                    const char *kernel_name);
    /*!
     * \brief Returns the KernelID, to be used with other fns in OpenCLHandle
     *
     * The created kernel can be free'd with a call to CleanupKernel(KernelID)
     *
     * \return KernelID with value 0 on failure, non-zero otherwise
     */
    KernelID CreateKernelFromFile(const std::string &filename,
                                  const char *kernel_name);
    /*!
     * \brief Returns the KernelID, to be used with other fns in OpenCLHandle
     *
     * The created kernel can be free'd with a call to CleanupKernel(KernelID)
     *
     * \return KernelID with value 0 on failure, non-zero otherwise
     */
    KernelID CreateKernelFromFile(const char *filename,
                                  const char *kernel_name);

    /*!
     * \brief Creates a cl_mem buffer and returns its id
     *
     * Note that the created buffer is stored with the specified kernel's data.
     * This means that the created buffer can only be accessed with the
     * KernelID that was used to create it.
     *
     * If buf_size is set to 0 and host_ptr set to nullptr, then the created
     * buffer will be uninitialized.
     *
     * \return non-zero BufferID on success
     */
    BufferID CreateKernelBuffer(KernelID kernel_id, cl_mem_flags flags,
                                std::size_t buf_size, void *host_ptr);

    /*!
     * \brief Assign host data to existing device buffer
     *
     * \return true on success
     */
    bool SetKernelBufferData(KernelID kernel_id, BufferID buffer_id,
                             std::size_t data_size, void *data_ptr);

    /*!
     * \brief Assign a previously created buffer to a kernel function's
     * parameter
     *
     * \return true on success
     */
    bool AssignKernelBuffer(KernelID kernel_id, unsigned int idx,
                            BufferID buffer_id);

    /*!
     * \brief Assign data to a kernel function's parameter
     *
     * id refers to the kernel's id, and idx refers to the parameter index for
     * the kernel function.
     *
     * \return true on success
     */
    bool AssignKernelArgument(KernelID kernel_id, unsigned int idx,
                              std::size_t data_size, const void *data_ptr);

    /*!
     * \brief Gets the sizes associated with CL_KERNEL_GLOBAL_WORK_SIZE
     *
     * \return {0, 0, 0} on failure
     */
    std::array<std::size_t, 3> GetGlobalWorkSize(KernelID kernel_id);

    /*!
     * \brief Gets the size associated with CL_KERNEL_WORK_GROUP_SIZE
     *
     * \return 0 on failure
     */
    std::size_t GetWorkGroupSize(KernelID kernel_id);

    /*!
     * \brief Executes the kernel with the given kernel_id
     *
     * \return true on success
     */
    bool ExecuteKernel(KernelID kernel_id, std::size_t global_work_size,
                       std::size_t local_work_size, bool is_blocking);

    /*!
     * \brief Copies device memory to data_out
     *
     * \return true on success
     */
    bool GetBufferData(KernelID kernel_id, BufferID buffer_id,
                       std::size_t out_size, void *data_out);

    /*!
     * \brief Cleans up a mem buffer
     *
     * If using CleanupKernel(KernelID id), there is no need to call this
     * function with the same kernel_id as it will cleanup the associated mem
     * buffers.
     *
     * \return true if clean has occurred
     */
    bool CleanupBuffer(KernelID kernel_id, BufferID buffer_id);

    /*!
     * \brief Cleans up a kernel object and its associated data (like mem
     * buffers)
     *
     * \return true if cleanup has occurred
     */
    bool CleanupKernel(KernelID id);

    /*!
     * \brief Cleans up all Kernel data (including mem buffers)
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
      std::unordered_map<BufferID, BufferInfo> mem_objects_;
      BufferID buffer_id_counter_;
    };

    OpenCLHandle();

    OpenCLContext::WeakPtr opencl_ptr_;

    std::unordered_map<KernelID, KernelInfo> kernels_;
    KernelID kernel_counter_;
  };

  ~OpenCLContext();

  // no copy
  OpenCLContext(const OpenCLContext &other) = delete;
  OpenCLContext &operator=(const OpenCLContext &other) = delete;

  // no move
  OpenCLContext(OpenCLContext &&other) = delete;
  OpenCLContext &operator=(OpenCLContext &&other) = delete;

  OpenCLHandle::Ptr GetHandle();

 private:
  OpenCLContext();

  static Ptr instance_;
  OpenCLHandle::WeakPtr weak_handle_;

  cl_context context_;
  cl_command_queue queue_;
  cl_device_id device_id_;

  static void CheckRefCount();

  bool IsValid() const;
};

typedef OpenCLContext::OpenCLHandle OpenCLHandle;

#endif
