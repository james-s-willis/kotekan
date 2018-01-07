#include "fftwEngine.hpp"

fftwEngine::fftwEngine(Config& config, const string& unique_name,
                         bufferContainer &buffer_container) :
    KotekanProcess(config, unique_name, buffer_container,
                   std::bind(&fftwEngine::main_thread, this)) {

    buf_in = get_buffer("in_buf");
    register_consumer(buf_in, unique_name.c_str());
    buf_out = get_buffer("out_buf");
    register_producer(buf_out, unique_name.c_str());

    spectrum_length = config.get_int_default(unique_name,"spectrum_length",1024);
    double_spectrum_length = spectrum_length*2;
    samples = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*double_spectrum_length);
    spectrum = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*double_spectrum_length);
    fft_plan = (fftwf_plan_s*)fftwf_plan_dft_1d(double_spectrum_length, samples, spectrum, 1, FFTW_ESTIMATE);
}

fftwEngine::~fftwEngine() {
    fftwf_free(samples);
    fftwf_free(spectrum);
    fftwf_free(fft_plan);
}

void fftwEngine::apply_config(uint64_t fpga_seq) {
}

void fftwEngine::main_thread() {
    short *in_local;
    fftwf_complex *out_local;

    frame_in = 0;
    frame_out = 0;

    int samples_per_input_frame = buf_in->frame_size/BYTES_PER_SAMPLE;

    while(!stop_thread) {
        in_local = (short*)wait_for_full_frame(buf_in, unique_name.c_str(), frame_in);
        out_local = (fftwf_complex*)wait_for_empty_frame(buf_out, unique_name.c_str(), frame_out);

        for (int j=0; j<samples_per_input_frame; j+=double_spectrum_length){
//            INFO("Running FFT, %i",((short*)(in_local))[j]-(1<<11));
            for (int i=0; i<double_spectrum_length; i++){
                samples[i][0]=in_local[i+j]-(1<<11);
                samples[i][1]=0;
            }
            fftwf_execute(fft_plan);
            memcpy(out_local,spectrum,sizeof(fftwf_complex)*spectrum_length);
            out_local += spectrum_length;
        }

        mark_frame_empty(buf_in, unique_name.c_str(), frame_in);
        mark_frame_full(buf_out, unique_name.c_str(), frame_out);
        frame_in =  (frame_in  + 1) % buf_in->num_frames;
        frame_out = (frame_out + 1) % buf_out->num_frames;
   }
}
