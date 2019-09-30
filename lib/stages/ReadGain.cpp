#include "ReadGain.hpp"

#include "buffer.h"
#include "bufferContainer.hpp"
#include "chimeMetadata.h"
#include "configUpdater.hpp"
#include "errors.h"

#include <functional>
#include <utils/visUtil.hpp>

using kotekan::bufferContainer;
using kotekan::Config;
using kotekan::configUpdater;
using kotekan::Stage;

using kotekan::connectionInstance;
using kotekan::HTTP_RESPONSE;
using kotekan::restServer;

REGISTER_KOTEKAN_STAGE(ReadGain);

// clang-format off

// Request gain file re-parse with e.g.
// FRB
// curl localhost:12048/frb_gain -X POST -H 'Content-Type: appication/json' -d '{"frb_gain_dir":"the_new_path"}'
// PSR
// curl localhost:12048/updatable_config/pulsar_gain -X POST -H 'Content-Type: application/json' -d
// '{"pulsar_gain_dir":["path0","path1","path2","path3","path4","path5","path6","path7","path8","path9"]}'
//
// clang-format on

ReadGain::ReadGain(Config& config, const std::string& unique_name,
                   bufferContainer& buffer_container) :
    Stage(config, unique_name, buffer_container, std::bind(&ReadGain::main_thread, this)) {
    // Apply config.
    _num_elements = config.get<uint32_t>(unique_name, "num_elements");
    _num_beams = config.get<int32_t>(unique_name, "num_beams");
    scaling = config.get_default<float>(unique_name, "frb_scaling", 1.0);
    vector<float> dg = {0.0, 0.0}; // re,im
    default_gains = config.get_default<std::vector<float>>(unique_name, "frb_missing_gains", dg);

    metadata_buf = get_buffer("in_buf");
    register_consumer(metadata_buf, unique_name.c_str());
    metadata_buffer_id = 0;
    metadata_buffer_precondition_id = 0;
    freq_idx = -1;
    freq_MHz = -1;

    // Gain for FRB
    gain_frb_buf = get_buffer("gain_frb_buf");
    gain_frb_buf_id = 0;
    register_producer(gain_frb_buf, unique_name.c_str());
    update_gains_frb = true;

    // Gain for PSR
    gain_psr_buf = get_buffer("gain_psr_buf");
    gain_psr_buf_id = 0;
    register_producer(gain_psr_buf, unique_name.c_str());
    update_gains_psr = true;

    first_pass = true;

    using namespace std::placeholders;

    // listen for gain updates FRB
    _gain_dir_frb = config.get_default<std::string>(unique_name, "updatable_config/gain_frb", "");
    if (_gain_dir_frb.length() > 0)
        configUpdater::instance().subscribe(
            config.get<std::string>(unique_name, "updatable_config/gain_frb"),
            std::bind(&ReadGain::update_gains_frb_callback, this, _1));

    // listen for gain updates PSR
    configUpdater::instance().subscribe(
        config.get<std::string>(unique_name, "updatable_config/gain_psr"),
        std::bind(&ReadGain::update_gains_psr_callback, this, _1));
}

bool ReadGain::update_gains_frb_callback(nlohmann::json& json) {
    {
        std::lock_guard<std::mutex> lock(mux);
        update_gains_frb = true;
    }
    cond_var.notify_all();

    try {
        _gain_dir_frb = json.at("frb_gain_dir");
    } catch (std::exception& e) {
        WARN("[FRB] Fail to read gain_dir {:s}", e.what());
        return false;
    }
    INFO("[ReadGain] updated gain with {:s}============update_gains={:d}", _gain_dir_frb,
         update_gains_frb);
    return true;
}

bool ReadGain::update_gains_psr_callback(nlohmann::json& json) {
    {
        std::lock_guard<std::mutex> lock(mux);
        update_gains_psr = true;
    }
    cond_var.notify_all();

    try {
        _gain_dir_psr = json.at("pulsar_gain_dir").get<std::vector<string>>();
        INFO("[PSR] Updating gains from {:s} {:s} {:s} {:s} {:s} {:s} {:s} {:s} {:s} {:s}",
             _gain_dir_psr[0].c_str(), _gain_dir_psr[1].c_str(), _gain_dir_psr[2].c_str(),
             _gain_dir_psr[3].c_str(), _gain_dir_psr[4].c_str(), _gain_dir_psr[5].c_str(),
             _gain_dir_psr[6].c_str(), _gain_dir_psr[7].c_str(), _gain_dir_psr[8].c_str(),
             _gain_dir_psr[9].c_str());
    } catch (std::exception const& e) {
        WARN("[PSR] Fail to read gain_dir {:s}", e.what());
        return false;
    }
    return true;
}

