#ifndef CUDA_SAXPY_KERNEL_CUH
#define CUDA_SAXPY_KERNEL_CUH

#include "cudaCommand.hpp"
#include "cudaDeviceInterface.hpp"

class cudaSaxpyKernel : public cudaCommand {
public:
    cudaSaxpyKernel(kotekan::Config& config, const string& unique_name,
                   kotekan::bufferContainer& host_buffers, cudaDeviceInterface& device);
    ~cudaSaxpyKernel();
    cudaEvent_t execute(int gpu_frame_id, cudaEvent_t pre_event) override;

protected:
private:
    // Common configuration values (which do not change in a run)
    /// Number of elements on the telescope (e.g. 2048 - CHIME, 256 - Pathfinder).
    int32_t _num_elements;
    /// Number of frequencies per data stream sent to each node.
    int32_t _num_local_freq;
    /// Total samples in each dataset. Must be a value that is a power of 2.
    int32_t _samples_per_data_set;
    /// Number of independent integrations within a single dataset. (eg. 8 means
    /// samples_per_data_set/8= amount of integration per dataset.)
    int32_t _num_data_sets;
    /// Calculated value: num_adjusted_elements/block_size * (num_adjusted_elements/block_size +
    /// 1)/2
    int32_t _num_blocks;
    /// This is a kernel tuning parameter for a global work space dimension that sets data sizes for
    /// GPU work items.
    int32_t _block_size;
    /// Global buffer depth for all buffers in system. Sets the number of frames to be queued up in
    /// each buffer.
    int32_t _buffer_depth;
};

#endif // CUDA_SAXPY_KERNEL_CUH