#ifndef APPLY_GAINS_HPP
#define APPLY_GAINS_HPP

#include "Stage.hpp"
#include "buffer.h"
#include "datasetManager.hpp"
#include "errors.h"
#include "fpga_header_functions.h"
#include "updateQueue.hpp"
#include "util.h"
#include "visFile.hpp"
#include "visUtil.hpp"

#include <shared_mutex>
#include <unistd.h>


/**
 * @class applyGains
 * @brief Receives gains and apply them to the output buffer.
 *
 * This stage registers as a subscriber to an updatable config block. The
 * full name of the block should be defined in the value @c updatable_block
 *
 * Gain updates *must* match the frequencies expected to be present in the
 * input stream. That is there must be exactly as many frequencies in the gain
 * update as there are in the `freqState` attached to the input stream.
 * The number of elements must also match those on the incoming
 * stream.
 *
 * The number of frequencies and inputs is locked in
 *
 *
 * @par Buffers
 * @buffer in_buf The input stream.
 *         @buffer_format visBuffer.
 *         @buffer_metadata visMetadata
 * @buffer out_buf The output stream.
 *         @buffer_format visBuffer.
 *         @buffer_metadata visMetadata
 *
 * @conf   num_elements     Int.    The number of elements (i.e. inputs) in the
 *                                  correlator data.
 * @conf   updatable_block  String. The full name of the updatable_block that
 *                                  will provide new flagging values (e.g. "/dynamic_block/gains").
 * @conf   gains_dir        String. The path to the directory holding the gains file.
 * @conf   num_kept_updates Int.    The number of gain updates stored in a FIFO.
 * @conf   num_threads      Int.    Number of threads to run. Default is 1.
 *
 * @par Metrics
 * @metric kotekan_applygains_late_update_count The number of updates received
 *     too late (The start time of the update is older than the currently
 *     processed frame).
 * @metric kotekan_applygains_late_frame_count The number of frames received
 *     late (The frames timestamp is older then all start times of stored
 *     updates).
 * @metric kotekan_applygains_update_age_seconds The time difference in
 *     seconds between the current frame being processed and the time stamp of
 *     the gains update being applied.
 *
 * @author Mateus Fandino
 */
class applyGains : public kotekan::Stage {

public:
    /// Default constructor
    applyGains(kotekan::Config& config, const string& unique_name,
               kotekan::bufferContainer& buffer_container);

    /// Main loop for the stage
    void main_thread() override;

    /// Callback function to receive updates on timestamps from configUpdater
    bool receive_update(nlohmann::json& json);

private:
    // An internal type for holding the actual gains
    struct GainData {
        std::vector<std::vector<cfloat>> gain;
        std::vector<std::vector<float>> weight;
    };

    // An internal type for holding all information about the gain update
    struct GainUpdate {
        GainData data;
        double transition_interval;
        state_id_t state_id;
    };

    // Parameters saved from the config files

    /// Path to gains directory
    std::string gains_dir;

    /// Number of gains updates to maintain
    uint64_t num_kept_updates;

    /// The gains and when to start applying them in a FIFO (len set by config)
    updateQueue<GainUpdate> gains_fifo;

    /// Input buffer to read from
    Buffer* in_buf;
    /// Output buffer with gains applied
    Buffer* out_buf;

    /// Mutex to protect access to gains and freq map
    std::shared_mutex gain_mtx;
    std::shared_mutex freqmap_mtx;

    /// Timestamp of the current frame
    std::atomic<timespec> ts_frame{{0, 0}};

    /// Entrancepoint for n threads. Each thread takes frames with a
    /// different frame_id from the buffer and applies gains.
    void apply_thread();

    /// Vector to hold the thread handles
    std::vector<std::thread> thread_handles;

    /// Number of parallel threads accessing the same buffers (default 1)
    uint32_t num_threads;

    /// Input frame ID, shared by apply threads.
    frameID frame_id_in;

    /// Output frame ID, shared by apply threads.
    frameID frame_id_out;

    /// Mutex protecting shared frame IDs.
    std::mutex m_frame_ids;

    // Prometheus metrics
    kotekan::prometheus::Gauge& update_age_metric;
    kotekan::prometheus::Counter& late_update_counter;
    kotekan::prometheus::Counter& late_frames_counter;

    /// Read the gain file from disk
    std::optional<GainData> read_gain_file(std::string update_id) const;

    /// Test that the frame is valid. On failure it will call FATAL_ERROR and
    /// return false
    bool validate_frame(const visFrameView& frame);

    /// Test that the gain is valid. On failure it will call FATAL_ERROR and
    /// return false. Gains failing this *should* have already been rejected,
    /// except the initial gains which can't be checked.
    bool validate_gain(const GainData& frame) const;


    // Check if file to read exists
    bool fexists(const std::string& filename) const;

    // Save the number of frequencies and elements for checking gain updates
    // against
    std::optional<size_t> num_freq;
    std::optional<size_t> num_elements;
    std::optional<size_t> num_prod;

    // Mapping from frequency ID to index (in the gain file)
    std::map<uint32_t, uint32_t> freq_map;

    // Map from the state being applied and input dataset to the output dataset.
    // This is used to keep track of the labels we should be applying for
    // timesamples coming out of order.
    std::map<std::pair<state_id_t, dset_id_t>, dset_id_t> output_dataset_ids;
};


#endif
