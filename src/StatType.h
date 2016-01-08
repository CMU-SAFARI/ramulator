#ifndef __STATTYPE_H
#define __STATTYPE_H

#include <limits>
#include <fstream>
#include <string>
#include <vector>

#include <cassert>
#include <cmath>
#include <cstdlib>

namespace ramulator {

class ScalarStat;
class AverageStat;
class VectorStat;
class AverageVectorStat;
} // namespace ramulator

namespace Stats {

const double eps = 1e-8;

typedef unsigned int size_type;
typedef unsigned int off_type;
typedef double Counter;
typedef double Result;
typedef uint64_t Tick;
typedef std::vector<Counter> VCounter;
typedef std::vector<Result> VResult;
typedef std::numeric_limits<Counter> CounterLimits;

// Flags
const uint16_t init      = 0x00000001;
const uint16_t display   = 0x00000002;
const uint16_t total     = 0x00000010;
const uint16_t pdf       = 0x00000020;
const uint16_t cdf       = 0x00000040;
const uint16_t dist      = 0x00000080;
const uint16_t nozero    = 0x00000100;
const uint16_t nonan     = 0x00000200;

class Flags {
 protected:
  uint16_t flags;
 public:
  Flags(){}
  Flags(uint16_t flags):flags(flags){}
  void operator=(uint16_t _flags){flags = _flags;}
  bool is_total() const {return flags & total;}
  bool is_pdf() const {return flags & pdf;}
  bool is_nozero() const {return flags & nozero;}
  bool is_nonan() const {return flags & nonan;}
  bool is_cdf() const {return flags & cdf;}
  bool is_display() const {return flags & display;}
};

class StatBase {
 public:
  // TODO implement print for Distribution, Histogram,
  // AverageDeviation, StandardDeviation
  virtual void print(std::ofstream& file) = 0;

  virtual size_type size() const = 0;
  virtual bool zero() const = 0;
  virtual void prepare() = 0;
  virtual void reset() = 0;

  virtual VResult vresult() const { return VResult(); };
  virtual Result total() const { return Result(); };

  virtual bool is_display() const  = 0;
  virtual bool is_nozero() const = 0;
};

class StatList {
 protected:
  std::vector<StatBase*> list;
  std::ofstream stat_output;
 public:
  void add(StatBase* stat) {
    list.push_back(stat);
  }
  void output(std::string filename) {
    stat_output.open(filename.c_str(), std::ios_base::out);
    if (!stat_output.good()) {
      assert(false && "!stat_output.good()");
    }
  }
  void printall() {
    for(off_type i = 0 ; i < list.size() ; ++i) {
      if (!list[i]) {
        continue;
      }
      if (list[i]->is_nozero() && list[i]->zero()) {
        continue;
      }
      if (list[i]->is_display()) {
        list[i]->prepare();
        list[i]->print(stat_output);
      }
    }
  }
  ~StatList() {
    stat_output.close();
  }
};

extern StatList statlist;

template<class Derived>
class Stat : public StatBase {
 protected:
  std::string _name;
  std::string _desc;
  int _precision = 1;
  Flags _flags = display;
  std::string separatorString;
 public:
  Stat() {
    statlist.add(selfptr());
  }
  Derived &self() {return *static_cast<Derived*>(this);}
  Derived *selfptr() {return static_cast<Derived*>(this);}
  Derived &name(const std::string &__name) {
    _name = __name;
    return self();
  };
  Derived &desc(const std::string &__desc) {
    _desc = __desc;
    return self();
  };
  Derived &precision(int __precision) {
    _precision = __precision;
    return self();
  };
  Derived &flags(Flags __flags) {
    _flags = __flags;
    return self();
  };

  template <class GenericStat>
  Derived &prereq(const GenericStat & prereq) {
    // TODO deal with prereq;
    // only print the stat if the prereq is not zero.
    return self();
  }

  Derived &setSeparator(std::string str) {
    separatorString = str;
    return self();
  }
  const std::string& setSeparator() const {return separatorString;}