void ReadGain::main_thread() {

    if (first_pass) {
        first_pass = false;
        uint8_t* frame =
            wait_for_full_frame(metadata_buf, unique_name.c_str(), metadata_buffer_precondition_id);
        if (frame == NULL)
            goto end_loop;
        stream_id_t stream_id = get_stream_id_t(metadata_buf, metadata_buffer_id);
        freq_idx = bin_number_chime(&stream_id);
        freq_MHz = freq_from_bin(freq_idx);
        metadata_buffer_precondition_id =
            (metadata_buffer_precondition_id + 1) % metadata_buf->num_frames;
    }
    mark_frame_empty(metadata_buf, unique_name.c_str(), metadata_buffer_id);
    metadata_buffer_id = (metadata_buffer_id + 1) % metadata_buf->num_frames;
    // unregister_consumer(metadata_buf, unique_name.c_str());

    while (!stop_thread) {
        DEBUG("update_frb_gains={:d} update_psr_gains={:d} (0=false 1=true)", update_gains_frb,
              update_gains_psr);

        // FRB================================================
        {
            std::unique_lock<std::mutex> lock(mux);
            while (!update_gains_frb) {
                cond_var.wait(lock);
            }
        }
        DEBUG("Going to update frb gain update_gains={:d}", update_gains_frb);

        float* out_frame_frb =
            (float*)wait_for_empty_frame(gain_frb_buf, unique_name.c_str(), gain_frb_buf_id);
        if (out_frame_frb == NULL) {
            goto end_loop;
        }

        double start_time = current_time();
        FILE* ptr_myfile;
        char filename[256];
        snprintf(filename, sizeof(filename), "%s/quick_gains_%04d_reordered.bin",
                 _gain_dir_frb.c_str(), freq_idx);
        INFO("[ReadGain] Loading gains from {:s}", filename);
        ptr_myfile = fopen(filename, "rb");
        if (ptr_myfile == NULL) {
            WARN("GPU Cannot open gain file {:s}", filename);
            for (int i = 0; i < 2048; i++) {
                out_frame_frb[i * 2] = default_gains[0] * scaling;
                out_frame_frb[i * 2 + 1] = default_gains[1] * scaling;
            }
        } else {
            if (_num_elements
                != fread(out_frame_frb, sizeof(float) * 2, _num_elements, ptr_myfile)) {
                WARN("Gain file ({:s}) wasn't long enough! Something went wrong, using default "
                     "gains",
                     filename);
                for (int i = 0; i < 2048; i++) {
                    out_frame_frb[i * 2] = default_gains[0] * scaling;
                    out_frame_frb[i * 2 + 1] = default_gains[1] * scaling;
                }
            }
            fclose(ptr_myfile);
        }
        mark_frame_full(gain_frb_buf, unique_name.c_str(), gain_frb_buf_id);
        DEBUG("Maked gain_frb_buf frame {:d} full", gain_frb_buf_id);
        DEBUG("Time required to load FRB gains: {:f}", current_time() - start_time);
        DEBUG("Gain_frb_buf: {:.2f} {:.2f} {:.2f} ", out_frame_frb[0], out_frame_frb[1],
              out_frame_frb[2]);
        gain_frb_buf_id = (gain_frb_buf_id + 1) % gain_frb_buf->num_frames;
        update_gains_frb = false;

        // PSR============================================
        {
            std::unique_lock<std::mutex> lock(mux);
            while (!update_gains_psr) {
                cond_var.wait(lock);
            }
        }
        DEBUG("Going to update psr gain update_gains={:d}", update_gains_psr);

        float* out_frame_psr =
            (float*)wait_for_empty_frame(gain_psr_buf, unique_name.c_str(), gain_psr_buf_id);
        if (out_frame_psr == NULL) {
            goto end_loop;
        }

        start_time = current_time();
        for (int b = 0; b < _num_beams; b++) {
            snprintf(filename, sizeof(filename), "%s/quick_gains_%04d_reordered.bin",
                     _gain_dir_psr[b].c_str(), freq_idx);
            INFO("Loading gains from {:s}", filename);
            ptr_myfile = fopen(filename, "rb");
            if (ptr_myfile == NULL) {
                WARN("GPU Cannot open gain file {:s}", filename);
                for (int i = 0; i < 2048; i++) {
                    out_frame_psr[(b * 2048 + i) * 2] = default_gains[0];
                    out_frame_psr[(b * 2048 + i) * 2 + 1] = default_gains[1];
                }
            } else {
                if (_num_elements
                    != fread(out_frame_psr, sizeof(float) * 2, _num_elements, ptr_myfile)) {
                    WARN("Gain file ({:s}) wasn't long enough! Something went wrong, using default "
                         "gains",
                         filename);
                    for (int i = 0; i < 2048; i++) {
                        out_frame_psr[(b * 2048 + i) * 2] = default_gains[0];
                        out_frame_psr[(b * 2048 + i) * 2 + 1] = default_gains[1];
                    }
                }
                fclose(ptr_myfile);
            }
        } // end beam
        mark_frame_full(gain_psr_buf, unique_name.c_str(), gain_psr_buf_id);
        DEBUG("Maked gain_psr_buf frame {:d} full", gain_psr_buf_id);
        DEBUG("Time required to load PSR gains: {:f}", current_time() - start_time);
        DEBUG("Gain_psr_buf: {:.2f} {:.2f} {:.2f} ", out_frame_psr[0], out_frame_psr[1],
              out_frame_psr[2]);
        gain_psr_buf_id = (gain_psr_buf_id + 1) % gain_psr_buf->num_frames;
        update_gains_psr = false;
    } // end stop thread
end_loop:;
}