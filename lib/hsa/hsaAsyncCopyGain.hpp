#ifndef HSA_ASYNC_COPY_GAIN_H
#define HSA_ASYNC_COPY_GAIN_H

#include "hsaCommand.hpp"

class hsaAsyncCopyGain : public hsaCommand {
public:
    hsaAsyncCopyGain(kotekan::Config& config, const string& unique_name,
                     kotekan::bufferContainer& host_buffers, hsaDeviceInterface& device);

    virtual ~hsaAsyncCopyGain();

    int wait_on_precondition(int gpu_frame_id) override;

    hsa_signal_t execute(int gpu_frame_id, hsa_signal_t precede_signal) override;

    void finalize_frame(int frame_id) override;

private:
    struct Buffer* gain_buf;
    int32_t gain_len;
    int32_t gain_buf_id;
    int32_t gain_buf_finalize_id;
    int32_t gain_buf_precondition_id;

    /// Track how many frames still need to have gains updated on.
    int32_t frames_to_update;

    /// Tracks which GPU frames have an active copy from the execute stage
    /// Note since for a given frame_id there can only be one active set
    /// of commands as long as finalize_frame() marks this as false
    /// there is no risk of a race condition, since that index will not be
    /// reused until finalize_frame() is finished.
    std::vector<bool> frame_copy_active;

    /// Mutex to lock updates to the gain and copy state.
    std::mutex update_mutex;

    /// Make sure we load defail gains on the first pass.
    bool first_pass;
};

#endif
