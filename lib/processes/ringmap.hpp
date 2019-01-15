
#ifndef RINGMAP_HPP
#define RINGMAP_HPP

#include "gsl-lite.hpp"
#include "KotekanProcess.hpp"
#include "visUtil.hpp"
#include "restServer.hpp"

class mapMaker : public KotekanProcess {

public:

    // Default constructor
    mapMaker(Config &config,
             const string& unique_name,
             bufferContainer &buffer_container);

    void apply_config(uint64_t fpga_seq) override;

    // Main loop for the process
    void main_thread() override;

    /// REST endpoint to request a map
    nlohmann::json rest_callback(connectionInstance& conn, nlohmann::json &json);

private:

    bool setup(uint frame_id);

    void change_dataset_state();

    void gen_matrices();

    void gen_baselines();

    inline float wl(uint32_t fid);

    std::vector<float> baselines;
    std::vector<float> sinza;
    std::map<uint32_t,std::vector<cfloat>> vis2map;
    std::map<uint,std::vector<cfloat>> map;
    std::map<uint,std::vector<cfloat>> wgt_map;
    uint32_t num_time;
    uint32_t num_pix;
    uint8_t num_pol;
    uint32_t num_stack;

    dset_id_t ds_id;
    std::vector<uint32_t> excl_input;
    std::vector<std::pair<uint32_t, freq_ctype>> freq;
    std::vector<prod_ctype> prod;

    // Map buffer file
    void * map_file;

    // Buffer to read from
    Buffer* in_buf;
};


class redundantStack : public KotekanProcess {

public:

    redundantStack(Config &config,
                   const string& unique_name,
                   bufferContainer &buffer_container);

};

#endif