#include "gpuHSACommandFactory.hpp"
#include <stdio.h>
#include <stdlib.h>
#include "errors.h"
#include <errno.h>
#include <iostream>
#include <vector>

#include "json.hpp"
#include "hsaCorrelatorKernel.hpp"
#include "hsaPreseedKernel.hpp"
#include "hsaInputData.hpp"
#include "hsaPresumZero.hpp"
#include "hsaOutputData.hpp"
#include "hsaOutputDataZero.hpp"
#include "hsaBarrier.hpp"
#include "hsaBeamformKernel.hpp"
#include "hsaBeamformOutput.hpp"

using namespace std;

gpuHSACommandFactory::gpuHSACommandFactory(Config& config_,
                                            gpuHSADeviceInterface& device_,
                                            bufferContainer &host_buffers_) :
    config(config_),
    device(device_),
    host_buffers(host_buffers_){

    vector<json> commands = config.get_json_array("/gpu/commands");

    for (uint32_t i = 0; i < commands.size(); i++){

        if (commands[i]["name"] == "hsa_correlator_kernel") {
            list_commands.push_back(new hsaCorrelatorKernel("CHIME_X",
                    commands[i]["kernel"].get<string>(),
                    device, config, host_buffers));
        } else if (commands[i]["name"] == "hsa_barrier") {
            list_commands.push_back(new hsaBarrier("hsa_barrier",
                    commands[i]["kernel"].get<string>(),
                    device, config, host_buffers));
        } else if (commands[i]["name"] == "hsa_preseed_kernel") {
            list_commands.push_back(new hsaPreseedKernel("ZZ4mainEN3_EC__019__cxxamp_trampolineEPjiiiiPiiiii",
                    commands[i]["kernel"].get<string>(),
                    device, config, host_buffers));
        } else if (commands[i]["name"] == "hsa_input_data") {
            list_commands.push_back(new hsaInputData("hsa_input_data",
                    commands[i]["kernel"].get<string>(),
                    device, config, host_buffers));
        } else if (commands[i]["name"] == "hsa_presum_zero") {
            list_commands.push_back(new hsaPresumZero("hsa_presum_zero",
                    commands[i]["kernel"].get<string>(),
                    device, config, host_buffers));
        } else if (commands[i]["name"] == "hsa_output_data") {
            list_commands.push_back(new hsaOutputData("hsa_output_data",
                    commands[i]["kernel"].get<string>(),
                    device, config, host_buffers));
        } else if (commands[i]["name"] == "hsa_output_data_zero") {
            list_commands.push_back(new hsaOutputDataZero("hsa_output_data_zero",
                    commands[i]["kernel"].get<string>(),
                    device, config, host_buffers));
        } else if (commands[i]["name"] == "hsa_beamform_kernel") {
            list_commands.push_back(new hsaBeamformKernel("zero_padded_FFT512",
                    commands[i]["kernel"].get<string>(),
                    device, config, host_buffers));
        } else if (commands[i]["name"] == "hsa_beamfrom_output") {
            list_commands.push_back(new hsaBeamformOutputData("hsa_beamfrom_output",
                    commands[i]["kernel"].get<string>(),
                    device, config, host_buffers));
        } else {
            ERROR("Command %s not found!", commands[i]["name"].get<string>().c_str());
        }
    }
}

gpuHSACommandFactory::~gpuHSACommandFactory() {
    // Does this work?
    for (auto command : list_commands) {
        delete command;
    }
}

vector<gpuHSAcommand*>& gpuHSACommandFactory::get_commands() {
    return list_commands;
}