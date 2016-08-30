//
// Created by dt10 on 11/04/2016.
//

#ifndef FPGA_PERFECT_HASH_SOLVER_ANNEAL_HPP
#define FPGA_PERFECT_HASH_SOLVER_ANNEAL_HPP

#include "bit_hash.hpp"
#include "bit_hash_anneal.hpp"

#include "key_value_set.hpp"
#include "weighted_shuffle.hpp"

#include "solve_context.hpp"


std::pair<BitHash,bool> solver_anneal(
        solve_context &ctxt,
        const key_value_set &problem
){
    int &verbose=ctxt.verbose;
    auto &urng=ctxt.urng;
    int wO=ctxt.wO;
    int wI=ctxt.wI;
    int wA=ctxt.wA;
    int groupSize=ctxt.groupSize;

    std::uniform_real_distribution<> udist;

    std::vector<BitHash> solBest(1, makeBitHashConcrete(urng, wO, wI, wA));
    double eBest=evalSolution(solBest[0], problem, groupSize);

    double swapProportion=0.02;
    int tries = 0;

    BitHash solCurr=solBest[0];
    EntryToKey manipCurr(solCurr, problem);

    double startTemperature=8.0;
    double temperature=startTemperature;
    int triesAtLevel=100000;
    double decreaseAtLevel=0.9;

    auto accept=[&](double ePrev, double eCurr)
    {
        if(eCurr < ePrev){
            return 1.0;
        }else if(eCurr == ePrev) {
            return 0.5;
        }else{
            return exp((ePrev-eCurr)/temperature);
        }
    };


    std::vector<int> flips;
    while (eBest!=0 && tries < ctxt.maxTries) {
        double ePrev=manipCurr.eval(groupSize);
        double eCurr=ePrev*10;

        int nFlips=3+(unsigned)ceil(-log2(udist(urng)));

        for(int i=0; i<nFlips; i++) {
            int b=urng()%manipCurr.bitCount();
            manipCurr.flipBit(b);
            flips.push_back(b);

            eCurr=manipCurr.eval(groupSize);
            if(eCurr < ePrev){
                break;
            }
        }

        if(verbose>2){
            std::cerr<<"    Try: "<<tries<<", e = "<<eCurr<<"\n";
        }

        if(eCurr <= eBest) {
            /*if(solBest.size()>100) {
                greedyOneBit(manipCurr, groupSize);
            }
            if(solBest.size()>200) {
                greedyTwoBit(manipCurr, groupSize);
            }*/
            eCurr = manipCurr.eval(groupSize);

            if(eCurr < eBest) {
                if (verbose > 1) {
                    std::cerr << "    Try: " << tries << ", New eBest = " << eBest << "\n";
                }
                solBest.clear();
                solBest.push_back(solCurr);
            }else{
                bool hit=false;
                for(const auto &x : solBest){
                    if(x==solCurr){
                        hit=true;
                        break;
                    }
                }
                if(!hit) {
                    if(solBest.size()>0){
                        solBest.erase(solBest.begin());
                    }
                    solBest.push_back(solCurr);
                }
            }
            eBest = eCurr;


        }

        double probAccept=accept(ePrev,eCurr);

        if(udist(urng) < probAccept) {
            // accept
        }else {
            for(int i=0; i<nFlips; i++) {
                manipCurr.flipBit(flips.back());
                flips.pop_back();
            }
        }

        tries++;

        if(0==(tries%triesAtLevel)){
            temperature=temperature*decreaseAtLevel;
            if(temperature<1e-6){
                startTemperature*=decreaseAtLevel;
                temperature=startTemperature;

                int sel;
                if(udist(urng)<0.9) {
                    //sel=-log(udist(urng)) * 5;
                    sel = sel % solBest.size();
                    solCurr = *(solBest.rbegin()+sel);
                }else{
                    sel=urng()%solBest.size();
                }
                manipCurr.sync();
            }

            if (verbose > 1) {
                std::cerr << "    Try: " << tries << ", eBest = " << eBest << ", nBest = "<<solBest.size()<<", ePrev = " << ePrev <<
                ", temperature = " << temperature << "\n";
            }


            // cpuTime() is really expensive in OS X
            if(cpuTime() > ctxt.maxTime)
                break;
        }
    }

    return std::make_pair(solBest[0], eBest==0);

};

#endif //FPGA_PERFECT_HASH_SOLVER_CNF_HPP
