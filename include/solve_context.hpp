//
// Created by dt10 on 11/04/2016.
//

#ifndef FPGA_PERFECT_HASH_SOLVE_CONTEXT_HPP
#define FPGA_PERFECT_HASH_SOLVE_CONTEXT_HPP

#include <cstdarg>
#include <fstream>
#include <random>

double cpuTime()
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_utime.tv_sec+ru.ru_utime.tv_usec/1000000.0;
}


/* This is the God Object where random flags and context goes */
struct solve_context
{
    solve_context(const solve_context &) = delete;
    solve_context(solve_context &&) = delete;
    void operator=(const solve_context &) = delete;
    void operator=(solve_context &&) = delete;

    // Sigh, OS X clang doesn't support thread_local. This appears to
    // be fairly portable for now.
    static solve_context * &pSolveContext()
    {
        static __thread solve_context *spSolveContext = 0;
        return spSolveContext;
    }


    solve_context()
    {
        assert(pSolveContext()==0);
        pSolveContext()=this;
    }

    ~solve_context()
    {
        assert(pSolveContext()==this);
        pSolveContext()=0;
    }



    int verbose=1;
    int maxTries=100000;
    double maxTime=300;
    double maxMem=4000;

    std::ostream *pCsvDst=0;
    std::ofstream csvLogFile;
    std::string csvLogPrefix;

    std::mt19937 urng;

    double startTime;
    int tries=0;

    std::string hashName;

    int wO=-1;
    int wI=-1;
    int wA=6;
    int wV=-1;
    int groupSize=1;

    std::string tapSelectMethod;

    void logMsg(int level, const char *fmt, ...)
    {
        if(level>verbose)
            return;

        va_list v;
        va_start(v, fmt);

        vfprintf(stderr, fmt, v);

        va_end(v);
    }

    template<class T>
    void logCsv(std::string type, T value)
    {
        if (pCsvDst) {
            double solveTime = cpuTime() - startTime;
            (*pCsvDst) << csvLogPrefix << ", " << solveTime << ", " << pSolveContext()->tries << "," << type<<" , "<<value<<"\n";
            pCsvDst->flush();
        }
    }

    static std::mt19937 &rng()
    {
        static std::mt19937 srng;
        if(pSolveContext()){
            return pSolveContext()->urng;
        }else{
            return srng;
        }
    }
};


#endif //FPGA_PERFECT_HASH_SOLVE_CONTEXT_HPP
