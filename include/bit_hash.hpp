#ifndef bit_hash_hpp
#define bit_hash_hpp

#include <vector>
#include <cassert>
#include <map>
#include <iostream>
#include <set>
#include <random>

#include "shuffle.hpp"
#include "bit_vector.hpp"
#include "key_value_set.hpp"

struct BitHash
{
  struct table
  {
    std::vector<unsigned> selectors;
    std::vector<int> lut; // 0=0, 1=1, -1=undecided

      bool operator==(const table &o)const
      { return selectors==o.selectors && lut==o.lut; }

      bool operator<(const table &o) const
      {
          if(selectors<o.selectors)
              return true;
          if(selectors>o.selectors)
              return true;
          return lut < o.lut;
      }

      void print(std::ostream &dst, unsigned idx, std::string prefix="") const
      {
          assert(lut.size()==(1u<<selectors.size()));

          dst<<prefix<<"table "<<idx<<" "<<selectors.size()<<"\n";

          dst<<prefix<<"  sel";
          for(unsigned i=0;i<selectors.size();i++){
              dst<<" "<<selectors[i];
          }
          dst<<"\n";

          dst<<prefix<<"  lut "<<bit_vector(lut)<<"\n";
      }

    table()
    {}

      template<class TRng>
      void bind_random(TRng &rng, double prob)
      {
          std::uniform_real_distribution<double> udist;

          for(int &v : lut){
              if(v==-1){
                  double p=udist(rng);
                  if(p<prob){
                      v = udist(rng) < 0.5 ? 0 : 1;
                  }
              }
          }
      }

      unsigned address(unsigned key) const
      {

          unsigned addr=0;
          for(unsigned i=0;i<selectors.size();i++){
              addr = addr | (((key>>selectors[i])&1)<<i);
          }
          return addr;
      }

      unsigned address(const bit_vector &key) const
      {
          unsigned addr=0;
          for(unsigned i=0;i<selectors.size();i++){
              int bit=key[selectors[i]];
              if(bit==-1)
                  throw std::runtime_error("Cannot lookup non-concrete key.");
              addr = addr | (unsigned(bit)<<i);
          }
          return addr;
      }

    int operator()(unsigned x) const
    {
        int res=lut.at(address(x));
        assert((res==0)||(res==1)||(res==-1));
        return res;
    }

      int operator()(const bit_vector &x) const
      {
          return lut.at(address(x));
      }
  };

  unsigned wI;
  unsigned wO;
  std::vector<table> tables;

    bool operator==(const BitHash &o) const
    { return wI==o.wI && wO==o.wO && tables==o.tables; }

    bool operator<(const BitHash &o) const {
        if (wI < o.wI) return true;
        if (wI > o.wI) return false;
        if (wO < o.wO) return true;
        if (wO > o.wO) return false;
        for (unsigned i = 0; i < tables.size(); i++) {
            if (tables[i] < o.tables.at(i))
                return true;
            if (o.tables.at(i) < tables[i])
                return false;
        }

        return false;
    }

    void print(std::ostream &dst, std::string prefix="") const
    {
        assert(tables.size()==wO);
        dst<<prefix<<"BitHashBegin "<<wO<<" "<<wI<<"\n";
        for(unsigned i=0;i<tables.size();i++){
            tables[i].print(dst,i,prefix+"  ");
        }
        dst<<prefix<<"BitHashEnd\n";
    }

  unsigned operator()(unsigned x) const
  {
      assert(x<(1u<<wI));
      assert(wO==tables.size());

      unsigned acc=0;
      for(unsigned i=0;i<wO;i++){
          int bit=tables[i](x);
          assert((bit==0)||(bit==1)); // Don't allow unknowns here
          acc=acc|(unsigned(bit)<<i);
      }
      return acc;
  }

    unsigned operator()(const bit_vector &x) const
    {
        assert(x.size()<(1u<<wI));
        assert(wO==tables.size());

        unsigned acc=0;
        for(unsigned i=0;i<wO;i++){
            int bit=tables[i](x);
            assert((bit==0)||(bit==1)); // Don't allow unknowns here
            acc=acc|(unsigned(bit)<<i);
        }
        return acc;
    }

