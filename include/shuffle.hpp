#ifndef shuffle_hpp
#define shuffle_hpp

#include <vector>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <set>


/*

  Given 1..wI we want to find wO sets of wA integers such that:
  - Each integer occurrs in at least one set
  - each set is distinct (really needed?)
  - the overlap between sets is minimal

  Assume wI > wA, otherwise it is pointless.
  Assume wA*wO >= wI, otherwise it is impossible
  
  We will canonicalise all sets (i.e. address taps) so that
  they are into a strictly ascending order (no repeats).

  We will also conicalise a shuffle by sorting the address
  sets in non-strict ascending order (as there may be repeats).
  Given we don't care what the actual output bits of the hash
  are, we might as well always put the largest number of taps
  at the start.
 */

void printShuffle(std::ostream &dst, const std::vector<std::vector<unsigned> > &shuffle)
{
    for(unsigned i=0;i<shuffle.size(); i++){
        dst<<i<<"=[";
        for(unsigned j=0;j<shuffle[i].size(); j++){
            if(j!=0)
                dst<<",";
            dst<<shuffle[i][j];
        }
        dst<<"]\n";
    }
}

void canonicaliseShuffleAddress(std::vector<unsigned> &addresses)
{
  std::sort(addresses.begin(), addresses.end());
  addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());
}


struct lex_compare_shuffle
{
    template<class TVec>
    bool operator()(const TVec &v0, const TVec &v1) const
    { return lexicographical_compare(v0.begin(), v0.end(), v1.begin(), v1.end()); }
};

void canonicaliseShuffle(std::vector<std::vector<unsigned> > &shuffle)
{
    for(unsigned i=0;i<shuffle.size();i++){
        canonicaliseShuffleAddress(shuffle[i]);
    }
    std::sort(shuffle.begin(), shuffle.end(), lex_compare_shuffle());
}

unsigned metricShuffleAddress(const std::vector<unsigned> &a0, const std::vector<unsigned> &a1)
{
  unsigned different=0;

  auto i0=a0.begin(), i1=a1.end();
  while(i0!=a0.end() && i1!=a1.end()){
    if(*i0 < *i1){
      ++different;
      ++i0;
    }else if(*i0 > *i1){
      ++different;
      ++i1;
    }else{
      ++i0;
      ++i1;
    }
  }
  different += a0.end()-i0;
  different += a1.end()-i1;
  return different;
}

bool isValidShuffle(unsigned wI, const std::vector<std::vector<unsigned> > &shuffle)
{
  unsigned hit=0;
  std::vector<int> mask(wI,0);

  for(unsigned i=0; i<shuffle.size(); i++){
    for(unsigned j=0; j<shuffle[i].size(); j++){
      if(!mask[shuffle[i][j]]){
	mask[shuffle[i][j]]=1;
	hit++;
	if(hit==wI)
	  return true;
      }
    }
  }
  return false;
}

unsigned evaluateShuffle(const std::vector<std::vector<unsigned> > &shuffle)
{
  unsigned acc=0;
  for(unsigned i=0;i<shuffle.size()-1; i++){
    for(unsigned j=i+1;j<shuffle.size();j++){
      acc += metricShuffleAddress(shuffle[i],shuffle[j]);
    }
  }
  return acc;
}


template<class TRng>
std::vector<std::vector<unsigned> > makeRandomShuffle(TRng &rng, unsigned wO, unsigned wI, unsigned wA)
{
  std::vector<std::set<unsigned> > res(wO);

  // Ensure every input is in at least one address
  for(unsigned i=0;i<wI;i++){
    res[i%wO].insert(i);
  }

  // Distribute the rest randomly
  for(unsigned i=0; i<wO; i++){
      while(res[i].size() < std::min(wI,wA) ){
          res[i].insert(rng() % wI);
      }
  }

    std::vector<std::vector<unsigned> > rres;
    for(auto &x : res){
        rres.emplace_back(x.begin(), x.end());
    }

  canonicaliseShuffle(rres);

  return rres;
}


#endif
