//
// Created by dt10 on 26/03/2016.
//

#ifndef HLS_PARSER_BIT_HASH_CNF_HPP
#define HLS_PARSER_BIT_HASH_CNF_HPP

#include "bit_hash.hpp"


#include "mtl/IntTypes.h"
#include "core/SolverTypes.h"
#include "core/Solver.h"
#include "utils/System.h"

void printStats(Minisat::Solver& solver)
{
    double cpu_time = Minisat::cpuTime();
    double mem_used = Minisat::memUsedPeak();
    printf("restarts              : %" PRIu64"\n", solver.starts);
    printf("conflicts             : %-12" PRIu64"   (%.0f /sec)\n", solver.conflicts   , solver.conflicts   /cpu_time);
    printf("decisions             : %-12" PRIu64"   (%4.2f %% random) (%.0f /sec)\n", solver.decisions, (float)solver.rnd_decisions*100 / (float)solver.decisions, solver.decisions   /cpu_time);
    printf("propagations          : %-12" PRIu64"   (%.0f /sec)\n", solver.propagations, solver.propagations/cpu_time);
    printf("conflict literals     : %-12" PRIu64"   (%4.2f %% deleted)\n", solver.tot_literals, (solver.max_literals - solver.tot_literals)*100 / (double)solver.max_literals);
    if (mem_used != 0) printf("Memory used           : %.2f MB\n", mem_used);
    printf("CPU time              : %g s\n", cpu_time);
}


struct cnf_problem
{
    // Mapping from (outputBit,lutAddr) to CNF variable
    std::map<std::pair<unsigned,unsigned>, int> lutToVariable;
    // Vector of CNF style clauses
    std::vector<std::vector<int> > clauses;
};

bool is_solution(const cnf_problem &cnf, const std::map<int,int> &solution)
{
    for(const auto &c : cnf.clauses){
        bool solved=false;
        for(auto t : c){
            int v=solution.at(std::abs(t));
            if(v==0 && t<0){
                solved=true;
                break;
            }
            if(v==1 && t>0){
                solved=true;
                break;
            }
        }
        if(!solved) {
            return false;
        }
    }
    return true;
}

BitHash substitute(const BitHash &bh, const cnf_problem &cnf, const std::map<int,int> &solution)
{
    BitHash res(bh);

    for(auto bit : cnf.lutToVariable){
        auto it=solution.find(bit.second);
        if(it!=solution.end()){
            int iO=bit.first.first, addr=bit.first.second;
            res.tables.at(iO).lut.at(addr)=it->second;
        }
    }
    return res;
}

std::map<int,int> minisat_solve(const cnf_problem &problem)
{
    using namespace Minisat;

    Solver S;
    for(const auto & v : problem.lutToVariable){
        while(v.second-1 >= S.nVars() ){
            S.newVar();
        }
    }
    assert(S.nVars() == (int)problem.lutToVariable.size());

    vec<Lit> lits;
    for(const auto &clause : problem.clauses){
        lits.clear();
        for(int raw : clause) {
            int var = std::abs(raw) - 1;
            lits.push((raw > 0) ? mkLit(var) : ~mkLit(var));
        }
        S.addClause(lits);
    }

    //printStats(S);

    if (!S.simplify()) {
        return std::map<int,int>();
    }

    //printStats(S);

    bool ret = S.solve();

    //printStats(S);

    //fprintf(stderr, ret == l_True ? "SATISFIABLE\n" : ret == l_False ? "UNSATISFIABLE\n" : "INDETERMINATE\n");

    if(ret){
        std::map<int,int> res;
        for (int i = 0; i < S.nVars(); i++) {
            if(S.model[i] == l_True){
                res.insert(std::make_pair(i+1,1));
            }else if(S.model[i]== l_False){
                res.insert(std::make_pair(i+1,0));
            }else{

                res.insert(std::make_pair(i+1,-1));
            }
        }
        return res;
    }else {
        return std::map<int,int>();
    }
};

/*
 * If we have the key  0b01x, then we need to make sure that
 *   hash(0b0100) == hash(0b011).
 *
 * One way of forcing it is to say that the version with all x values
 * replaced with 0 is the canonical version. The standard pair-wise key
 * non-equality constraints then operate on the canonical versions.
 * We then add constraints that ensure that all non-canonical variants are
 * the same (form an equivalence class).
 *
 * for all k : equiv(k_0):
 *    hash(k) = hash(k_0)
 *
 *
 */