  size_type size() const { return 0; }

  virtual void print(std::ofstream& file) {};
  virtual void printname(std::ofstream& file) {
    file.width(40);
    file << _name;
  }

  virtual void printdesc(std::ofstream& file) {
    file.width(40);
    file << "# " << _desc << std::endl;
  }

  virtual bool is_display() const {
    return _flags.is_display();
  }

  virtual bool is_nozero() const {
    return _flags.is_nozero();
  }
};

template <class ScalarType>
class ScalarBase: public Stat<ScalarType> {
 public:
  virtual Counter value() const = 0;
  virtual Result result() const = 0;
  virtual Result total() const = 0;

  size_type size() const {return 1;}
  VResult vresult() const {return VResult(1, result());}

  virtual void print(std::ofstream& file) {
    Stat<ScalarType>::printname(file);
    // TODO deal with flag
    file.precision(Stat<ScalarType>::_precision);
    file.width(20);
    Result res = Stat<ScalarType>::self().result();
    file << std::fixed << res;
    Stat<ScalarType>::printdesc(file);
  }
};

class ConstValue: public ScalarBase<ConstValue> {
 private:
  Counter _value;
 public:
  ConstValue(Counter __value):_value(__value){}

  void operator ++ () { ++_value; }
  void operator -- () { --_value; }
  void operator ++ (int) { _value++; }
  void operator -- (int) { _value--; }

  template <typename U>
  void operator = (const U &v) { _value = v; }

  template <typename U>
  void operator += (const U &v) { _value += v;}

  template <typename U>
  void operator -= (const U &v) { _value -= v;}


  Counter value() const {return _value;}
  Result result() const {return (Result)_value;}
  Result total() const {return result();}
  bool zero() const {return (fabs(_value) < eps);}
  void prepare() {}
  void reset() {}
};

class Scalar: public ScalarBase<Scalar> {
 private:
  Counter _value;
 public:
  Scalar():_value(0) {}
  Counter value() const {return _value;}
  Result result() const {return (Result)_value;}
  Result total() const {return (Result)_value;}

  void operator ++ () { ++_value; }
  void operator -- () { --_value; }
  void operator ++ (int) { _value++; }
  void operator -- (int) { _value--; }

  template <typename U>
  void operator = (const U &v) { _value = v; }

  template <typename U>
  void operator += (const U &v) { _value += v;}

  template <typename U>
  void operator -= (const U &v) { _value -= v;}


  virtual bool zero() const {return (fabs(_value) < eps);}
  void prepare() {}
  void reset() {_value = Counter();}

};

extern Tick curTick;

class Average: public ScalarBase<Average> {
 private:
  Counter current;
  Tick lastReset;
  Result total_val;
  Tick last;
 public:
  Average():current(0), lastReset(0), total_val(0), last(0){}

  void set(Counter val) {
    total_val += current * (curTick - last);
    last = curTick;
    current = val;
  }
  void inc(Counter val) {
    set(current + val);
  }
  void dec(Counter val) {
    set(current - val);
  }
  void operator ++ () { inc(1); }
  void operator -- () { dec(1); }
  void operator ++ (int) { inc(1); }
  void operator -- (int) { dec(1); }

  template <typename U>
  void operator = (const U &v) { set(v); }

  template <typename U>
  void operator += (const U &v) { inc(v);}

  template <typename U>
  void operator -= (const U &v) { dec(v);}


  bool zero() const { return (fabs(total_val) < eps); }
  void prepare() {
    total_val += current * (curTick - last);
    last = curTick;
  }
  void reset() {
    total_val = 0.0;
    last = curTick;
    lastReset = curTick;
  }

  Counter value() const { return current; }
  Result result() const {
    assert(last == curTick);
    return (Result)(total_val + current)/ (Result)(curTick - lastReset + 1);
  }
  Result total() const {return result();}
};

template<class Derived, class Element>
class VectorBase: public Stat<Derived> {
 private:
  size_type _size = 0;
  std::vector<Element> data;

