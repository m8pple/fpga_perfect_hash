#include "bit_hash.hpp"
#include "bit_hash_cnf.hpp"
#include "bit_hash_cpp.hpp"

#include "key_value_set.hpp"
#include "weighted_shuffle.hpp"

#include <random>
#include <iostream>
#include <set>
#include <fstream>
#include <signal.h>


// These are global as we may want to use them in signal handlers
static std::ostream *pCsvDst=0;
static std::ofstream csvLogFile;
static std::string csvLogPrefix;

static double startTime;
static int wO=-1, wI=-1, wA=6;
static int tries=0;

double cpuTime()
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_utime.tv_sec+ru.ru_utime.tv_usec/1000000.0;
}

static void signal_outOfTime(int signum)
{
    fprintf(stderr, "Out of time\n");
    if(pCsvDst){
        double solveTime=cpuTime()-startTime;
        (*pCsvDst)<<csvLogPrefix<<", "<<wO<<", "<<wI<<", "<<wA<<", "<<solveTime<<", "<<tries<<", "<<"OutOfTime"<<"\n";
        pCsvDst->flush();
        fflush(0);
        _exit(0);
    }else{
        _exit(1);
    }
}

void setTimeAndSpaceLimit(double maxSeconds, double maxMegaBytes)
{
    // Note: this code is based heavily on code in minisat (Main.cc)

    if (maxSeconds!=0){
        rlimit rl;
        getrlimit(RLIMIT_CPU, &rl);
        if (rl.rlim_max == RLIM_INFINITY || (rlim_t)maxSeconds < rl.rlim_max){
            rl.rlim_cur = (rlim_t)maxSeconds;
            if (setrlimit(RLIMIT_CPU, &rl) == -1)
                fprintf(stderr, "WARNING! Could not set resource limit: CPU-time.\n");
        } }

    // Set limit on virtual memory:
    if (maxMegaBytes!=0){
        rlim_t new_mem_lim = (rlim_t)(maxMegaBytes * 1024*1024);
        rlimit rl;
        getrlimit(RLIMIT_AS, &rl);
        std::cerr<<"  new_mem_lim = "<<new_mem_lim<<", rl.rlim_max = "<<rl.rlim_max<<"\n";
        if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max){
            std::cerr<<"  Setting\n";
            rl.rlim_cur = new_mem_lim;
            if (setrlimit(RLIMIT_AS, &rl) == -1)
                fprintf(stderr, "WARNING! Could not set resource limit: Virtual memory.\n");
        }

#if defined(__APPLE__)
        getrlimit(RLIMIT_DATA, &rl);
        std::cerr<<"  new_mem_lim = "<<new_mem_lim<<", rl.rlim_max = "<<rl.rlim_max<<"\n";
        if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max){
            rl.rlim_cur = new_mem_lim;
            if (setrlimit(RLIMIT_DATA, &rl) == -1)
                fprintf(stderr, "WARNING! Could not set resource limit: Virtual memory.\n");
        }

        Minisat::setMaxMemory(maxMegaBytes);
