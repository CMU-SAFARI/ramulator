#include "MemoryFactory.h"
#include "LPDDR4.h"
#include "WideIO.h"
#include "WideIO2.h"
#include "HBM.h"
#include "SALP.h"

using namespace ramulator;

namespace ramulator
{

template <>
void MemoryFactory<LPDDR4>::validate(int channels, int ranks, const Config& configs) {
    assert(channels >= 2 && "LPDDR4 requires 2, 4, 8 ... channels");
}

template <>
void MemoryFactory<WideIO>::validate(int channels, int ranks, const Config& configs) {
    assert(channels == 4 && "WideIO comes with 4 channels");
}

template <>
void MemoryFactory<WideIO2>::validate(int channels, int ranks, const Config& configs) {
    assert((channels == 4 || channels == 8) && "WideIO2 comes with 4 or 8 channels");
    assert((ranks == 1 || ranks == 2) && "WideIO2 comes with 1 or 2 ranks");
}

template <>
void MemoryFactory<HBM>::validate(int channels, int ranks, const Config& configs) {
    assert(channels == 8 && "HBM comes with 8 channels");
}

template <>
MemoryBase *MemoryFactory<WideIO2>::create(const Config& configs, int cacheline) {
    int channels = stoi(configs["channels"], NULL, 0);
    int ranks = stoi(configs["ranks"], NULL, 0);
    validate(channels, ranks, configs);

    const string& org_name = configs["org"];
    const string& speed_name = configs["speed"];

    WideIO2 *spec = new WideIO2(org_name, speed_name, channels);

    extend_channel_width(spec, cacheline);

    return (MemoryBase *)populate_memory(configs, spec, channels, ranks);
}


template <>
MemoryBase *MemoryFactory<SALP>::create(const Config& configs, int cacheline) {
    int channels = stoi(configs["channels"], NULL, 0);
    int ranks = stoi(configs["ranks"], NULL, 0);
    int subarrays = stoi(configs["subarrays"], NULL, 0);
    validate(channels, ranks, configs);

    const string& std_name = configs["standard"];
    const string& org_name = configs["org"];
    const string& speed_name = configs["speed"];

    SALP *spec = new SALP(org_name, speed_name, std_name, subarrays);

    extend_channel_width(spec, cacheline);

    return (MemoryBase *)populate_memory(configs, spec, channels, ranks);
}

}

// This function can be used by autoconf AC_CHECK_LIB since
// apparently it can't detect C++ functions.
// Basically just an entry in the symbol table
extern "C"
{
    void libramulator_is_present(void)
    {
        ;
    }
}
