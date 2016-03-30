//
// Created by dt10 on 29/03/2016.
//

#ifndef HLS_PARSER_WEIGHTED_SHUFFLE_HPP
#define HLS_PARSER_WEIGHTED_SHUFFLE_HPP

#include "key_value_set.hpp"

#include <algorithm>

std::vector<double> calculateBitWeights(const key_value_set &keys)
{
    std::vector<int> ones(keys.getKeyWidth()), zeros(keys.getKeyWidth());
    for(const auto &kv : keys){
        const auto &key=kv.first;
        for(unsigned i=0; i<keys.getKeyWidth(); i++){
            int b=key[i];
            if(b!=-1){
                if(b) ones[i]++;
                else zeros[i]++;
            }
        }
    }

    /* If it occasionally matters that a bit is 1, but never matters that a bit
     * is 0, then we never need to look at it. Conversely, if it matters that it
     * is 1, but not 0, then we never need to look at it. This is equivalent to
     * looking at MSB+1: it is always 0, so we never need to look at it.
     *
     * If all bits are needed in all cases (i.e. no don't cares), then entropy
     * is a good model of the relative importance. If some bits are occasionally
     * don't cares, then it seems appropriate to linearly scale the entropy.
     */

    auto entropy=[](int h, int t) -> double
    {
        if(h==0 || t==0){
            return 0.0; // We never need to look at it
        }
        double p=h/(double)(h+t);
        return -p*log2(p) - (1-p)*log2(1-p);
    };

    std::vector<double> weights(keys.getKeyWidth());
    for(unsigned i=0;i<weights.size();i++){
        double e=entropy(ones[i],zeros[i]);
        double w=(ones[i]+zeros[i])*e;
        weights[i]=w;
    }

    double scale=1.0/std::accumulate(weights.begin(), weights.end(), 0.0);

    for(unsigned i=0;i<weights.size();i++){
        weights[i] *= scale;
    }

    return weights;
}

template<class TRng>
std::vector<std::vector<unsigned> > makeWeightedRandomShuffle(TRng &rng,  unsigned wO, const std::vector<double> &weights, unsigned wA)
{
    unsigned wI=weights.size();

    std::vector<std::set<unsigned> > res(wO);

    std::vector<int> neededWeights;
    for(unsigned i=0;i<weights.size();i++){
        if(weights[i]>0)
            neededWeights.push_back(i);
    }

    // Ensure every needed input is in at least one address
    for(unsigned i=0;i<neededWeights.size();i++){
        res[i%wO].insert(neededWeights[i]);
    }

    std::discrete_distribution<unsigned> dist(weights.begin(), weights.end());

    // Distribute the rest randomly
    for(unsigned i=0; i<wO; i++){
        while(res[i].size() < std::min(wI,wA) ){
            unsigned v=dist(rng);
            res[i].insert(v);
        }
    }

    std::vector<std::vector<unsigned> > rres;
    for(auto &x : res){
        rres.emplace_back(x.begin(), x.end());
    }

    canonicaliseShuffle(rres);

    return rres;
}


template<class TRng>
BitHash makeWeightedBitHash(TRng &rng, const key_value_set &keys, unsigned wO, unsigned wI, unsigned wA)
{
    auto weights=calculateBitWeights(keys);

    for(unsigned i=0;i<weights.size();i++){
        std::cerr<<"  weight["<<i<<"] = "<<weights[i]<<"\n";
    }

    auto shuffle=makeWeightedRandomShuffle(rng, wO, weights, wA);

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

#endif //HLS_PARSER_WEIGHTED_SHUFFLE_HPP
