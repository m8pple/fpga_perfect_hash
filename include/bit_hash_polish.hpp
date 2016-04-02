//
// Created by dt10 on 01/04/2016.
//

#ifndef FPGA_PERFECT_HASH_BIT_HASH_POLISH_HPP
#define FPGA_PERFECT_HASH_BIT_HASH_POLISH_HPP

#include "bit_hash_cnf.hpp"
#include "bit_hash_anneal.hpp"

/* In a non-working solution, we are going to get a few clashes, but hopefully a
 * small number. If we have wO bits, and there are nC clashing pairs, then in the
 * worst case there are 2*nC*wO bits involved in those clashes. For example, with
 * wO=8, nC=8, wA=6, we would have at most 128 bits directly involved in the clash,
 * out of a total of 8*(1<<6) = 512 bits.
 *
 * The hypothesis is that we can unbind those clashing bits, while leaving the others
 * in place, and hopefully re-solve the smaller problem. This may be a stupid idea,
 * as there is no real reason to think that a close solution found by some kind
 * of heuristic search is going to make it easier for a sat solver to get to the
 * solution - we area already likely to be stuck in some local minima.
 * */

/* Calculate a list of keys which clash, plus the number of clashes they are involved in */
std::vector<std::pair<bit_vector,int> > find_clashing_keys(const BitHash &bh, const key_value_set &problem)
{
    std::vector<std::vector<bit_vector> > hits(1<<bh.wO);

    for(const auto &kv : problem)
    {
        unsigned h=bh(kv.first);
        hits[h].push_back(kv.first);
    }

    std::map<bit_vector,int> clashes;
    for(const auto &h : hits){
        if(h.size() > 1){
            for(const auto &k : h){
                clashes[k]++;
            }
        }
    }

    std::vector<std::pair<bit_vector,int> > res(clashes.begin(), clashes.end());

    std::sort(res.begin(), res.end(),
              [](const std::pair<bit_vector,int> &a, const std::pair<bit_vector,int> &b) { return a.second > b.second; }
    );

    return res;
}

/* Produce a list of (table,lutBit) -> count */
std::vector<std::pair<std::pair<int,int>,int> > find_clashing_entries(const BitHash &bh, const std::vector<std::pair<bit_vector,int> > &keys)
{
    std::map<std::pair<int,int>,int> hits;

    for(const auto &kh : keys){
        for(unsigned ti=0;ti<bh.tables.size();ti++){
            unsigned li=bh.tables[ti].address(kh.first);

            hits[std::make_pair(ti,li)] += kh.second;
        }
    }

    std::vector<std::pair<std::pair<int,int>,int> > res(hits.begin(), hits.end());
    std::sort(res.begin(), res.end(),
              [](const std::pair<std::pair<int,int>,int> &a, const std::pair<std::pair<int,int>,int> &b) { return a.second > b.second; }
    );

    return res;
};

template<class TRng>
BitHash bit_hash_polish(TRng &rng, const BitHash &bh, const key_value_set &problem, int verbose)
{
    std::uniform_real_distribution<> udist;

    if(bh.is_solution(problem))
        return bh;

    unsigned nEntries=0;
    for(const auto &t : bh.tables){
        nEntries+=t.lut.size();
    }

    auto clashKeys=find_clashing_keys(bh, problem);
    if(verbose>0){
        std::cerr<<"  Total clash keys = "<<clashKeys.size()<<"\n";
        if(verbose>1){
            for(auto x : clashKeys){
                std::cerr<<"    "<<x.first<<" : "<<x.second<<"\n";
            }
        }
    }

    auto clashBits=find_clashing_entries(bh, clashKeys);
    if(verbose>0) {
        std::cerr << "  Total clash entries = " << clashBits.size() << "\n";
        if (verbose > 1) {
            for (auto x : clashBits) {
                std::cerr << "    (" << x.first.first << "," << x.first.second << ") : " << x.second << "\n";
            }
        }
    }

    double extra=0.01;
    while(extra<1) {
        double todo=extra*nEntries;

        if(verbose > 0){
            std::cerr<<"   trying with clash = "<< clashBits.size() / (double)nEntries <<" extra = "<<extra<<"\n";
        }

        double done=0.0;

        BitHash res(bh);

        for(unsigned i=0; i< clashBits.size(); i++){
            auto x = clashBits[i];
            res.tables[x.first.first].lut[x.first.second] = -1;
            done++;
            if(done>= todo)
                break;
        }


        for(auto &t: res.tables){
            for(auto &b : t.lut){
                if(udist(rng) < extra){
                    b=-1;
                    done++;
                }
                if(done>=todo)
                    break;
            }
        }

        auto prob = to_cnf(res, problem.keys());
        auto sol = minisat_solve(prob);
        if (verbose > 0) {
            std::cerr << "  CNF solution " << (sol.empty() ? "Failed" : "Succeeded") << "\n";
        }

        if (!sol.empty()) {
            res = substitute(res, prob, sol);

            return res;
        }

        extra=extra*1.5;
    }

    return bh;
}

#endif //FPGA_PERFECT_HASH_BIT_HASH_POLISH_HPP
