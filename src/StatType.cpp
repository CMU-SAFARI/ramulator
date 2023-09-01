#include "StatType.h"

namespace Stats {

// Statistics list
StatList statlist;

// The smallest timing granularity.
Tick curTick = 0;

std::vector<StatBase*> all_stats;
void reset_stats() {
    for(auto s : all_stats)
        s->reset();
}

void
Histogram::grow_out()
{
    int size = cvec.size();
    int zero = size / 2; // round down!
    int top_half = zero + (size - zero + 1) / 2; // round up!
    int bottom_half = (size - zero) / 2; // round down!

    // grow down
    int low_pair = zero - 1;
    for (int i = zero - 1; i >= bottom_half; i--) {
        cvec[i] = cvec[low_pair];
        if (low_pair - 1 >= 0)
            cvec[i] += cvec[low_pair - 1];
        low_pair -= 2;
    }
    assert(low_pair == 0 || low_pair == -1 || low_pair == -2);

    for (int i = bottom_half - 1; i >= 0; i--)
        cvec[i] = Counter();

    // grow up
    int high_pair = zero;
    for (int i = zero; i < top_half; i++) {
        cvec[i] = cvec[high_pair];
        if (high_pair + 1 < size)
            cvec[i] += cvec[high_pair + 1];
        high_pair += 2;
    }
    assert(high_pair == size || high_pair == size + 1);

    for (int i = top_half; i < size; i++)
        cvec[i] = Counter();

    max_bucket *= 2;
    min_bucket *= 2;
    bucket_size *= 2;
}

void
Histogram::grow_convert()
{
    int size = cvec.size();
    int half = (size + 1) / 2; // round up!
    //bool even = (size & 1) == 0;

    int pair = size - 1;
    for (int i = size - 1; i >= half; --i) {
        cvec[i] = cvec[pair];
        if (pair - 1 >= 0)
            cvec[i] += cvec[pair - 1];
        pair -= 2;
    }

    for (int i = half - 1; i >= 0; i--)
        cvec[i] = Counter();

    min_bucket = -max_bucket;// - (even ? bucket_size : 0);
    bucket_size *= 2;
}

void
Histogram::grow_up()
{
    int size = cvec.size();
    int half = (size + 1) / 2; // round up!

    int pair = 0;
    for (int i = 0; i < half; i++) {
        cvec[i] = cvec[pair];
        if (pair + 1 < size)
            cvec[i] += cvec[pair + 1];
        pair += 2;
    }
    assert(pair == size || pair == size + 1);

    for (int i = half; i < size; i++)
        cvec[i] = Counter();

    max_bucket *= 2;
    bucket_size *= 2;
}

void
Histogram::add(Histogram &hs)
{
    size_type b_size = hs.size();
    assert(size() == b_size);
    assert(min_bucket == hs.min_bucket);

    sum += hs.sum;
    logs += hs.logs;
    squares += hs.squares;
    samples += hs.samples;

    while(bucket_size > hs.bucket_size)
        hs.grow_up();
    while(bucket_size < hs.bucket_size)
        grow_up();

    for (uint32_t i = 0; i < b_size; i++)
        cvec[i] += hs.cvec[i];
}

void
Histogram::sample(Counter val, int number)
{
    assert(min_bucket < max_bucket);
    if (val < min_bucket) {
        if (min_bucket == 0)
            grow_convert();

        while (val < min_bucket)
            grow_out();
    } else if (val >= max_bucket + bucket_size) {
        if (min_bucket == 0) {
            while (val >= max_bucket + bucket_size)
                grow_up();
        } else {
            while (val >= max_bucket + bucket_size)
                grow_out();
        }
    }

    size_type index =
        (int64_t)std::floor((val - min_bucket) / bucket_size);

    assert(index >= 0 && index < size());
    cvec[index] += number;

    sum += val * number;
    squares += val * val * number;
    logs += log(val) * number;
    samples += number;
}

} /* namespace Stats */
