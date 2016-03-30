#include "shuffle.hpp"

#include <random>
#include <iostream>

std::mt19937 urng;

int main() {
    unsigned wO=6, wI=16, wA=6;

    unsigned best=0;
    for (int i = 0; i < 100000; i++) {
        auto res=makeRandomShuffle(urng, wO, wI, wA);

        //printShuffle(std::cerr, res);
        auto metric=evaluateShuffle(res);
        if(metric>best) {
            std::cerr << metric << "\n";
            best=metric;
        }
    }
}
