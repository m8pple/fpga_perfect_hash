#include "bit_hash.hpp"
#include "bit_hash_cpp.hpp"

#include "key_value_set.hpp"
#include "weighted_shuffle.hpp"

#include <random>
#include <iostream>
#include <set>
#include <fstream>
#include <cstring>

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
    int maxTries=100000;
    int wO=-1;
    int wI=-1;
    int wV=-1;
    std::string name="perfect";
    std::string writeCpp="";
    std::string method="default";

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

        BitHash solution;

        int tries = 0;
        bool success = false;
        while (tries < maxTries) {
            if (verbose > 0) {
                std::cerr << "  Attempt " << tries << "\n";
            }
            if (verbose > 1) {
                std::cerr << "  Creating bit hash...\n";
            }
            solution= makeBitHashConcrete(urng, wO, wI, wA);

            if (solution.is_solution(problem)){
                success=true;
                break;
            }
            tries++;
        }

        if (!success)
            exit(1);

        solution.print(std::cout);
        problem.print(std::cout);
    }catch(std::exception &e){
        std::cerr<<"Caught exception : ";
        print_exception(e);
        exit(1);
    }

    return 0;
}
