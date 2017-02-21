#include "bit_hash.hpp"
#include "bit_hash_cnf.hpp"

#include <random>
#include <iostream>
#include <set>

std::mt19937 urng;

int main() {
    unsigned wO=9, wI=16, wA=6;
    double load=0.75;

    for(int i=0;i<10;i++) {
        auto bh = makeBitHash(urng, wO, wI, wA);

        //bh.bind_random(urng, 0.25);

        bh.print(std::cout);
        std::cout.flush();


        std::map<bit_vector,bit_vector> tmp;
        for(unsigned i=0;i<unsigned(load*(1<<wO));i++){
            tmp.insert(std::make_pair(random_bit_vector(urng, wO), bit_vector{} ));
        }
        key_value_set keys{tmp};

        std::cerr<<"Creating problem...\n";
        cnf_problem prob;
        to_cnf(bh, keys.keys(), prob);
        std::cerr<<"Solving...\n";
        auto sol=minisat_solve(prob);
        if(sol.empty()){
            std::cerr<<"No solution\n";
        }else {

            std::cerr << "Substituting...\n";
            auto back = substitute(bh, prob, sol);
            back.print(std::cout);
            std::cerr<<"checking...\n";
            if(back.is_solution(keys)) {
                std::cerr<<"Fine.\n";
            }else{
                std::cerr<<"FAIL.\n";
                exit(1);
            }

        }

        std::cout.flush();
    }
}
