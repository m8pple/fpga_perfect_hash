//
// Created by dt10 on 31/03/2016.
//

#ifndef FPGA_PERFECT_HASH_BIT_HASH_ANNEAL_HPP
#define FPGA_PERFECT_HASH_BIT_HASH_ANNEAL_HPP



/* This decomposes the combination of bit has and keys into four
 * related data structures:
 *
 * - A per-lut-bit entry which maps each bit to:
 *   - A point to the actual bit storage
 *   - The offset of the bit within the output (i.e. a mask)
 *   - A list of keys which make use of this bit
 *
 * - A per-key entry containing the current hash
 *
 * - A per-hash entry containing the current count of keys that map to it
 *
 * - A histogram of the number of hash with each collision cout
 */
struct EntryToKey
{
    BitHash &bh;
    const key_value_set &kvs;

    struct bit_info
    {
        int *pBit;     // Direct pointer into table entries
        unsigned mask; // This is what will be added or removed for each
        std::vector<unsigned> keys; // Offsets of all keys that depend on this

    };
    std::vector<bit_info> bits;
    std::vector<unsigned> keys;
    std::vector<unsigned> hashes;
    std::vector<unsigned> counts;

    EntryToKey(BitHash &_bh, const key_value_set &_kvs)
        : bh(_bh)
        , kvs(_kvs)
    {
        // Build the linear entries, plus a mapping of (ti,li) to bit index
        std::map<std::pair<unsigned,unsigned>, unsigned> pairToBit;

        unsigned ti=0;
        for(auto &t : bh.tables){
            unsigned li=0;
            for(int & i : t.lut){
                pairToBit[std::make_pair(ti, li)] = bits.size();
                bit_info b;
                b.pBit=&i;
                b.mask=1<<ti;
                bits.push_back(b);
                li++;
            }
            ti++;
        }

        hashes.resize(1<<bh.wO); // Maps:  Hash -> NumKeysInHash
        // Work out which entries are affected by each bit

        for(const auto &kv : kvs){
            unsigned ki=keys.size();

            // Loop over each output bit (i.e. lut output)
            for(unsigned ti=0;ti<bh.wO;ti++) {
                // Find the address of the selected bit within the lut.
                unsigned li=bh.tables[ti].address(kv.first); // Implies concrete key

                unsigned bi=pairToBit.at(std::make_pair(ti,li));
                bits.at(bi).keys.push_back(ki);
            }

            unsigned h=bh(kv.first);
            keys.push_back(h);
            hashes.at(h)++;
        }
        //fprintf(stderr, "  nKeys = %u, sumHashes=%u", (unsigned)keys.size(), std::accumulate(hashes.begin(),hashes.end(),0));

        // Work out how many hashes have the same count
        counts.reserve((1<<bh.wO)+1); // Maps:  CountKeys -> NumHashesWithCountKeys
        counts.resize(0); // Start off empty
        unsigned ci=0, sumHashes=0;
        for(auto c : hashes){
            //fprintf(stderr, "  counts[%u] = %u\n", ci, c);

            if(c>=counts.size())
                counts.resize(c+1);
            counts[c]++;
            ci++;

            sumHashes+=c;
        }
        //fprintf(stderr,"  sumHashes=%u\n", sumHashes);
    }

    unsigned bitCount() const
    { return bits.size(); }

    void flipBit(int i)
    {
        const auto &info=bits.at(i);

        // Flip the bit
        *info.pBit = 1 - *info.pBit;

        // Update all the hashes
        for(unsigned ki : info.keys){
            unsigned &key=keys[ki];

            // Remove the old key
            unsigned collisions=hashes[key]--;
            counts[collisions]--;
            if(counts[collisions]==0 && collisions+1==counts.size()){
                counts.resize(collisions);
            }
            counts[collisions-1]++;

            key ^= info.mask; // Flip the bit in the hash

            // Add the new key
            collisions=++hashes[key];
            counts[collisions-1]--;
            if(collisions==counts.size()){
                counts.resize(collisions+1);
            }
            counts[collisions]++;

        }

    }