    template<class TRng>
    void bind_random(TRng &rng, double prob)
    {
        for(auto &t : tables){
            t.bind_random(rng,prob);
        }
    }

    bool is_solution(const key_value_set &keys) const
    {
        std::set<unsigned> hits;

        for(const auto &kv : keys){
            const auto &k=kv.first;

            auto it=k.variants_begin();
            unsigned h=(*this)(*it);
            //std::cerr<<"  "<<k<<" : "<<h<<"\n";

            auto i=hits.insert(h);
            if(!i.second)
                return false;

            ++it;
            auto end=k.variants_end();
            while(it!=end){
                unsigned ho=(*this)(*it);
                if(ho!=h)
                    return false;
                ++it;
            }
        }
        return true;
    }

};


uint64_t hashForLutEntry(unsigned lutIndex, unsigned lutOffset, int value )
{
    const uint64_t S1 = 33554467; // next_prime(2^25)
    const uint64_t S2 = 67108879; // next_prime(2^26)

    assert((-1<=value) && (value<=+1));
    return (lutIndex*S1+lutOffset)*S2 * (value+2);
}

uint64_t hashForTap(unsigned bitIndex, unsigned tapOffset, unsigned index )
{
    const uint64_t S1 = 134217757; // next_prime(2^27)
    const uint64_t S2 = 268435459; // next_prime(2^28)

    return (bitIndex*S1+tapOffset)*S2 * (index+1);
}



/* The hash is designed to be incrementally updatable as the
 * parts of a hash are changed. So it is just the modulo-2^64 sum of
 * a bunch of components based on the taps and lut entries
 */
uint64_t hash(const BitHash &bh)
{
    uint64_t acc=0;
    for(unsigned i=0;i<bh.tables.size();i++){
        const auto & t = bh.tables[i];
        for(unsigned j=0; j<t.selectors.size(); j++){
            acc += hashForTap(i, j, t.selectors[j]);
        }
        for(unsigned j=0; j<t.lut.size(); j++){
            acc += hashForLutEntry(i, j, t.lut[j]);
        }
    }
    return acc;
}

BitHash parse_bit_hash(std::istream &src)
{
    auto expect=[&](const char *str) -> std::istream &
    {
        std::string tmp;
        src>>tmp;
        if(tmp.empty() || tmp!=str)
            throw std::runtime_error(std::string("Expected string '")+str+"' but got '"+tmp+"'");
        return src;
    };

    BitHash res;

    expect("BitHashBegin")>>res.wO>>res.wI;
    for(unsigned i=0;i<res.wO;i++){
        res.tables.push_back(BitHash::table{});
        auto &t = res.tables.back();
        unsigned idx, wa;
        expect("table")>>idx>>wa;
        if(idx!=i)
            throw std::runtime_error("Persisted bit hash is corrupt.");

        t.selectors.resize(wa);
        t.lut.resize(1<<wa);

        expect("sel");

        for(unsigned j=0;j<t.selectors.size();j++){
            src>>t.selectors[j];
        }

        bit_vector bv;
        expect("lut")>>bv;
        for(unsigned j=0;j<t.lut.size();j++){
            t.lut[j]=bv[j];
        }
    }
    expect("BitHashEnd");

    return res;
}

template<class TRng>
BitHash makeBitHash(TRng &rng, unsigned wO, unsigned wI, unsigned wA)
{
    auto shuffle=makeRandomShuffle(rng, wO, wI, wA);

    BitHash hash;
    hash.wI=wI;
    hash.wO=wO;
    for(unsigned i=0; i<wO; i++){
        hash.tables.push_back(BitHash::table());
        hash.tables.back().selectors=shuffle[i];
        hash.tables.back().lut=std::vector<int>(1<<(shuffle[i].size()),-1);
    }

    return hash;
}

template<class TRng>
BitHash makeBitHashConcrete(TRng &rng, unsigned wO, unsigned wI, unsigned wA)
{
    auto hash=makeBitHash(rng,wO,wI,wA);
    for(auto &t : hash.tables){
        for(int &le : t.lut){
            le=rng()%2;
        }
    }
    return hash;
}



#endif
