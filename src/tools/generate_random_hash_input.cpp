#include "key_value_set.hpp"

#include <random>
#include <iostream>
#include <set>
#include <fstream>
#include <cstring>

#include <sys/types.h>
#include <unistd.h>

std::mt19937 urng;

void printUsage()
{
    std::cerr<<"generate_random_hash_input\n";
    std::cerr<<"  --verbose level : Set verbosity on stderr.\n";
    std::cerr<<"  --wi bits       : Set the number of input (key) bits.\n";
    std::cerr<<"  --wo bits       : Set the number of output (hash) bits.\n";
    std::cerr<<"  --wv bits       : Set number of value bits (default is zero).\n";
    std::cerr<<"  --load-factor f : Used to scale the key density (i.e. difficulty).\n";
    std::cerr<<"  --prob-undefined p : Probability of any key bit being a don't care (default 0).\n";
    std::cerr<<"  --distribution [uniform|exponential] : Choose the distribution.\n";
    std::cerr<<"  --seed value    : Specify the rng start seed.\n";
    std::cerr<<"  --random-seed   : Try to pick a unique starting seed.\n";
}


int main(int argc, char *argv[])
{
    int verbose=2;
    int wO=6;
    int wI=12;
    int wV=16;
    double loadFactor=0.75;
    double probUndefined=0.0;
    uint32_t seed=0;
    std::string mode="uniform";

    try {
        int ia = 1;
        while (ia < argc) {
            if (!strcmp(argv[ia], "--verbose")) {
                printUsage();
                exit(1);
            }else if (!strcmp(argv[ia], "--verbose")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --verbose");
                verbose = atoi(argv[ia + 1]);
                ia += 2;
            } else if (!strcmp(argv[ia], "--wo")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --wo");
                wO = atoi(argv[ia + 1]);
                if (wO < 1) throw std::runtime_error("Can't have wo < 1");
                if (wO > 16) throw std::runtime_error("wo > 16 is unexpectedly large (edit code if you are sure).");
                ia += 2;
            } else if (!strcmp(argv[ia], "--wv")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --wv");
                wV = atoi(argv[ia + 1]);
                if (wV < 0) throw std::runtime_error("Can't have wv < 0");
                ia += 2;
            } else if (!strcmp(argv[ia], "--wi")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --wi");
                wI = atoi(argv[ia + 1]);
                if (wI < 1) throw std::runtime_error("Can't have wi < 1");
                if (wI > 32) throw std::runtime_error("wo > 32 is unexpectedly large (edit code if you are sure).");
                ia += 2;
            } else if (!strcmp(argv[ia], "--seed")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --seed");
                seed = atoi(argv[ia + 1]);
                ia += 2;
            } else if (!strcmp(argv[ia], "--random-seed")) {
                timespec ts;
                clock_getres(CLOCK_REALTIME, &ts);
                std::seed_seq seq{ (int)ts.tv_sec, (int)ts.tv_nsec, (int)getpid(), (int)getppid() };
                seq.generate(&seed, 1+&seed);
                ia ++;
            } else if (!strcmp(argv[ia], "--distribution")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --distribution");
                mode=argv[ia+1];
                if(mode!="exponential" && mode!="uniform")
                    throw std::runtime_error("Didn't understand distribution "+mode);
                ia += 2;
            } else if (!strcmp(argv[ia], "--load-factor")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --load-factor");
                loadFactor = strtod(argv[ia + 1], 0);
                if (loadFactor <= 0) throw std::runtime_error("Can't have loadFactor <= 0");
                if (loadFactor > 1) throw std::runtime_error("Can't have loadFactor > 1");
                ia += 2;
            } else if (!strcmp(argv[ia], "--prob-undefined")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --prob-undefined");
                probUndefined = strtod(argv[ia + 1], 0);
                if (probUndefined < 0) throw std::runtime_error("Can't have probUndefined < 0");
                if (probUndefined >= 1) throw std::runtime_error("Can't have probUndefined >= 1");
                ia += 2;
            } else {
                printUsage();
                throw std::runtime_error(std::string("Didn't understand argument ") + argv[ia]);
            }
        }

        urng.seed(seed);

        key_value_set problem;

        if (wI == -1)
            throw std::runtime_error("wI must be set for random input.");
        if (wO == -1)
            throw std::runtime_error("wO must be set for random input.");
        if (wV == -1)
            wV = 0;
        if (verbose > 0) {
            std::cerr << "Generating random input with wO=" << wO << ", wI=" << wI << ", wV=" << wV <<
            ", loadFactor=" << loadFactor << ", probUndefined=" << probUndefined << "\n";
        }

        if(mode=="uniform")
            problem = uniform_random_key_value_set(urng, wO, wI, wV, loadFactor, probUndefined);
        else
            problem = exponential_random_key_value_set(urng, wO, wI, wV, loadFactor, probUndefined);

        if (verbose > 1) {
            std::cerr << "nKeys = " << problem.size() << "\n";
            std::cerr << "wKey = " << problem.getKeyWidth() << "\n";
            std::cerr << "wValue = " << problem.getValueWidth() << "\n";
        }

        if (verbose > 0) {
            std::cerr << "Group load factor is 2^wO / nKeyGroups = " << problem.keys_size() << " / " << (1 << wO) <<
            " = " << problem.keys_size() / (double) (1 << wO) << "\n";
            std::cerr << "True load factor is 2^wO / nKeysDistinct = " << problem.keys_size_distinct() << " / " <<
            (1 << wO) << " = " << problem.keys_size_distinct() / (double) (1 << wO) << "\n";
        }

        problem.print(std::cout);
    }catch(std::exception &e){
        std::cerr<<"Caught exception : "<<e.what()<<"\n";
        exit(1);
    }

    return 0;
}
