/*****************************************
@file
@brief Generate fake visBuffer data.
- fakeVis : public KotekanProcess
*****************************************/

#ifndef FAKE_VIS
#define FAKE_VIS

#include <unistd.h>
#include "buffer.h"
#include "KotekanProcess.hpp"
#include "errors.h"
#include "util.h"

/**
 * @class fakeVis
 * @brief Producer ``KotekanProcess`` that generates fake visibility data into a ``visBuffer``.
 *
 * This process produces fake visibility data that can be used to feed downstream
 * kotekan processes for testing. It fills its buffer with frames in the ``visFrameView``
 * format. The visibility array is populated with integers increasing from zero on the
 * diagonal and FPGA sequence number, timestamp, frequency, and frame ID in the first
 * elements. The remaining elements are zero. Frames are generated for a set of frequencies
 * and a cadence specified in the config.
 *
 * @par Buffers
 * @buffer out_buf The kotekan buffer which will be fed, can be any size.
 *     @buffer_format visBuffer structured
 *     @buffer_metadata visMetadata
 *
 * @conf  out_buf           String. Name of buffer to output to.
 * @conf  num_elements      Int. The number of elements (i.e. inputs) in the
 *                          correlator data,
 * @conf  block_size        Int. The block size of the packed data.
 * @conf  num_eigenvectors  Int. The number of eigenvectors to be stored.
 * @conf  freq              List of int. The frequency IDs to generate frames for.
 * @conf  cadence           Float. The interval of time (in seconds) between frames.
 * @conf  fill_ij           Bool (default false). Fill the real part with the index
 *                          of feed i and the imaginary part with the index of j
 *                          instead of the structure described above.
 *
 * @todo  It might be useful eventually to produce realistic looking mock visibilities.
 *
 * @author  Tristan Pinsonneault-Marotte
 *
 */
class fakeVis : public KotekanProcess {

public:
    /// Constructor. Loads config options.
    fakeVis(Config &config,
            const string& unique_name,
            bufferContainer &buffer_container);

    /// Not yet implemented, should update runtime parameters.
    void apply_config(uint64_t fpga_seq);

    /// Primary loop to wait for buffers, stuff in data, mark full, lather, rinse and repeat.
    void main_thread();

private:
    /// Parameters saved from the config files
    size_t num_elements, num_eigenvectors, block_size, num_prod;

    /// Output buffer
    Buffer * out_buf;

    /// List of frequencies for this buffer
    std::vector<uint16_t> freq;

    /// Cadence to simulate (in seconds)
    float cadence;

    // Fill with feed indices
    bool fill_ij;
};

#endif