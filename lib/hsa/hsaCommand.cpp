#include "hsaCommand.hpp"
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include "hsaBase.h"

using std::string;

#define MAX_ARGS_LEN 64

hsaCommand::hsaCommand(const string &command_name_, const string &kernel_file_name_,
        hsaDeviceInterface& device_, Config& config_,
        bufferContainer &host_buffers_, const string &unique_name_) :
        command_name(command_name_),
        kernel_file_name(kernel_file_name_),
        config(config_),
        device(device_),
        host_buffers(host_buffers_),
        unique_name(unique_name_)
{
    apply_config(0);
    signals = (hsa_signal_t *)hsa_host_malloc(_gpu_buffer_depth * sizeof(hsa_signal_t));
    assert(signals != nullptr);
    memset(signals, 0, _gpu_buffer_depth * sizeof(hsa_signal_t));

    for (int i = 0; i < _gpu_buffer_depth; ++i) {
        hsa_signal_create(0, 0, NULL, &signals[i]);
    }

    // Not everyone needs this, maybe move out of constructor
    kernel_args = (void **)hsa_host_malloc(_gpu_buffer_depth * sizeof(void*));
    assert(kernel_args != nullptr);

    // Load the kernel if there is one.
    if (kernel_file_name != "") {
        // Should this be moved to the base class?
        allocate_kernel_arg_memory(MAX_ARGS_LEN);
        kernel_object = load_hsaco_file(kernel_file_name, command_name);
    }
}

hsaCommand::~hsaCommand() {
    hsa_host_free(signals);
    hsa_status_t hsa_status;
    for (int i = 0; i < _gpu_buffer_depth; ++i) {
        hsa_status = hsa_memory_free(kernel_args[i]);
        assert(hsa_status == HSA_STATUS_SUCCESS);
        hsa_status = hsa_signal_destroy(signals[i]);
        assert(hsa_status == HSA_STATUS_SUCCESS);
    }
    hsa_host_free(kernel_args);

    // TODO free kernel!!!

}

string &hsaCommand::get_name() {
    return command_name;
}

void hsaCommand::apply_config(const uint64_t& fpga_seq) {
    _gpu_buffer_depth = config.get_int("/gpu", "buffer_depth");
    //INFO("GPU Buffer depth: %d", _gpu_buffer_depth);
}

void hsaCommand::allocate_kernel_arg_memory(int max_size) {
    hsa_status_t hsa_status;
    for (int i = 0; i < _gpu_buffer_depth; ++i) {

        hsa_status = hsa_memory_allocate(device.get_kernarg_region(),
                                         max_size, &kernel_args[i]);
        assert(hsa_status == HSA_STATUS_SUCCESS);
    }
}

void hsaCommand::finalize_frame(int frame_id) {
    //if (signals[frame_id].handle != 0) {
        //hsa_status_t hsa_status;
        //hsa_status = hsa_signal_destroy(signals[frame_id]);
        //assert(hsa_status == HSA_STATUS_SUCCESS);
    //}
}

void hsaCommand::wait_on_precondition(int gpu_frame_id) {
    (void)gpu_frame_id;
}