template<class TKeyCont>
cnf_problem to_cnf(
        const BitHash &bh,
        const TKeyCont &keys
) {
    // Set up a mapping from bits in the table to variables in the CNF output
    std::map<std::pair<unsigned,unsigned>,int > bitMapping;

    auto get_idx=[&](unsigned iO, unsigned iAddr) -> int
    {
        auto key=std::make_pair(iO,iAddr);
        auto val=*bitMapping.insert(std::make_pair(key, bitMapping.size()+1)).first;
        return val.second;
    };

    // Work out the hash values for a given key. The value might be
    // partially known (if some table entries are already fixed), or
    // partially unknown (if some entries still need to be determined).
    //
    // We will return:
    // - false : 0 (same as in CNF)
    // - true : -1 (not present in CNF, and will cause the elimination of a clause)
    // - (iO,addr) : some strictly positive integer that appears in the output.
    auto calcHash=[&](const bit_vector &key) -> std::vector<int> {
        std::vector<int> res;
        res.reserve(bh.wO);
        for(unsigned iO=0; iO<bh.wO; iO++){
            auto addr=bh.tables[iO].address(key);
            auto bit=bh.tables[iO].lut.at(addr);

            if(bit==0){
                res.push_back(0); // false
            }else if(bit==1){
                res.push_back(-1); // true
            }else if(bit==-1){
                res.push_back( get_idx(iO,addr) ); // Some unknown in the CNF
            }else{
                assert(0);
            }
        }
        return res;
    };

    std::vector<std::vector<int> > clauses;

    std::vector<std::vector<int> > hashes;
    hashes.reserve(keys.size());
    for(const auto &k : keys){
        auto it=k.variants_begin();
        auto h0=calcHash(*it);
        hashes.push_back(h0);
        ++it;

        auto end=k.variants_end();
        while(it!=end){
            auto hx=calcHash(*it);
            // Need to assert that h0==hx
            std::vector<int> tmp(2);
            for(unsigned i=0;i<bh.wO;i++){
                tmp[0]=h0[i];
                tmp[1]=-hx[i];
                clauses.push_back(tmp);
                tmp[0]=-h0[i];
                tmp[1]=hx[i];
                clauses.push_back(tmp);
            }

            ++it;
        }
    }

    std::vector<std::vector<int> > acc;
    // Consider all pairs of keys
    for(unsigned iK=0; iK<keys.size()-1; iK++){
        for(unsigned jK=iK+1;jK<keys.size();jK++){
            assert(acc.size()==0);

            acc.push_back(std::vector<int>());

            //std::cerr<<"  "<<iK<<","<<jK<<"\n";

            // Assert that hash[iK] != hash[jK], which means asserting that
            // at least one bit is different.
            auto iH=hashes.at(iK);
            auto jH=hashes.at(jK);

            for(unsigned iO=0; iO<bh.wO; iO++){
                // The two bits we are considering at this level
                int iB=iH[iO], jB=jH[iO];

                if(iB<=0 && jB<=0){
                    // Both bits are completely known
                    if(iB==jB){
                        // The two bits are equal, they are irrelevant to the comparison
                        continue;
                    }else{
                        // The two bits are not equal, so the whole hash is not equal
                        acc.clear(); // Remove all clauses for this pair, as they are provably different
                        break; // Stop looking at other bits
                    }
                    assert(0); // We never fall-through here
                }

                // If a single bit is different, make sure iB=known, jB=unknown
                if(jB<=0){
                    std::swap(iB,jB);
                }

                // If we consider the general form, it is:
                //   acc' = acc | (iB & ~jB) | (~iB & jB)
                // Simplifying that to CNF we get:
                //   acc' = ( acc | iB | !iB) & (acc | iB | jB) & (acc | !jB | !iB ) & (acc | !jB | jB )
                // There are two trivial cases (iB & !iB) plus (jB & !jB), which leaves us with:
                //   acc' = (acc | iB | jB) & (acc | !iB | !jB)

                if(iB<=0){ // If iB has a known value...
                    assert(jB>0); // but jB is still unknown
                    if(iB==0){ // if !iB, then (acc | !iB | !jB) holds
                        // Only (acc | iB | jB) holds, which is now (acc | jB)
                        for(auto &c : acc){
                            c.push_back(jB);
                        }
                    }else{ // if jB, then (acc | iB | jB) holds
                        // And all that remains is (acc | !jB)
                        for(auto &c : acc){
                            c.push_back(-jB);
                        }
                    }
                }else{ // Both iB and jB are uknown
                    assert(iB>0);
                    assert(jB>0);

                    //std::cerr<<"      not equal\n";

                    // We know exactly how many there will be, so may as well reserve
                    int aSize=acc.size();
                    acc.reserve(aSize*2);

                    for(int a=0; a<aSize; a++) {
                        // Create a new copy.
                        acc.emplace_back(acc[a]);
                        // acc' = acc | iB | jB
                        acc[a].push_back(iB);
                        acc[a].push_back(jB);
                        // acc' = acc | !iB | !jB
                        acc[a + aSize].push_back(-iB);
                        acc[a + aSize].push_back(-jB);
                    }
                }
            }

            // Transfer them all over
            std::move(acc.begin(), acc.end(), std::back_inserter(clauses));
            acc.clear();
        }
    }

    return cnf_problem{
            bitMapping,
            clauses
    };
};

#endif //HLS_PARSER_BIT_HASH_CNF_HPP