 public:
  void init(size_type __size) {
    _size = __size;
    data.resize(size());
    for (off_type i = 0 ; i < size() ; ++i) {
      data[i].flags(0)
             .name("[" + std::string(1, char(i + '0')) + "]");
    }
  }
  size_type size() const {return _size;}
  // Copy the values to a local vector and return a reference to it.
  void value(VCounter& vec) const {
    vec.resize(size());
    for (off_type i = 0 ; i < size() ; ++i) {
      vec[i] = data[i].value();
    }
  }
  // Copy the results to a local vector and return a reference to it.
  void result(VResult& vec) const {
    vec.resize(size());
    for (off_type i = 0 ; i < size() ; ++i) {
      vec[i] = data[i].result();
    }
  }

  Result total() const {
    Result sum = 0.0;
    for (off_type i = 0 ; i < size() ; ++i) {
      sum += data[i].result();
    }
    return sum;
  }

  VResult vresult() const {
    VResult vres;
    for (off_type i = 0 ; i < size() ; ++i) {
      vres[i] = data[i].result();
    }
    return vres;
  }

  bool check() const {
    // We don't separate storage and access as gem5 does.
    // So here is always true.
    return true;
  }

  Element &operator[](off_type index) {
    assert(index >= 0 && index < size());
    return data[index];
  }

  bool zero() const {
    return (fabs(total()) < eps);
  }

  void prepare() {
    for (off_type i = 0 ; i < size() ; ++i) {
      data[i].prepare();
    }
  }
  void reset() {
    for (off_type i = 0 ; i < size() ; ++i) {
      data[i].reset();
    }
  }
  void print(std::ofstream& file) {
    Stat<Derived>::printname(file);
    file.precision(Stat<Derived>::_precision);
    file.width(20);
    file << std::fixed << total();
    Stat<Derived>::printdesc(file);
    for (off_type i = 0 ; i < size() ; ++i) {
      data[i].print(file);
    }
  }
};

class Vector: public VectorBase<Vector, Scalar> {
};

class AverageVector: public VectorBase<AverageVector, Average> {
};

class Distribution: public Stat<Distribution> {
 private:
  // Parameter part:
  Counter param_min;
  Counter param_max;
  Counter param_bucket_size;
  Counter param_buckets;

  // The minimum value to track
  Counter min_track;
  // The maximum value to track
  Counter max_track;
  // The number of entries in each bucket
  Counter bucket_size;

  Counter min_val;
  Counter max_val;
  // The number of values sampled less than min
  Counter underflow;
  // The number of values sampled more than max
  Counter overflow;
  // The current sum
  Counter sum;
  // The sum of squares
  Counter squares;
  // The number of samples
  Counter samples;
  // Counter for each bucket
  VCounter cvec;

 public:
  Distribution():param_min(Counter()), param_max(Counter()),
      param_bucket_size(Counter()) { reset(); }
  void init(Counter min, Counter max, Counter bkt) {
    param_min = min;
    param_max = max;
    param_bucket_size = bkt;
    param_buckets = (size_type)ceil((max - min + 1.0) / bkt);
    cvec.resize(param_buckets);

    reset();
  }
  void sample(Counter val, int number) {
    if (val < min_track)
      underflow += number;
    else if (val > max_track)
      overflow += number;
    else {
      size_type index =
          (size_type)std::floor((val - min_track) / bucket_size);
      assert(index < size());
      cvec[index] += number;
    }

    if (val < min_val)
      min_val = val;

    if (val > max_val)
      max_val = val;

    sum += val * number;
    squares += val * val * number;
    samples += number;
  }

