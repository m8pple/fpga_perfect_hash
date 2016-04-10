//
// Created by dt10 on 09/04/2016.
//

#ifndef FPGA_PERFECT_HASH_BIT_HASH_HISTORY_HPP
#define FPGA_PERFECT_HASH_BIT_HASH_HISTORY_HPP

/*
 * A should be prime relative to 2^64 (a prime would work well...), so it
 * will go through every value as we calculate
 * x_{i+1}=mod(x_{i}+A,2^64). If x_{i}=0, this means
 * that x_{i+1} = mod(i*A,2^64), so we can jump to it.
 *
 * The default A is next_prime(sqrt(0.6)*2^64)
 */
template<class Type=uint64_t, Type A=14288786517845491783ull,int Skip=1>
class bit_signature
{
private:
    Type m_a;
public:
    /* By default all bits are 0 (not set) */
    bit_signature()
            : m_a(0)
    {}

    void flip(unsigned bit)
    {
        m_a=hash_with_flip(bit);
    }

    std::string str() const
    {
        char buff[9]={0};
        sprintf(buff, "%08x", m_a);
        return buff;
    }

    Type hash() const
    { return m_a; }

    Type hash_with_flip(unsigned bit) const
    {
        uint32_t off=1+bit*Skip;
        off=(off^(off>>1))|1;
        return m_a ^ (off*A);
    }

    bool operator==(const bit_signature &o) const
    { return m_a==o.m_a; }

    bool operator<(const bit_signature &o) const
    {   return m_a<o.m_a; }
};


namespace std {
    template <class Type, Type A, int Skip>
    struct hash<bit_signature<Type, A,Skip> >
    {
        size_t operator()(const bit_signature<Type,A,Skip> & x) const
        { return x.hash(); }
    };
}

template<unsigned TId=0>
class bit_signature_table
{
private:
    static uint32_t lut(unsigned i)
    {
        static std::mt19937 urng(TId);
        static std::vector<uint32_t> table;

        while(i>=table.size()){
            table.push_back(urng());
        }

        return table[i];
    }

    uint32_t m_a;
public:
    /* By default all bits are 0 (not set) */
    bit_signature_table()
            : m_a(0)
    {}

    void flip(unsigned bit)
    {
        m_a=hash_with_flip(bit);
    }

    std::string str() const
    {
        char buff[9]={0};
        sprintf(buff, "%08x", m_a);
        return buff;
    }

    uint32_t hash() const
    { return m_a; }

    uint32_t hash_with_flip(unsigned bit) const
    {
        return m_a ^ lut(bit);
    }

    bool operator==(const bit_signature_table &o) const
    { return m_a==o.m_a; }

    bool operator<(const bit_signature_table &o) const
    {   return m_a<o.m_a; }
};

namespace std {
    template <unsigned TId>
    struct hash<bit_signature_table<TId> >
    {
        size_t operator()(const bit_signature_table<TId> & x) const
        { return x.hash(); }
    };
}


template<class TFirst,class TRest>
class bit_sig_multi
{
public:
    TFirst first;
    TRest rest;

    typedef decltype(first.hash()+rest.hash()) result_type;

    void flip(unsigned bit)
    {
        first.flip(bit);
        rest.flip(bit);
    }

    std::string str() const
    {
        return first.str()+":"+rest.str();
    }

    result_type hash() const
    { return first.hash()+rest.hash(); }

    result_type hash_with_flip(unsigned bit) const
    { return first.hash_with_flip(bit)+rest.hash_with_flip(bit); }

    bool operator==(const bit_sig_multi &o) const
    { return first==o.first && rest==o.rest; }

    bool operator<(const bit_sig_multi &o) const {
        if (first < o.first) return true;
        if (first == o.first) return rest < o.rest;
        return false;
    }
};

template<class TSig>
class bit_sig_multi_set
{
private:
    unsigned m_m, m_log2m; // Size of the table
    unsigned m_shift;
    unsigned m_n; // Number of things in the table
    std::vector<uint8_t> m_table;

    template<class TFirst,class TRest>
    bool containsR(const bit_sig_multi<TFirst,TRest> &sig) const
    { return containsR(sig.first) && containsR(sig.rest); }

