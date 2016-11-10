#ifndef BEAMFORMING_POST_PROCESS
#define BEAMFORMING_POST_PROCESS

#include "KotekanProcess.hpp"
#include "Config.hpp"
#include "buffers.h"
#include <vector>

using std::vector;

class beamformingPostProcess : public KotekanProcess {
public:
    beamformingPostProcess(Config &config,
                  struct Buffer *in_buf,
                  struct Buffer &vdif_buf);
    virtual ~beamformingPostProcess();
    void main_thread();
    virtual void apply_config(uint64_t fpga_seq);

private:
    void fill_headers(unsigned char * out_buf,
                  struct VDIFHeader * vdif_header,
                  const uint32_t second,
                  const uint32_t fpga_seq_num,
                  const uint32_t num_links,
                  uint32_t *thread_id);

    struct Buffer *in_buf;
    struct Buffer &vdif_buf;

    // Config variables
    int32_t _num_fpga_links;
    int32_t _samples_per_data_set;
    int32_t _num_data_sets;
    vector<int32_t> _link_map;
    int32_t _num_local_freq;
    int32_t _num_gpus;

};


#endif