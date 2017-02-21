#include "bit_hash.hpp"
#include "bit_hash_history.hpp"

#include <random>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

std::mt19937 urng(time(0));

template<class TSig, class TSig2, class TSrc>
void testSig(TSig sig, TSig2 sig2, TSrc src, unsigned test)
{
    std::unordered_map<TSig,uint64_t> seen;

    for(int i=0;i<test;i++) {
        TSig sig;
        TSig2 sig2;
        uint64_t x=src();

        uint64_t sx=x;
        unsigned si=0;
        while(sx>0){
            int b=sx&1;
            if(b) {
                sig.flip(si);
                sig2.flip(si);
                //fprintf(stderr, "  flip %u : hash=%llu\n", j, sig.hash());
            }
            sx=sx>>1;
            si++;
        }

        if(x!=0 && (sig.hash()==sig2.hash())){
            // This is a bit strict, it could well happen.
            fprintf(stderr, "  FAIL : sig collision, sig1=%u, sig2=%u\n", sig.hash(), sig2.hash());
            exit(1);
        }

        //fprintf(stderr, "%u : hash=%llu\n", i, (unsigned long long)sig.hash());

        auto it=seen.find(sig);
        if(it!=seen.end()){
            if(it->second==x) {
                fprintf(stderr, "  hit\n");
            }else {
                fprintf(stderr, "  %u : collision with %llu, hash=%u\n", i, it->second, sig.hash());

                unsigned hashBits=sizeof(sig.hash())*8;
                double expectedCollisionFree=sqrt(ldexp(1.0,hashBits));
                if(i < 0.1*expectedCollisionFree){
                    fprintf(stderr, "FAIL: got first collision after %u, but expected first collision at %f", i, expectedCollisionFree);
                    exit(1);
                }else{
                    break;
                }
            }
        }

        seen.insert(std::make_pair(sig,x));

        if(seen.find(sig)==seen.end()){
            fprintf(stderr, "FAIL: Hash cannot be used to look up.\n");
            exit(1);
        }


        if(0==(i%1000000)){
            fprintf(stderr, "2^%f, size=2^%f\n", log2(i), log2(seen.size()));
        }
    }
}


int main() {
    unsigned n=32;
    unsigned test=1<<22;

    /*
    typedef bit_signature<uint32_t,3037000507ul,19937> sig_t;
    typedef bit_signature<uint32_t,2716375859ul,31> sig2_t;
    */

    typedef bit_signature_table<0> sig_t;
    typedef bit_signature_table<1> sig2_t;
/*
    unsigned counter=0;
    testSig(sig_t(), sig2_t(), [&](){ return counter++; }, test );

    testSig(sig_t(), sig2_t(), [&](){ counter++; return counter ^ (counter>>1); }, test );

    std::mt19937 urng;
    testSig(sig_t(), sig2_t(), [&](){ return urng(); }, test );
    testSig(sig_t(), sig2_t(), [&](){ return urng(); }, test );
    testSig(sig_t(), sig2_t(), [&](){ return urng(); }, test );
    testSig(sig_t(), sig2_t(), [&](){ return urng(); }, test );
*/

    //typedef bit_sig_multi<sig_t,sig2_t> sigm_t;
    typedef bit_sig_multi<sig_t,bit_sig_multi<sig2_t,bit_signature_table<2> > > sigm_t;
    //typedef bit_signature<uint32_t,3037000507ul,3> sigm_t;
    //typedef bit_signature<uint32_t,2716375859ul,3> sigm_t;
    //typedef bit_signature_table<1> sigm_t;
    typedef bit_sig_multi_set<sigm_t> set_t;

    set_t seen(30);
    std::unordered_set<uint64_t> aseen;

    double nFalse=0;
    for(unsigned i=0; i<(1<<24);i++){
        uint64_t x=urng()+urng();

        sigm_t sig;

        uint64_t sx=x;
        unsigned si=0;
        while(sx){
            if(sx&1){
                sig.flip(si);
            }
            sx=sx>>1;
            si++;
        }

        if(seen.contains(sig) && (aseen.find(x)==aseen.end())){
            fprintf(stderr, "FAIL : Claims to have seen %u : %llu = %s\n", i, x, sig.str().c_str());
            nFalse++;
        }

        seen.add(sig);
        aseen.insert(x);

        if(!seen.contains(sig)){
            fprintf(stderr, "FAIL : Claims not to have seen %llu\n", x);
        }
    }

    fprintf(stderr, "  pFalse = %g\n", nFalse/aseen.size());

    fprintf(stderr, "Done\n");

}
