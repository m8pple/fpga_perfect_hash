//
// Created by dt10 on 31/03/2016.
//

#ifndef FPGA_PERFECT_HASH_BIT_HASH_RELINK_HPP
#define FPGA_PERFECT_HASH_BIT_HASH_RELINK_HPP

#include "solve_context.hpp"

#include <cfloat>

void randomised_greedy(EntryToKey &et, int groupSize)
{
    auto &urng = solve_context::rng();
    std::uniform_real_distribution<> udist;

    for(int i=0;i<et.bitCount();i++){
        if(urng()%2)
            et.flipBit(i);
    }

    double eBest=et.eval(groupSize);
    BitHash bhBest=et.bh;

    int ii=0;
    int noIncrease=0;
    while(eBest!=0){
        std::vector<std::pair<double,int> > moves;
        moves.reserve(et.bitCount());

        int offset=urng()%et.bitCount(); // Avoid always selecting lower bits
        for(int i=0;i<et.bitCount();i++){
            int d=(i+offset)%et.bitCount();
            et.flipBit(d);

            double e=et.eval(groupSize);
            moves.push_back(std::make_pair(e,d));

            et.flipBit(d);
        }

        std::sort(moves.begin(), moves.end());

        double eMin=moves.front().first;
        double eMax=moves.back().first;

        if(eMin<eBest){
            eBest=eMin;
            et.flipBit(moves.front().second);
            bhBest=et.bh;
            et.flipBit(moves.front().second);
            noIncrease=0;
        }else{
            noIncrease++;
        }


        if((eMin >= eBest) && (noIncrease>1000)){
            break;
        }

        double alpha=udist(urng);
        for(int i=moves.size()-1;i>0;i--){
            if(moves[i].first > alpha*(eMax-eMin)){
                moves.erase(moves.begin()+i);
            }
        }

        /*if(noIncrease==0) {
            fprintf(stderr, "  %u, eBest=%f, eMin=%f, eMax=%f, alpha=%f, nMoves=%u, noIncrease=%u'\n", ii, eBest, eMin,
                    eMax, alpha, moves.size(), noIncrease);
        }*/


        int sel=urng()%moves.size();
        et.flipBit(moves[sel].second);

        ++ii;
    }

    et.bh=bhBest;
    et.sync();
}

void relink_path(EntryToKey &et, const BitHash &dst, int groupSize)
{
    auto &urng = solve_context::rng();

    double eTotalBest=et.eval(groupSize);
    BitHash bTotalBest=et.bh;

    while(1){
        std::vector<int> differences=et.getDifferenceIndices(dst);
        //fprintf(stderr, "  distance = %d\n", differences.size());

        if(differences.size()==0)
            break;

        double eBest=DBL_MAX;

        // We may have multiple equivalent solutions
        std::vector<int> flipBest;

        for(unsigned i=0; i<differences.size(); i++){
            int d=differences[i];
            et.flipBit(d);

            double eCurr=et.eval(groupSize);

            if(eCurr < eBest){
                eBest = eCurr;
                flipBest.clear();
                flipBest.push_back(d);
            }else if(eCurr==eBest){
                flipBest.push_back(d);
            }

            if(eBest < eTotalBest){
                eTotalBest=eBest;
                bTotalBest=et.bh;
            }

            et.flipBit(d);
        }



        int sel=urng()%flipBest.size();
        int i=flipBest.at(sel);
        et.flipBit(i);
    }

    et.bh=bTotalBest;
    et.sync();
}


#endif //FPGA_PERFECT_HASH_BIT_HASH_ANNEAL_HPP
