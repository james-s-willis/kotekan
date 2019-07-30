#include "cudaEventContainer.hpp"

#include "errors.h"

#include <unistd.h>

void cudaEventContainer::set(void* sig) {
    signal = (cudaEvent_t)sig;
}

void* cudaEventContainer::get() {
    return signal;
}

void cudaEventContainer::unset() {
    signal = NULL;
}

void cudaEventContainer::wait() {
    CHECK_CUDA_ERROR(cudaEventSynchronize(signal));
}