    double eval() const
    {
        double acc=0;
        for(int i=1;i<counts.size();i++){
            //fprintf(stderr, "  Counts[%u] = %u\n", i, counts[i]);
            acc += counts[i] * (i - 1);
        }
        return acc;
    }

    static double evalFull(const BitHash &bh, const key_value_set &kvs)
    {
        std::vector<unsigned> hits(1<<bh.wO, 0);

        for(const auto &kv : kvs){
            const auto &k=kv.first;

            unsigned h=bh(k);

            ++hits[h];
        }

        double acc=0;
        for(unsigned i=0;i<hits.size();i++){
            if(hits[i]>0) {
                acc += hits[i] - 1;
            }
        }
        return acc;
    }
};



/* This creates a metric whereby a value of zero is a solution, and large values
 * represent progressively worse solutions. The approach is currently driven by three
 * measures:
 * - The maximum number of collisions in one bucket
 * - The total number of number of collisions
 * - The total variants that don't map to a singel value
 * Given a lack of anything better, the metric is just the euclidian norm of the
 * three, as I have no idea which is more important.
 */
double evalSolution(const BitHash &bh, const key_value_set &keys)
{
    /*std::vector<unsigned> hits(1<<keys.getKeyWidth(), 0);

    double m1=0, m2=0, m3=0;

    for(const auto &kv : keys){
        const auto &k=kv.first;

        auto it=k.variants_begin();
        unsigned h=bh(*it);

        unsigned gh=++hits[h];
        if(gh>1){
            m1++;
            m2=std::max(m2, (double)gh);
        }

        ++it;
        auto end=k.variants_end();
        while(it!=end){
            unsigned ho=bh(*it);
            if(ho!=h) {
                m3++;
            }
            ++it;
        }
    }
    return std::sqrt(0.1*m1*m1+0.9*m2*m2+m3*m3);*/

    double ref=EntryToKey::evalFull(bh,keys);

    /*
    BitHash tmp(bh);
    EntryToKey et(tmp,keys);
    double got=et.eval();
    fprintf(stderr, "  got=%f, ref=%f\n", got,ref);
    assert(got==ref);
     */

    return ref;
}

template<class TRng>
BitHash perturbHash(TRng &rng, const BitHash &x, double swapProportion)
{
    std::uniform_real_distribution<> udist;

    BitHash bh(x);

    double totalBits=0;
    for(unsigned i=0;i<bh.tables.size();i++){
        totalBits+=bh.tables[i].lut.size();
    }

    double swapCount = swapProportion * totalBits;
    swapCount = std::max(3.0, swapCount); // Aim to swap at least three bits at minimum, as one or two we can do greedy

    double probSwap=swapCount / totalBits;
    //std::cerr<<"    propSwap = eChange = "<<probSwap * totalBits<<"\n";


    for(unsigned i=0;i<bh.tables.size();i++){
        for(int &b : bh.tables[i].lut) {
            if (b == -1){
                b = rng() % 2;
            }else if(udist(rng) < probSwap){
                b=1-b;
            }
        }
    }

    return bh;
}

BitHash greedyOneBit(const BitHash &bh, const key_value_set &problem)
{
    BitHash best(bh);
    double eBest=evalSolution(bh, problem);

    BitHash follow(bh);
    EntryToKey et(follow,problem);

    BitHash curr(best);
    unsigned linear=0;
    for(unsigned i=0; i<curr.tables.size();i++){
        for(int &b : curr.tables[i].lut){
            b=1-b;

            et.flipBit(linear);

            double eCurr=evalSolution(curr, problem);
            double eFollow=et.eval();

            assert(eCurr==eFollow);

            if(eCurr < eBest){
                best=curr;
                eBest = eCurr;
            }

            et.flipBit(linear);

            b=1-b;
            linear++;
        }
    }

    return best;
}

BitHash greedyOneBitFast(const BitHash &bh, const key_value_set &problem)
{
    BitHash curr(bh);
    EntryToKey et(curr,problem);

    double eBest=et.eval();
    BitHash best(bh);

    unsigned linear=0;
    for(unsigned linear=0; linear<et.bitCount(); linear++){
        et.flipBit(linear);

        double eCurr=et.eval();

        if(eCurr < eBest){
            best=curr;
            eBest = eCurr;
        }

        et.flipBit(linear);
    }

    return best;
}

