//
// Created by dt10 on 11/04/2016.
//

#ifndef FPGA_PERFECT_HASH_SOLVER_CNF_HPP
#define FPGA_PERFECT_HASH_SOLVER_CNF_HPP

#include "bit_hash.hpp"
#include "bit_hash_cnf.hpp"
#include "bit_hash_cpp.hpp"

#include "key_value_set.hpp"
#include "weighted_shuffle.hpp"

#include "solve_context.hpp"


std::pair<BitHash,bool> solve_cnf(
        solve_context &ctxt,
        const key_value_set &problem
) {
    int &verbose=ctxt.verbose;
    int &tries=ctxt.tries;
    auto &urng=ctxt.urng;

    if(ctxt.groupSize!=1)
        throw std::runtime_error("Solver_cnf does not support groupSize!=1");

    BitHash result;

    tries=1;
    bool success = false;
    while (ctxt.tries < ctxt.maxTries) {
        if (verbose > 0) {
            std::cerr << "  Attempt " << tries << "\n";
        }
        if (verbose > 0) {
            std::cerr << "  Creating bit hash...\n";
        }
        auto bh = (ctxt.tapSelectMethod == "weighted") ? makeWeightedBitHash(urng, problem, ctxt.wO, ctxt.wI, ctxt.wA) : makeBitHash(urng, ctxt.wO, ctxt.wI, ctxt.wA);
        if (verbose > 0) {
            std::cerr << "  Converting to CNF...\n";
        }

        cnf_problem prob;
        to_cnf(bh, problem.keys(), prob, problem.getMaxHash());
        if (verbose > 0) {
            std::cerr << "  Solving problem with minisat...\n";
        }
        auto sol = minisat_solve(prob, verbose);

        if (sol.empty()) {
            if (verbose > 0) {
                std::cerr << "  No solution\n";
            }
        } else {
            if (verbose > 0) {
                std::cerr << "  Checking raw...\n";
            }

            if (verbose > 0) {
                std::cerr << "  Substituting...\n";
            }
            auto back = substitute(bh, prob, sol);

            if (verbose > 0) {
                std::cerr << "  checking...\n";
            }
            if (!back.is_solution(problem))
                throw std::runtime_error("Failed post substitution check.");

            success = true;
            result = back;
            return std::make_pair(result,true);
        }
        tries++;
    }

    return std::make_pair(result, false);
}


#endif //FPGA_PERFECT_HASH_SOLVER_CNF_HPP
