#include "bit_hash.hpp"
#include "bit_hash_polish.hpp"

#include "key_value_set.hpp"

#include <random>
#include <iostream>
#include <set>
#include <fstream>
#include <cstring>

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
    std::string dstFileName="-";
    std::string name="perfect";
    bool writeTest=false;

    std::mt19937 rng;

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
            } else {
                throw std::runtime_error(std::string("Didn't understand argument ") + argv[ia]);
            }
        }

        if (verbose > 0) {
            std::cerr << "Loading input from " << (srcFileName == "-" ? "<stdin>" : srcFileName) << ".\n";
        }

        BitHash solution;
        key_value_set problem;

        if (srcFileName == "-") {
            solution = parse_bit_hash(std::cin);
            problem = parse_key_value_set(std::cin);
        } else {
            std::ifstream srcFile(srcFileName);
            if (!srcFile.is_open())
                throw std::runtime_error("Couldn't open source file " + srcFileName);
            solution = parse_bit_hash(srcFile);
            problem = parse_key_value_set(srcFile);
        }

        if (verbose > 1) {
            std::cerr << "nKeys = " << problem.size() << "\n";
            std::cerr << "wKey = " << problem.getKeyWidth() << "\n";
            std::cerr << "wValue = " << problem.getValueWidth() << "\n";
        }

        solution=bit_hash_polish(rng, solution, problem, verbose);


        std::ofstream dstFile;
        if (dstFileName != "-") {
            if (verbose > 0) {
                std::cerr << "Writing to file '" << dstFileName << "'\n";
            }
            dstFile.open(dstFileName);
            if (!dstFile.is_open())
                throw std::runtime_error(std::string("Couldn't open file '") + dstFileName + "'");
        }
        std::ostream &dst = dstFile.is_open() ? dstFile : std::cout;

        solution.print(dst);
        problem.print(dst);
    }catch(std::exception &e){
        std::cerr<<"Caught exception : ";
        print_exception(e);
        exit(1);
    }

    return 0;
}
