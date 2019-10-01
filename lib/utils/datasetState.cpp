#include "datasetState.hpp"

#include <typeinfo>


// Static map of type names
std::map<size_t, std::string> datasetState::_registered_names;

// Initialise static map of types
std::map<std::string, std::function<state_uptr(json&)>>&
datasetState::_registered_types() {
    static std::map<std::string, std::function<state_uptr(json&)>> _register;

    return _register;
}

state_uptr datasetState::_create(std::string name, json& data) {
    try {
        return _registered_types()[name](data);
    } catch (std::bad_function_call& e) {
        WARN_NON_OO("datasetManager: no state of type {:s} is registered.", name);
        return nullptr;
    }
}

state_uptr datasetState::from_json(json& data) {

    // Fetch the required properties from the json
    std::string dtype = data.at("type");
    json d = data.at("data");

    // Create and return the
    return datasetState::_create(dtype, d);
}

json datasetState::to_json() const {

    json j;

    // Use RTTI to serialise the type of datasetState this is
    j["type"] = datasetState::_registered_names[typeid(*this).hash_code()];
    j["data"] = data_to_json();

    return j;
}

// TODO: compare without serialization
bool datasetState::equals(datasetState& s) const {
    return to_json() == s.to_json();
}

std::set<std::string> datasetState::types() const {
    std::set<std::string> types;

    types.insert(datasetState::_registered_names[typeid(*this).hash_code()]);

    return types;
}

REGISTER_DATASET_STATE(freqState, "frequencies");
REGISTER_DATASET_STATE(inputState, "inputs");
REGISTER_DATASET_STATE(prodState, "products");
REGISTER_DATASET_STATE(stackState, "stack");
REGISTER_DATASET_STATE(eigenvalueState, "eigenvalues");
REGISTER_DATASET_STATE(timeState, "time");
REGISTER_DATASET_STATE(metadataState, "metadata");
REGISTER_DATASET_STATE(gatingState, "gating");
REGISTER_DATASET_STATE(acqDatasetIdState, "acq_dataset_id");