    template<class THash>
    bool containsR(const THash &hash) const
    {
        uint32_t h=hash.hash();
        h=((h<<m_shift)^h)>>m_shift;
        return m_table.at( h ) > 0;
    }

    template<class TFirst,class TRest>
    bool containsWithFlipR(const bit_sig_multi<TFirst,TRest> &sig, unsigned flip) const
    { return containsWithFlipL(sig.first, flip) && containsWithFlipR(sig.rest, flip); }

    template<class THash>
    void containsWithFlipL(const THash &hash, unsigned flip) const
    {
        uint32_t h=hash.hash_with_flip(flip);
        h=((h<<m_shift)^h)>>m_shift;
        return m_table[ h ] > 0;
    }

    template<class TFirst,class TRest>
    void addR(const bit_sig_multi<TFirst,TRest> &sig)
    {
        addR(sig.first);
        addR(sig.rest);
    };

    template<class THash>
    void addR(const THash &hash)
    {
        uint32_t h=hash.hash();
        h=((h<<m_shift)^h)>>m_shift;
        assert(m_table[ h ] < 255  );
        ++m_table[ h ];
    }

    template<class TFirst,class TRest>
    void remR(const bit_sig_multi<TFirst,TRest> &sig)
    {
        remR(sig.first);
        remR(sig.rest);
    };

    template<class THash>
    void remR(const THash &hash)
    {
        uint32_t h=hash.hash();
        h=((h<<m_shift)^h)>>m_shift;
        assert(m_table[ h ] > 0);
        --m_table[ h ];
    }


public:
    bit_sig_multi_set(unsigned log2m)
        : m_m(1u<<log2m)
        , m_log2m(log2m)
        , m_shift(32-log2m)
        , m_n(0)
        , m_table(m_m, 0)
    {
    }

    bool contains(const TSig &sig)
    { return containsR(sig); }

    bool contains_with_flip(const TSig &sig, unsigned flip)
    { return containsWithFlipR(sig); }

    void add(const TSig &sig)
    {
        m_n++;
        addR(sig);
    }

    void remove(const TSig &sig)
    {
        m_n--;
        remR(sig);
    }

    double load() const
    {
        double n=0;
        for(auto x : m_table){
            if(x>0)
                n++;
        }
        return n/m_m;
    }

    void dump() const
    {
        for(unsigned i=0;i<m_m;i++){
            fprintf(stderr, "  %u : %u\n", i, m_table[i]);
        }

    }
};


/*
struct packed_bits
{

    size_t hash;
    std::vector<uint64_t> bits;

    template<class TIt>
    packed_bits(TIt begin, TIt end)
        : hash(0)
    {
        // assume random iterators - could probably do iterator traits if I cared...
        unsigned nBits=end-begin;
        unsigned nWords=(nBits+63)/64;
        bits.resize(nWords);

        TIt it=begin;

        for(unsigned i=0; i<nBits; i+=64) {
            unsigned todo=std::min(nBits-i,64);
            uint64_t acc=0;
            for(unsigned j=0;j<todo;j++){
                uint64_t b=*it;

                assert( (b==0) || (b==1) );
                acc=acc+(b<<j);

                unsigned hi=(i+j)*3;
                uint64_t hash_contrib = (hi+1+b)*A; // implicitly modulo 2^64
                hash=hash ^ hash_contrib;

                ++it;
            }
            bits[i]=acc;
        }

        assert(it==end());
    }

    bool operator==(const packed_bits &o) const
    {
        if(hash!=o.hash)
            return false;
        return bits==o.bits;
    }

    bool operator<(const packed_bits &o) const
    {
        if(hash<o.hash) return true;
        if(hash>o.hash) return false;
        return bits<o.bits;
    }

    uint64_t flipped_hash(unsigned bit) const
    {
        uint64_t hLo=(bit*3+1)*A;
        uint64_t hHi=hLo+A;
        return hash ^ hLo ^ hHi;
    }

    void flip(unsigned bit)
    {
        hash=flipped_hash(bit);

        unsigned wIndex=bit/64;
        unsigned wOffset=bit%64;
        bits[wIndex] ^= (1ull<<wOffset);
    }
};

 */

#endif //FPGA_PERFECT_HASH_BIT_HASH_HISTORY_HPP