uint64_t hsaCommand::load_hsaco_file(string& file_name, string& kernel_name) {

    hsa_status_t hsa_status;

    // Open file.
    std::ifstream file(file_name, std::ios::in | std::ios::binary);
    assert(file.is_open() && file.good());

    // Find out file size.
    file.seekg(0, file.end);
    size_t code_object_size = file.tellg();
    file.seekg(0, file.beg);

    // Allocate memory for raw code object.
    char *raw_code_object = (char*)malloc(code_object_size);
    assert(raw_code_object);

    // Read file contents.
    file.read(raw_code_object, code_object_size);

    // Close file.
    file.close();

    // Deserialize code object.
    hsa_code_object_t code_object = {0};
    hsa_status = hsa_code_object_deserialize((void*)raw_code_object, code_object_size, NULL, &code_object);
    assert(HSA_STATUS_SUCCESS == hsa_status);
    assert(0 != code_object.handle);

    // Create executable.
    hsa_executable_t hsaExecutable;
    hsa_status = hsa_executable_create(HSA_PROFILE_FULL, HSA_EXECUTABLE_STATE_UNFROZEN, NULL, &hsaExecutable);
    assert(HSA_STATUS_SUCCESS == hsa_status);

    // Load code object.
    hsa_status = hsa_executable_load_code_object(hsaExecutable, device.get_gpu_agent(), code_object, NULL);
    assert(HSA_STATUS_SUCCESS == hsa_status);

    // Freeze executable.
    hsa_status = hsa_executable_freeze(hsaExecutable, NULL);
    assert(HSA_STATUS_SUCCESS == hsa_status);

    // Get symbol handle.
    hsa_executable_symbol_t kernelSymbol;
    hsa_status = hsa_executable_get_symbol(hsaExecutable, NULL, kernel_name.c_str(), device.get_gpu_agent(), 0, &kernelSymbol);
    assert(HSA_STATUS_SUCCESS == hsa_status);

    // Get code handle.
    uint64_t codeHandle;
    hsa_status = hsa_executable_symbol_get_info(kernelSymbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &codeHandle);
    assert(HSA_STATUS_SUCCESS == hsa_status);

    uint32_t group_segment_size;
    hsa_status = hsa_executable_symbol_get_info(kernelSymbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE, &group_segment_size);

    uint32_t priv_segment_size;
    hsa_status = hsa_executable_symbol_get_info(kernelSymbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE, &priv_segment_size);

    // Free raw code object memory.
    free((void*)raw_code_object);

    return codeHandle;
}

void hsaCommand::packet_store_release(uint32_t* packet, uint16_t header, uint16_t rest) {
    __atomic_store_n(packet, header | (rest << 16),   __ATOMIC_RELEASE);
}

uint16_t hsaCommand::header(hsa_packet_type_t type) {
    uint16_t header = (type << HSA_PACKET_HEADER_TYPE) |
                      (1 << HSA_PACKET_HEADER_BARRIER);
    header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_SCACQUIRE_FENCE_SCOPE;
    header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_SCRELEASE_FENCE_SCOPE;
    return header;
}

hsa_signal_t hsaCommand::enqueue_kernel(const kernelParams &params, const int gpu_frame_id) {

    // Get the queue index
    uint64_t packet_id = hsa_queue_add_write_index_screlease(device.get_queue(), 1);

    // Make sure the queue isn't full
    // Should never hit this condition, but lets be safe.
    // See the HSA docs for details.
    while (packet_id - hsa_queue_load_read_index_scacquire(device.get_queue())
            >= device.get_queue()->size);

    // Get the packet address
    hsa_kernel_dispatch_packet_t* packet =
            (hsa_kernel_dispatch_packet_t*) device.get_queue()->base_address
            + (packet_id % device.get_queue()->size);

    // Zero the packet (see HSA docs)
    memset(((uint8_t*) packet) + 4, 0, sizeof(hsa_kernel_dispatch_packet_t) - 4);

    // Set kernel dims for packet
    packet->workgroup_size_x = params.workgroup_size_x;
    packet->workgroup_size_y = params.workgroup_size_y;
    packet->workgroup_size_z = params.workgroup_size_z;
    packet->grid_size_x = params.grid_size_x;
    packet->grid_size_y = params.grid_size_y;
    packet->grid_size_z = params.grid_size_z;

    // Extra details
    packet->private_segment_size = params.private_segment_size;
    packet->group_segment_size = params.group_segment_size;

    // Set the kernel object (loaded HSACO code)
    packet->kernel_object = this->kernel_object;

    // Add the kernel args
    // Must have been copied before this function is called!
    packet->kernarg_address = (void*) kernel_args[gpu_frame_id];

    // Create the completion signal for this kernel run.
    hsa_signal_store_relaxed(signals[gpu_frame_id], 1);
    packet->completion_signal = signals[gpu_frame_id];

    // Create the AQL packet header as an atomic operation,
    // recommended by the HSA docs.
    packet_store_release((uint32_t*) packet,
            header(HSA_PACKET_TYPE_KERNEL_DISPATCH),
            params.num_dims << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS);

    // Notify the device there is a new packet in the queue
    hsa_signal_store_screlease(device.get_queue()->doorbell_signal, packet_id);

    return packet->completion_signal;
}