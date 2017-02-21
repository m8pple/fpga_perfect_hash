#include "bit_hash.hpp"
#include "bit_hash_cnf.hpp"
#include "bit_hash_cpp.hpp"

#include "key_value_set.hpp"
#include "weighted_shuffle.hpp"

#include "solve_context.hpp"
#include "solver_cnf.hpp"
#include "solver_anneal.hpp"
#include "solver_grasp.hpp"

#include <random>
#include <iostream>
#include <set>
#include <fstream>
#include <signal.h>
#include <cstdarg>



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



static void signal_outOfTime(int signum)
{
    if(solve_context::pSolveContext() && solve_context::pSolveContext()->pCsvDst) {
        solve_context::pSolveContext()->logCsv("Result", "OutOfTime");
    }
    _exit(1);
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




int main(int argc, char *argv[])
{
    solve_context ctxt;


    ctxt.verbose=2;
    std::string srcFileName="-";
    ctxt.maxTries=INT_MAX;
    ctxt.hashName="perfect";

    ctxt.wA=6;
    ctxt.wO=-1;
    ctxt.wI=-1;
    ctxt.wV=-1;

    ctxt.tapSelectMethod="default";
    unsigned maxHash=0;

    double solveTime=0.0;
    std::string csvLogDst;

    std::string method;

    urng.seed(time(0));

    try {
        int ia = 1;
        while (ia < argc) {
            if (!strcmp(argv[ia], "--verbose")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --verbose");
                ctxt.verbose = atoi(argv[ia + 1]);
                ia += 2;
            } else if (!strcmp(argv[ia], "--input")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --input");
                srcFileName = argv[ia + 1];
                ia += 2;
            } else if (!strcmp(argv[ia], "--csv-log")) {
                if ((argc - ia) < 3) throw std::runtime_error("No argument to --csv-dst");
                ctxt.csvLogPrefix = argv[ia + 1];
                csvLogDst = argv[ia + 2];
                ia += 3;
            } else if (!strcmp(argv[ia], "--wa")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --wa");
                ctxt.wA = atoi(argv[ia + 1]);
                if (ctxt.wA < 1) throw std::runtime_error("Can't have wa < 1");
                if (ctxt.wA > 12) throw std::runtime_error("wa > 12 is unexpectedly large (edit code if you are sure).");
                ia += 2;
            } else if (!strcmp(argv[ia], "--wo")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --wo");
                ctxt.wO = atoi(argv[ia + 1]);
                if (ctxt.wO < 1) throw std::runtime_error("Can't have wo < 1");
                if (ctxt.wO > 16) throw std::runtime_error("wo > 16 is unexpectedly large (edit code if you are sure).");
                ia += 2;
            } else if (!strcmp(argv[ia], "--wv")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --wv");
                ctxt.wV = atoi(argv[ia + 1]);
                if (ctxt.wV < 0) throw std::runtime_error("Can't have wv < 0");
                ia += 2;
            } else if (!strcmp(argv[ia], "--wi")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --wi");
                ctxt.wI = atoi(argv[ia + 1]);
                if (ctxt.wI < 1) throw std::runtime_error("Can't have wi < 1");
                if (ctxt.wI > 32) throw std::runtime_error("wo > 32 is unexpectedly large (edit code if you are sure).");
                ia += 2;
            } else if (!strcmp(argv[ia], "--max-time")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --max-time");
                ctxt.maxTime = strtod(argv[ia + 1], 0);
                ia += 2;
            } else if (!strcmp(argv[ia], "--max-mem")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --max-mem");
                ctxt.maxMem = strtod(argv[ia + 1], 0);
                ia += 2;
            } else if (!strcmp(argv[ia], "--method")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --method");
                method = argv[ia + 1];
                ia += 2;
            } else if (!strcmp(argv[ia], "--tap-select-method")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --tap-select-method");
                ctxt.tapSelectMethod = argv[ia + 1];
                ia += 2;
            } else if (!strcmp(argv[ia], "--max-hash")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --max-hash");
                maxHash = atoi(argv[ia + 1]);
                ia += 2;
            } else if (!strcmp(argv[ia], "--minimal")) {
                if ((argc - ia) < 1) throw std::runtime_error("No argument to --minimal");
                maxHash = UINT_MAX;
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
                ctxt.pCsvDst = &std::cout;
            }else{
                ctxt.csvLogFile.open(csvLogDst);
                if(!ctxt.csvLogFile.is_open()){
                    throw std::runtime_error("Couldn't open csv log destination.");
                }
                ctxt.pCsvDst=&ctxt.csvLogFile;
            }

        }

        ctxt.logMsg(1, "Setting limits of %f seconds CPU time and %f MB of memory.\n", ctxt.maxTime, ctxt.maxMem);
        setTimeAndSpaceLimit(ctxt.maxTime, ctxt.maxMem);

        if (ctxt.tapSelectMethod == "default") {
            ctxt.tapSelectMethod = "minisat_weighted";
            ctxt.logMsg(1, "Selecting default tap method = %s", ctxt.tapSelectMethod.c_str());
        }


        ctxt.logMsg(1, "Loading input from %s.\n", (srcFileName == "-" ? "<stdin>" : srcFileName.c_str()));

        key_value_set problem;

        if (srcFileName == "-") {
            problem = parse_key_value_set(std::cin);
        } else {
            std::ifstream srcFile(srcFileName);
            if (!srcFile.is_open())
                throw std::runtime_error("Couldn't open source file " + srcFileName);
            problem = parse_key_value_set(srcFile);
        }

        if (ctxt.verbose > 2) {
            std::cerr << "Input key value pairs:\n";
            problem.print(std::cerr, "  ");
        }
        if(ctxt.verbose>1){
            std::cerr << "nKeys = " << problem.size() << "\n";
            std::cerr << "wKey = " << problem.getKeyWidth() << "\n";
            std::cerr << "wValue = " << problem.getValueWidth() << "\n";
        }

        if(maxHash==UINT_MAX){
            ctxt.logMsg(1," Building minimal hash.\n");
            problem.setMaxHash(maxHash);
        }else if(maxHash>0){
            ctxt.logMsg(1," Setting maxHash=%u\n", maxHash);
            problem.setMaxHash(maxHash);
        }

        if (ctxt.wI == -1) {
            ctxt.wI = problem.getKeyWidth();
            ctxt.logMsg(1, "Auto-selecting wI = %u\n", ctxt.wI);
        } else {
            if (ctxt.wI < (int) problem.getKeyWidth()) {
                throw std::runtime_error("Specified key width does not cover all keys.");
            }
        }

        if (ctxt.wO == -1) {
            unsigned nKeys = problem.keys_size();
            ctxt.wO = (unsigned) ceil(log(nKeys) / log(2.0));
            ctxt.logMsg(1, "Auto-selecting wO = %u  based on nKeys = %u\n", ctxt.wO, nKeys);
        } else {
            if ((1u << ctxt.wO) < problem.keys_size()) {
                throw std::runtime_error("Specified output width cannot span number of keys.");
            }
        }
        if (ctxt.verbose > 0) {
            std::cerr << "Group load factor is 2^wO / nKeyGroups = " << problem.keys_size() << " / " << (1 << ctxt.wO) <<
            " = " << problem.keys_size() / (double) (1 << ctxt.wO) << "\n";
            std::cerr << "True load factor is 2^wO / nKeysDistinct = " << problem.keys_size_distinct() << " / " <<
            (1 << ctxt.wO) << " = " << problem.keys_size_distinct() / (double) (1 << ctxt.wO) << "\n";
        }

        BitHash result;
        bool success;

        ctxt.startTime=cpuTime();

        if(method=="minisat") {
            std::tie(result, success) = solve_cnf(ctxt, problem);
        }else if(method=="anneal") {
            std::tie(result, success) = solver_anneal(ctxt, problem);
        }else if(method=="grasp") {
            std::tie(result, success) = solver_grasp(ctxt, problem);
        }else{
            throw std::runtime_error("Didn't understand method '"+method+"'");
        }

        double finishTime=cpuTime();
        solveTime=finishTime-ctxt.startTime;

        ctxt.logCsv("Result", success?"Success":"OutOfAttempts");

        ctxt.logMsg(0, success?"Success":"OutOfAttempts");

        if (!success) {
            exit(1);
        }

        if(ctxt.verbose>2){
            for(const auto &kv : problem){
                auto key = *kv.first.variants_begin();

                std::cerr<<"  "<<key<<" -> "<<result(key)<<"\n";
            }
        }
        // Print the two back to back
        result.print(std::cout);
        problem.print(std::cout);

    }catch(Minisat::OutOfMemoryException &e){

        ctxt.logCsv("Result", "OutOfMemory");
        ctxt.logMsg(0, "Out of Memory");

        _exit(2);
    }catch(std::exception &e){
        ctxt.logCsv("Result", "Exception");

        std::cerr<<"Caught exception : ";
        print_exception(e);
	    std::cerr.flush();
        _exit(3);
    }

    return 0;
}