BitHash greedyTwoBit(const BitHash &bh, const key_value_set &problem)
{
    BitHash best(bh);
    double eBest=evalSolution(bh, problem);

    BitHash curr(best);
    for(unsigned i=0; i<curr.tables.size()-1;i++){
        for(int &bi : curr.tables[i].lut){
            bi=1-bi;
            for(unsigned j=i+1; j<curr.tables.size(); j++) {
                for (int &bj : curr.tables[j].lut) {
                    bj=1-bj;

                    double eCurr = evalSolution(curr, problem);

                    if (eCurr < eBest) {
                        best = curr;
                        eBest = eCurr;
                    }

                    bj=1-bj;
                }
            }

            bi=1-bi;
        }
    }

    return best;
}

BitHash greedyTwoBitFast(const BitHash &bh, const key_value_set &problem)
{
    BitHash curr(bh);
    EntryToKey et(curr,problem);

    double eBest=et.eval();
    BitHash best(bh);

    for(unsigned i=0; i<et.bitCount()-1; i++){
        et.flipBit(i);
        for(unsigned j=i+1; j<et.bitCount()-1; j++){
            et.flipBit(j);

            double eCurr=et.eval();

            if(eCurr < eBest){
                best=curr;
                eBest = eCurr;
            }

            et.flipBit(j);
        }
        et.flipBit(i);
    }

    return best;
}


BitHash greedyThreeBit(const BitHash &bh, const key_value_set &problem)
{
    BitHash best(bh);
    double eBest=evalSolution(bh, problem);

    BitHash curr(best);
    for(unsigned i=0; i<curr.tables.size()-2;i++){
        for(int &bi : curr.tables[i].lut){
            bi=1-bi;
            for(unsigned j=i+1; j<curr.tables.size()-1; j++) {
                for (int &bj : curr.tables[j].lut) {
                    bj=1-bj;
                    for(unsigned k=j+1; j<curr.tables.size(); j++) {
                        for (int &bk : curr.tables[k].lut) {
                            bk = 1 - bk;

                            double eCurr = evalSolution(curr, problem);

                            if (eCurr < eBest) {
                                best = curr;
                                eBest = eCurr;
                            }

                            bk=1-bk;
                        }
                    }

                    bj=1-bj;
                }
            }

            bi=1-bi;
        }
    }

    return best;
}

BitHash greedyThreeBitFast(const BitHash &bh, const key_value_set &problem)
{
    BitHash curr(bh);
    EntryToKey et(curr,problem);

    double eBest=et.eval();
    BitHash best(bh);

    for(unsigned i=0; i<et.bitCount()-2; i++){
        et.flipBit(i);
        for(unsigned j=i+1; j<et.bitCount()-1; j++){
            et.flipBit(j);
            for(unsigned k=j+1; k<et.bitCount(); k++){
                et.flipBit(k);

                double eCurr=et.eval();

                if(eCurr < eBest){
                    best=curr;
                    eBest = eCurr;
                }

                et.flipBit(k);
            }
            et.flipBit(j);
        }
        et.flipBit(i);
    }

    return best;
}


BitHash greedyFourBitFast(const BitHash &bh, const key_value_set &problem)
{
    BitHash curr(bh);
    EntryToKey et(curr,problem);

    double eBest=et.eval();
    BitHash best(bh);

    for(unsigned i=0; i<et.bitCount()-3; i++){
        et.flipBit(i);
        for(unsigned j=i+1; j<et.bitCount()-1; j++){
            et.flipBit(j);
            for(unsigned k=j+1; k<et.bitCount()-1; k++){
                et.flipBit(k);
                for(unsigned m=k+1; m<et.bitCount(); m++){
                    et.flipBit(m);

                    double eCurr=et.eval();

                    if(eCurr < eBest){
                        best=curr;
                        eBest = eCurr;
                    }

                    et.flipBit(m);
                }
                et.flipBit(k);
            }
            et.flipBit(j);
        }
        et.flipBit(i);
    }

    return best;
}


#endif //FPGA_PERFECT_HASH_BIT_HASH_ANNEAL_HPP
