//
// Created by dt10 on 11/04/2016.
//

#ifndef FPGA_PERFECT_HASH_SOLVER_GRASP_HPP
#define FPGA_PERFECT_HASH_SOLVER_GRASP_HPP

#include "bit_hash.hpp"
#include "bit_hash_anneal.hpp"
#include "bit_hash_relink.hpp"

#include "key_value_set.hpp"
#include "weighted_shuffle.hpp"

#include "solve_context.hpp"


std::pair<BitHash,bool> solver_grasp(
        solve_context &ctxt,
        const key_value_set &problem
){
    int &verbose=ctxt.verbose;
    auto &urng=ctxt.urng;
    int wO=ctxt.wO;
    int wI=ctxt.wI;
    int wA=ctxt.wA;
    int groupSize=ctxt.groupSize;
    int &tries=ctxt.tries;

    std::uniform_real_distribution<> udist;

    BitHash solBest=makeBitHashConcrete(urng, wO, wI, wA);
    double eBest=evalSolution(solBest, problem, groupSize);

    std::vector<std::pair<double,BitHash> > elites;
    elites.push_back(std::make_pair(eBest, solBest));

    BitHash solCurr=solBest;
    EntryToKey manipCurr(solCurr, problem);

    unsigned triesAtLevel=100;

    while (eBest!=0 && tries < ctxt.maxTries) {
        if(verbose>0){
            std::cerr<<"    Try: "<<tries<<", e = "<<eBest<<"\n";
        }

        randomised_greedy(manipCurr, groupSize);
        double eCurr=manipCurr.eval(groupSize);
        fprintf(stderr, "  ePostSearch = %f", eCurr);

        int sel=urng()%elites.size();
        BitHash target=elites[sel].second;

        relink_path(manipCurr, target, groupSize);
        eCurr=manipCurr.eval(groupSize);
        fprintf(stderr, "  ePostLink = %f", eCurr);

        if(eCurr < eBest) {
            eBest=eCurr;
            solBest=manipCurr.bh;
        }

        bool seen=false;
        for(unsigned i=0;i<elites.size();i++){
            if(elites[i].second==manipCurr.bh){
                seen=true;
            }
        }
        if(!seen) {
            elites.push_back(std::make_pair(eCurr, manipCurr.bh));
        }
        if(elites.size()>20){
            std::sort(elites.begin(), elites.end());
            elites.erase(elites.end()-1);
        }

        if(0==(tries%triesAtLevel)){
            if (verbose > 1) {
                std::cerr << "    Try: " << tries << ", eBest = " << eBest << "\n";
            }

            // cpuTime() is really expensive in OS X
            if(cpuTime() > ctxt.maxTime)
                break;
        }
    }

    return std::make_pair(solBest, eBest==0);

};

#endif //FPGA_PERFECT_HASH_SOLVER_CNF_HPP