#endif
    }

    signal(SIGXCPU, signal_outOfTime);
}

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
    std::string srcFileName="-";
    int maxTries=INT_MAX;
    std::string name="perfect";
    //std::string writeCpp="";
    std::string method="default";
    int wV=0;
    unsigned maxHash=0;

    double maxTime=600;
    double maxMem=4000;

    double solveTime=0.0;
    std::string csvLogDst;

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
            } else if (!strcmp(argv[ia], "--csv-log")) {
                if ((argc - ia) < 3) throw std::runtime_error("No argument to --csv-dst");
                csvLogPrefix = argv[ia + 1];
                csvLogDst = argv[ia + 2];
                ia += 3;
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
            } else if (!strcmp(argv[ia], "--max-time")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --max-time");
                maxTime = strtod(argv[ia + 1], 0);
                ia += 2;
            } else if (!strcmp(argv[ia], "--max-mem")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --max-mem");
                maxMem = strtod(argv[ia + 1], 0);
                ia += 2;
            } else if (!strcmp(argv[ia], "--method")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --method");
                method = argv[ia + 1];
                ia += 2;
            } else if (!strcmp(argv[ia], "--max-hash")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --max-hash");
                maxHash = atoi(argv[ia + 1]);
                ia += 2;
            } else if (!strcmp(argv[ia], "--minimal")) {
                if ((argc - ia) < 1) throw std::runtime_error("No argument to --minimal");
                maxHash=UINT_MAX;
                ia += 1;
            /*} else if (!strcmp(argv[ia], "--write-cpp")) {
                if ((argc - ia) < 2) throw std::runtime_error("Not enough arguments to --write-cpp");
                writeCpp = argv[ia + 1];
                ia += 2;*/
            } else {
                throw std::runtime_error(std::string("Didn't understand argument ") + argv[ia]);
            }
        }

        if(!csvLogDst.empty()){
            if(csvLogDst=="-"){
                pCsvDst = &std::cout;
            }else{
                csvLogFile.open(csvLogDst);
                if(!csvLogFile.is_open()){
                    throw std::runtime_error("Couldn't open csv log destination.");
                }
                pCsvDst=&csvLogFile;
            }

        }

        if(verbose>0){
            fprintf(stderr, "Setting limits of %f seconds CPU time and %f MB of memory.\n", maxTime, maxMem);
            setTimeAndSpaceLimit(maxTime, maxMem);
        }

        if (method == "default") {
            if (verbose > 0) {
                method = "minisat_weighted";
                std::cerr << "Selecting default method = " << method << "\n";
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

        if(maxHash==UINT_MAX){
            if(verbose>0){
                std::cerr << " Building minimal hash.\n";
            }
            problem.setMaxHash(maxHash);
        }else if(maxHash>0){
            if(verbose>0){
                std::cerr << " Setting maxHash="<<maxHash<<"\n";
            }
            problem.setMaxHash(maxHash);
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
                std::cerr << "Auto-selecting wO = " << wO << " based on nKeys = " << nKeys << "\n";
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

        BitHash result;

        startTime=cpuTime();

        tries = 1;
        bool success = false;
        while (tries < maxTries) {
            if (verbose > 0) {
                std::cerr << "  Attempt " << tries << "\n";
            }
            if (verbose > 0) {
                std::cerr << "  Creating bit hash...\n";
            }
            auto bh = (method == "minisat_weighted") ? makeWeightedBitHash(urng, problem, wO, wI, wA) : makeBitHash(
                    urng, wO, wI, wA);
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
                break;
            }
            tries++;
        }

        double finishTime=cpuTime();
        solveTime=finishTime-startTime;

        if(pCsvDst){
            (*pCsvDst)<<csvLogPrefix<<", "<<wO<<", "<<wI<<", "<<wA<<", "<<solveTime<<", "<<tries<<", "<<(success?"Success":"OutOfAttempts")<<"\n";
        }

        if (!success) {
            if(pCsvDst) {
                exit(0);
            }else{
                exit(1);
            }
        }
        if(verbose>1){
            for(const auto &kv : problem){
                auto key = *kv.first.variants_begin();

                std::cerr<<"  "<<key<<" -> "<<result(key)<<"\n";
            }
        }

        // Print the two back to back
        result.print(std::cout);
        problem.print(std::cout);

    }catch(Minisat::OutOfMemoryException &e){

        if(pCsvDst){
            (*pCsvDst)<<csvLogPrefix<<", "<<wO<<", "<<wI<<", "<<wA<<", "<<solveTime<<", "<<tries<<", "<<"OutOfMemory"<<"\n";
	    pCsvDst->flush();
        }

        std::cerr<<"Memory limit exceeded.";
        _exit(pCsvDst ? 0 : 1);
    }catch(std::exception &e){
        if(pCsvDst){
            (*pCsvDst)<<csvLogPrefix<<", "<<wO<<", "<<wI<<", "<<wA<<", "<<solveTime<<", "<<tries<<", "<<"Exception"<<"\n";
	    pCsvDst->flush();
        }

        std::cerr<<"Caught exception : ";
        print_exception(e);
	std::cerr.flush();
        _exit(pCsvDst ? 0 : 1);
    }

    return 0;
}