  size_type size() const {return cvec.size();}
  bool zero() const {
    return (fabs(samples) < eps);
  }
  void prepare() {};
  void reset() {
    min_track = param_min;
    max_track = param_max;
    bucket_size = param_bucket_size;

    min_val = CounterLimits::max();
    max_val = CounterLimits::min();
    underflow = Counter();
    overflow = Counter();

    size_type _size = cvec.size();
    for (off_type i = 0 ; i < _size ; ++i) {
      cvec[i] = Counter();
    }

    sum = Counter();
    squares = Counter();
    samples = Counter();
  };
  void add(Distribution &d) {
    size_type d_size = d.size();
    assert(size() == d_size);
    assert(min_track == d.min_track);
    assert(max_track == d.max_track);

    underflow += d.underflow;
    overflow += d.overflow;

    sum += d.sum;
    squares += d.squares;
    samples += d.samples;

    if (d.min_val < min_val) {
      min_val = d.min_val;
    }

    if (d.max_val > max_val) {
      max_val = d.max_val;
    }

    for (off_type i = 0 ; i < d_size ; ++i) {
      cvec[i] += d.cvec[i];
    }
  }
};

class Histogram: public Stat<Histogram> {
 private:
  size_type param_buckets;

  Counter min_bucket;
  Counter max_bucket;
  Counter bucket_size;

  Counter sum;
  Counter logs;
  Counter squares;
  Counter samples;
  VCounter cvec;

 public:
  Histogram():param_buckets(0) { reset(); }
  Histogram(size_type __buckets):cvec(__buckets) {
    init(__buckets);
  }
  void init(size_type __buckets) {
    cvec.resize(__buckets);
    param_buckets = __buckets;
    reset();
  }

  void grow_up();
  void grow_out();
  void grow_convert();
  void add(Histogram& hs);
  void sample(Counter val, int number);

  bool zero() const {
    return (fabs(samples) < eps);
  }
  void prepare() {}
  void reset() {
    min_bucket = 0;
    max_bucket = param_buckets - 1;
    bucket_size = 1;

    size_type size = param_buckets;
    for (off_type i = 0 ; i < size ; ++i) {
      cvec[i] = Counter();
    }

    sum = Counter();
    squares = Counter();
    samples = Counter();
    logs = Counter();
  }

  size_type size() const {return param_buckets;}
};

class StandardDeviation: public Stat<StandardDeviation> {
 private:
  Counter sum;
  Counter squares;
  Counter samples;

 public:
  StandardDeviation():sum(Counter()), squares(Counter()),
      samples(Counter()) {}
  void sample(Counter val, int number) {
    Counter value = val * number;
    sum += value;
    squares += value * value;
    samples += number;
  }
  size_type size() const {return 1;}
  bool zero() const {return (fabs(samples) < eps);}
  void prepare() {}
  void reset() {
    sum = Counter();
    squares = Counter();
    samples = Counter();
  }
  void add(StandardDeviation& sd) {
    sum += sd.sum;
    squares += sd.squares;
    samples += sd.samples;
  }
};

class AverageDeviation: public Stat<AverageDeviation> {
 private:
  Counter sum;
  Counter squares;

 public:
  AverageDeviation():sum(Counter()), squares(Counter()) {}
  void sample(Counter val, int number) {
    Counter value = val * number;
    sum += value;
    squares += value * value;
  }
  size_type size() const {return 1;}
  bool zero() const {return (fabs(sum) < eps);}
  void prepare() {}
  void reset() {
    sum = Counter();
    squares = Counter();
  }
  void add(AverageDeviation& ad) {
    sum += ad.sum;
    squares += ad.squares;
  }
};

class Op {
 private:
  std::string opstring;
 public:
  Op() {}
  Op(std::string __opstring):opstring(__opstring){}
  Result operator() (Result r) const {
    if (opstring == "-") {
      return -r;
    } else {
      assert("Unary operation can only be unary negation." && false);
    }
  }
  Result operator() (Result l, Result r) const {
    if (opstring == "+") {
      return l + r;
    } else if (opstring == "-") {
      return l - r;
    } else if (opstring == "*") {
      return l * r;
    } else if (opstring == "/") {
      assert(fabs(r) > 1e-8 || "divide zero error");
      return l / r;
    } else {
      assert("invalid binary opstring " && false);
    }
  }
};

} // namespace Stats

#endif
