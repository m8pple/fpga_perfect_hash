#include "bit_hash.hpp"
#include "bit_hash_anneal.hpp"

#include "key_value_set.hpp"
#include "weighted_shuffle.hpp"

#include <random>
#include <iostream>
#include <set>
#include <fstream>
#include <cstring>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>

std::mt19937 urng;


void print_exception(const std::exception& e, int level =  0)
{
    std::cerr << std::string(level, ' ') << "exception: " << e.what() << '\n';
    try {
        std::rethrow_if_nested(e);
    } catch(const std::exception& e) {
        print_exception(e, level+1);
    } catch(...) {}
}

int main(int argc, char *argv[])
{
    int verbose=2;
    int wA=6;
    std::string srcFileName="-";
    int maxTries=INT_MAX;
    int wO=-1;
    int wI=-1;
    int wV=-1;
    double maxTime=600;

    urng.seed(time(0));

    try {

        int ia = 1;
        while (ia < argc) {
            if (!strcmp(argv[ia], "--verbose")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --verbose");
                verbose = atoi(argv[ia + 1]);
                ia += 2;
            } else if (!strcmp(argv[ia], "--input")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --input");
                srcFileName = argv[ia + 1];
                ia += 2;
            } else if (!strcmp(argv[ia], "--wa")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --wa");
                wA = atoi(argv[ia + 1]);
                if (wA < 1) throw std::runtime_error("Can't have wa < 1");
                if (wA > 12) throw std::runtime_error("wa > 12 is unexpectedly large (edit code if you are sure).");
                ia += 2;
            } else if (!strcmp(argv[ia], "--wo")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --wo");
                wO = atoi(argv[ia + 1]);
                if (wO < 1) throw std::runtime_error("Can't have wo < 1");
                if (wO > 16) throw std::runtime_error("wo > 16 is unexpectedly large (edit code if you are sure).");
                ia += 2;
            } else if (!strcmp(argv[ia], "--wi")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --wi");
                wI = atoi(argv[ia + 1]);
                if (wI < 1) throw std::runtime_error("Can't have wi < 1");
                if (wI > 32) throw std::runtime_error("wo > 32 is unexpectedly large (edit code if you are sure).");
                ia += 2;
            } else if (!strcmp(argv[ia], "--max-time")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --max-time");
                maxTime = strtod(argv[ia + 1], 0);
                if (maxTime <= 0) throw std::runtime_error("Can't have maxTime<=0");
                ia += 2;
            } else {
                throw std::runtime_error(std::string("Didn't understand argument ") + argv[ia]);
            }
        }

        if (verbose > 0) {
            std::cerr << "Loading input from " << (srcFileName == "-" ? "<stdin>" : srcFileName) << ".\n";
        }

        key_value_set problem;

        if (srcFileName == "-") {
            problem = parse_key_value_set(std::cin);
        } else {
            std::ifstream srcFile(srcFileName);
            if (!srcFile.is_open())
                throw std::runtime_error("Couldn't open source file " + srcFileName);
            problem = parse_key_value_set(srcFile);
        }

        if (verbose > 1) {
            std::cerr << "Input key value pairs:\n";
            problem.print(std::cerr, "  ");
            std::cerr << "nKeys = " << problem.size() << "\n";
            std::cerr << "wKey = " << problem.getKeyWidth() << "\n";
            std::cerr << "wValue = " << problem.getValueWidth() << "\n";
        }

        if (wI == -1) {
            wI = problem.getKeyWidth();
            if (verbose > 0) {
                std::cerr << "Auto-selecting wI = " << wI << "\n";
            }
        } else {
            if (wI < (int) problem.getKeyWidth()) {
                throw std::runtime_error("Specified key width does not cover all keys.");
            }
        }

        if (wO == -1) {
            unsigned nKeys = problem.keys_size();
            wO = (unsigned) ceil(log(nKeys) / log(2.0));
            if (verbose > 0) {
                std::cerr << "Auto-selecting wO = " << wI << " based on nKeys = " << nKeys << "\n";
            }
        } else {
            if ((1u << wO) < problem.keys_size()) {
                throw std::runtime_error("Specified output width cannot span number of keys.");
            }
        }
        if (verbose > 0) {
            std::cerr << "Group load factor is 2^wO / nKeyGroups = " << problem.keys_size() << " / " << (1 << wO) <<
            " = " << problem.keys_size() / (double) (1 << wO) << "\n";
            std::cerr << "True load factor is 2^wO / nKeysDistinct = " << problem.keys_size_distinct() << " / " <<
            (1 << wO) << " = " << problem.keys_size_distinct() / (double) (1 << wO) << "\n";
        }

        std::uniform_real_distribution<> udist;

        BitHash solution= makeBitHashConcrete(urng, wO, wI, wA);
        double ePrev=evalSolution(solution, problem, 1);

        double swapProportion=0.02;

        bool greedyOne=false, greedyTwo=false, greedyThree=false;
        int tries = 0;
        bool success = false;
        int fails=0;
        while (ePrev!=0 && tries < maxTries && cpuTime() < maxTime) {
            BitHash candidate=perturbHash(urng, solution, swapProportion);

            //candidate=greedyTwoBitFast(candidate, problem);

            double eCandidate=evalSolution(candidate, problem, 1);
/*
            if(eCandidate < 1.1*ePrev || eCandidate-ePrev < 10 ){
                candidate=greedyOneBit(candidate,problem);
                eCandidate=evalSolution(candidate, problem);

            }
*/
            if((verbose > 1) && (0==(tries%1000)) ){
                std::cerr<<"    Try: "<<tries<<", eBest = "<<ePrev<<", eCandidate = "<<eCandidate<<", swapProprtion="<<swapProportion<<"\n";
            }

            if(eCandidate < ePrev || (eCandidate==ePrev && !(candidate==solution) && (udist(urng)<0.5))){
                solution=greedyOneBit(solution,problem, 1);

                std::swap(solution, candidate);
                ePrev=eCandidate;
                if(verbose > 1  ) {
                    std::cerr << "    Try: "<<tries<<", New eBest = " << ePrev << "\n";
                }
                //swapProportion =std::min(0.1, swapProportion*1.1);
                greedyOne=false;
                greedyTwo=false;
                greedyThree=false;
                fails=0;
            }else if(!greedyOne && fails>100 ) {
                candidate=greedyOneBitFast(solution, problem);
                eCandidate=evalSolution(candidate, problem, 1);

                if(eCandidate < ePrev){
                    solution=candidate;
                    ePrev=eCandidate;
                    if(verbose>1){
                        std::cerr << "    greedy: "<<tries<<", New eBest = " << ePrev << "\n";
                    }
                    greedyOne=false;
                    greedyTwo=false;
                    greedyThree=false;
                    fails=0;
                }else{
                    greedyOne=true;
                }
            }else if( (!greedyTwo && fails>1000)) {
                candidate=greedyTwoBitFast(solution, problem);
                eCandidate=evalSolution(candidate, problem, 1);

                if(eCandidate < ePrev){
                    solution=candidate;
                    ePrev=eCandidate;
                    if(verbose>1){
                        std::cerr << "    greedyTwo: "<<tries<<", New eBest = " << ePrev << "\n";
                    }
                    greedyOne=false;
                    greedyTwo=false;
                    greedyThree=false;
                    fails=0;
                }else{
                    greedyTwo=true;
                }
            }else if(!greedyThree && fails > 10000 && problem.keys_size()<256 ) {
                candidate=greedyThreeBitFast(solution, problem, 1);
                eCandidate=evalSolution(candidate, problem, 1);

                if(eCandidate < ePrev){
                    solution=candidate;
                    ePrev=eCandidate;
                    if(verbose>1){
                        std::cerr << "    greedyThree: "<<tries<<", New eBest = " << ePrev << "\n";
                    }
                    greedyOne=false;
                    greedyTwo=false;
                    greedyThree=false;
                    fails=0;
                }else{
                    greedyThree=true;
                }
            }else{
                swapProportion *= 0.999;
                fails++;
            }

            tries++;
        }


        solution.print(std::cout);
        problem.print(std::cout);
    }catch(std::exception &e){
        std::cerr<<"Caught exception : ";
        print_exception(e);
        exit(1);
    }

    return 0;
}
