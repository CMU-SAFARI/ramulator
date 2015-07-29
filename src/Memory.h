#ifndef __MEMORY_H
#define __MEMORY_H

#include "DRAM.h"
#include "Request.h"
#include "Controller.h"
#include <vector>
#include <functional>
#include <cmath>
#include <cassert>
#include <tuple>

using namespace std;

namespace ramulator
{

class MemoryBase{
public:
    MemoryBase() {}
    virtual ~MemoryBase() {}
    virtual double clk_ns() = 0;
    virtual void tick() = 0;
    virtual bool send(Request req) = 0;
    virtual int pending_requests() = 0;
};

template <class T, template<typename> class Controller = Controller >
class Memory : public MemoryBase
{
public:
    enum class Type {
        ChRaBaRoCo,
        RoBaRaCoCh,
        MAX,
    } type = Type::RoBaRaCoCh;

    vector<Controller<T>*> ctrls;
    T * spec;
    vector<int> addr_bits;

    int tx_bits;

    Memory(vector<Controller<T>*> ctrls)
        : ctrls(ctrls),
          spec(ctrls[0]->channel->spec),
          addr_bits(int(T::Level::MAX))
    {
        // make sure 2^N channels/ranks
        int *sz = spec->org_entry.count;
        assert((sz[0] & (sz[0] - 1)) == 0);
        assert((sz[1] & (sz[1] - 1)) == 0);
        // validate size of one transaction
        int tx = (spec->prefetch_size * spec->channel_width / 8);
        tx_bits = calc_log2(tx);
        assert((1<<tx_bits) == tx);
        // If hi address bits will not be assigned to Rows
        // then the chips must not be LPDDRx 6Gb, 12Gb etc.
        if (type != Type::RoBaRaCoCh && spec->standard_name.substr(0, 5) == "LPDDR")
            assert((sz[int(T::Level::Row)] & (sz[int(T::Level::Row)] - 1)) == 0);

        // The number of channels and ranks are set when a spec class is
        // initialized. However the initialization does not update the channel
        // and rank count in org_entry.count (*sz), and it's not supposed to.
        for (unsigned int lev = 0; lev < addr_bits.size(); lev++) {
            if (lev == int(T::Level::Channel))
              addr_bits[lev] = calc_log2(ctrls.size());
            else if (lev == int(T::Level::Rank))
              addr_bits[lev] = calc_log2(ctrls[0]->channel->children.size());
            else
              addr_bits[lev] = calc_log2(sz[lev]);
        }

        addr_bits[int(T::Level::MAX) - 1] -= calc_log2(spec->prefetch_size);
    }

    ~Memory()
    {
        for (auto ctrl: ctrls)
            delete ctrl;
        delete spec;
    }

    double clk_ns()
    {
        return spec->speed_entry.tCK;
    }

    void tick()
    {
        for (auto ctrl : ctrls)
            ctrl->tick();
    }

    bool send(Request req)
    {
        req.addr_vec.resize(addr_bits.size());
        long addr = req.addr;
        assert(slice_lower_bits(addr, tx_bits) == 0); // check address alignment

        switch(int(type)){
            case int(Type::ChRaBaRoCo):
                for (int i = addr_bits.size() - 1; i >= 0; i--)
                    req.addr_vec[i] = slice_lower_bits(addr, addr_bits[i]);
                break;
            case int(Type::RoBaRaCoCh):
                req.addr_vec[0] = slice_lower_bits(addr, addr_bits[0]);
                req.addr_vec[addr_bits.size() - 1] = slice_lower_bits(addr, addr_bits[addr_bits.size() - 1]);
                for (int i = 1; i <= int(T::Level::Row); i++)
                    req.addr_vec[i] = slice_lower_bits(addr, addr_bits[i]);
                break;
            default:
                assert(false);
        }
        // req.addr_phy = req.addr_vec[0];
        // for (int i = 1; i < addr_bits.size(); i ++)
        //     req.addr_phy = (req.addr_phy<<addr_bits[i]) + req.addr_vec[i];
        // req.addr_phy <<= tx_bits;
        // assert(addr == 0); // check address is within range

        // dispatch to the right channel
        return ctrls[req.addr_vec[0]]->enqueue(req);
    }

    int pending_requests()
    {
        int reqs = 0;
        for (auto ctrl: ctrls)
            reqs += ctrl->readq.size() + ctrl->writeq.size() + ctrl->otherq.size() + ctrl->pending.size();
        return reqs;
    }

private:

    int calc_log2(int val){
        int n = 0;
        while ((val >>= 1))
            n ++;
        return n;
    }
    int slice_lower_bits(long& addr, int bits)
    {
        int lbits = addr & ((1<<bits) - 1);
        addr >>= bits;
        return lbits;
    }
};

} /*namespace ramulator*/

#endif /*__MEMORY_H*/
