#include "bit_hash.hpp"
#include "bit_hash_anneal.hpp"

#include "key_value_set.hpp"
#include "weighted_shuffle.hpp"
#include "bit_hash_history.hpp"

#include <random>
#include <iostream>
#include <set>
#include <fstream>
#include <cstring>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <queue>

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
    int groupSize=1;

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
            } else if (!strcmp(argv[ia], "--wo")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --wo");
                wO = atoi(argv[ia + 1]);
                if (wO < 1) throw std::runtime_error("Can't have wo < 1");
                if (wO > 16) throw std::runtime_error("wo > 16 is unexpectedly large (edit code if you are sure).");
                ia += 2;
            } else if (!strcmp(argv[ia], "--group-size")) {
                if ((argc - ia) < 2) throw std::runtime_error("No argument to --group-size");
                groupSize = atoi(argv[ia + 1]);
                if (groupSize < 1) throw std::runtime_error("Can't have groupSize < 1");
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
            if (wI < (int)problem.getKeyWidth()) {
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
            if ((1u << wO)*groupSize < problem.keys_size()) {
                throw std::runtime_error("Specified output width cannot span number of keys.");
            }
        }
        if (verbose > 0) {
            std::cerr << "Group load factor is nKeyGroups / (2^wO*groupSize) = " << problem.keys_size() << " / " << ((1 << wO)*groupSize) <<
            " = " << problem.keys_size() / (double)(groupSize* (1 << wO)) << "\n";
            std::cerr << "True load factor is nKeysDistinct / (2^wO*groupSize) = " << problem.keys_size_distinct() << " / " <<
                    ((1 << wO)*groupSize) << " = " << problem.keys_size_distinct() / (double) (1 << wO) << "\n";
        }

        std::uniform_real_distribution<> udist;

        std::vector<BitHash> solBest(1, makeBitHashConcrete(urng, wO, wI, wA));
        double eBest=evalSolution(solBest[0], problem, groupSize);

        int tries = 0;
        int triesAtLevel = 10000;

        BitHash solCurr=solBest[0];
        EntryToKey manipCurr(solCurr, problem);

        typedef bit_signature_table<0> sig0_t;
        typedef bit_signature_table<1> sig1_t;
        typedef bit_signature_table<2> sig2_t;
        typedef bit_sig_multi<sig0_t,bit_sig_multi<sig1_t,sig2_t> > sig_t;

        unsigned maxNaboo=1<<16;
        bit_sig_multi_set<sig_t> nabooSet(22);
        std::queue<sig_t> nabooQueue;


        auto isNaboo=[&](const sig_t &base, unsigned flip) -> bool
        {
            return nabooSet.contains_with_flip(base, flip);
        };

        auto addNaboo=[&](const sig_t &base)
        {
            if(nabooQueue.size()>maxNaboo){
                nabooSet.remove(nabooQueue.front());
                nabooQueue.pop();
            }
            nabooSet.add(base);
            nabooQueue.push(base);
        };


        while (eBest!=0 && tries < maxTries) {
            double ePrev=manipCurr.eval(groupSize);

            sig_t sig=toSignature<sig_t>(manipCurr);

            double eBestLocal=ePrev*1e6;
            std::vector<int> flipBestLocal;

            int allowed=0;
            for(int i=0;i<manipCurr.bitCount();i++){
                //auto sigNext(sig);
                //sigNext.flip(i);
                //std::cerr<<"   try "<<sigNext.str()<<", contained = "<<nabooSet.contains(sigNext)<<" \n";
                //if(!nabooSet.contains(sigNext)){
                if(!nabooSet.contains_with_flip(sig,i)){
                    ++allowed;
                    manipCurr.flipBit(i);

                    double eCurr=manipCurr.eval(groupSize);
                    if(eCurr<eBestLocal){
                        flipBestLocal.clear();
                    }
                    if(eCurr<=eBestLocal){
                        eBestLocal=eCurr;
                        flipBestLocal.push_back(i);
                    }

                    manipCurr.flipBit(i);
                }
            }

            //if(verbose>1){
            //    std::cerr<<"   allowed = "<<allowed<<", naboo= "<<nabooQueue.size()<<"\n";
            //}

            if(flipBestLocal.size()==0){
                std::cerr<<"  out of moves...\n";
                exit(1);
            }

            int flip=flipBestLocal[urng()%flipBestLocal.size()];
            manipCurr.flipBit(flip);
            double eCurr=manipCurr.eval(groupSize);
            sig.flip(flip);

            addNaboo(sig);
            //std::cerr<<"  naboo includes : "<<sig.str()<<"\n";


            if(verbose>2){
                std::cerr<<"    Try: "<<tries<<", e = "<<eCurr<<"\n";
            }


            if(eCurr <= eBest) {
                eCurr = manipCurr.eval(groupSize);

                if(eCurr < eBest) {
                    if (verbose > 1) {
                        std::cerr << "    Try: " << tries << ", New eBest = " << eBest << "\n";
                    }
                    solBest.clear();
                    solBest.push_back(solCurr);
                }else{
                    bool hit=false;
                    for(const auto &x : solBest){
                        if(x==solCurr){
                            hit=true;
                            break;
                        }
                    }
                    if(!hit) {
                        if(solBest.size()>0){
                            solBest.erase(solBest.begin());
                        }
                        solBest.push_back(solCurr);
                    }
                }
                eBest = eCurr;
            }

            tries++;

            if(udist(urng)<0.001){
                solCurr=solBest[urng()%solBest.size()];
                manipCurr.sync();
            }

            if(0==(tries%triesAtLevel)){
                if (verbose > 1) {
                    std::cerr << "    Try: " << tries << ", eBest = " << eBest << ", nBest = "<<solBest.size()<<", ePrev = " << ePrev <<", naboo=2^"<<log2(nabooQueue.size())<<"\n";
                }



                // cpuTime() is really expensive in OS X
                if(cpuTime() > maxTime)
                    break;
            }


        }


        solBest[0].print(std::cout);
        problem.print(std::cout);
    }catch(std::exception &e){
        std::cerr<<"Caught exception : ";
        print_exception(e);
        exit(1);
    }

    return 0;
}
