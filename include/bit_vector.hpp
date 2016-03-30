//
// Created by dt10 on 28/03/2016.
//

#ifndef HLS_PARSER_BIT_VECTOR_HPP
#define HLS_PARSER_BIT_VECTOR_HPP

#include <climits>
#include <vector>
#include <stdexcept>
#include <cassert>
#include <random>
#include <map>

/* A bit-vector is an immutable structure that contains an unbounded
 * number of bits. Each bit can be one, zero, or don't care.
 *
 * There is an ordering on bits, such that narrower vectors come before wider vectors
 * (as judged by the highest (potentially) non-zero bit). However, there is no particular
 * meaning to the ordering of don't care versus fixed bits.
 *
 */
class bit_vector {
private:
    std::vector<int> m_bits;
    unsigned m_minNonZero;
    unsigned m_maxNonZero;

    void setCounts()
    {
        int lastNonZero=-1;
        m_minNonZero=0;
        m_maxNonZero=0;

        for(int i=m_bits.size()-1; i>=0 ; i--){
            int v=m_bits[i];
            if((v<-1) || (v>+1))
                throw std::invalid_argument("Bit is not -1 (X), 0 (False), or +1 (True)");

            if(v==1){
                m_minNonZero++;
            }
            if(v!=0){
                m_maxNonZero++;
                lastNonZero=std::max(lastNonZero, i);
            }
        }
        m_bits.resize(lastNonZero+1);
    }
public:
    bit_vector()
        : m_minNonZero(0)
        , m_maxNonZero(0)
    {}

    template<class TIt>
    bit_vector(TIt begin, TIt end)
        : m_bits(begin, end)
    {
        setCounts();
    }

    bit_vector(const std::vector<int> bits)
        : m_bits(bits)
    {
        setCounts();
    }

    /*! Return number of bits set to one. If the vector is indeterminate (contains
     * X values), it will return the maximum possible count.
    */
    unsigned count() const
    {
        if(m_minNonZero!=m_maxNonZero)
            throw std::logic_error("Vector contains unknowns");
        return m_maxNonZero;
    }

    unsigned max_count() const
    { return m_maxNonZero; }

    unsigned min_count() const
    { return m_minNonZero; }

    /*! This is the smallest number of LSBs needed to contain all non-zero bits */
    unsigned size() const
    { return m_bits.size(); }

    //! A concrete vector contains no unknown bits
    bool is_concrete() const
    { return m_minNonZero==m_maxNonZero; }

    int operator[](unsigned o) const {
        if (o >= m_bits.size())
            return 0;
        return m_bits[o];
    }

    bool operator<(const bit_vector &o) const
    {
        int diff=size() - o.size();
        if(diff>0)
            return false;
        if(diff<0)
            return true;
        for(int i=m_bits.size()-1;i>=0;i--){
            if(m_bits[i] < o.m_bits[i])
                return true;
            if(m_bits[i] > o.m_bits[i])
                return false;
        }
        return false;
    }

    // return true if the variant sets of this vector and o overlap in any way
    bool overlaps(const bit_vector &o) const
    {
        for(unsigned i=0; i<std::max(size(), o.size()); i++){
            int a=(*this)[i], b=o[i];
            if(a!=-1 && b!=-1){
                if(a!=b)
                    return false;
            }
        }
        return true;
    }

    /*! Return a mask identifying all the defined (0 or 1) bits.
     * The w parameter is to make sure it gets MSB 0 bits.
     * */
    bit_vector get_concrete_mask(unsigned w) const
    {
        std::vector<int> res(w);
        for(unsigned i=0;i<w;i++){
            res[i]=(*this)[i]!=-1 ? 1 : 0;
        }
        return bit_vector{res};
    }

    unsigned variants_count() const
    { return 1<<(max_count()-min_count()); }

    struct variant_iterator
    {
    private:
        std::vector<int> m_bits;
        std::vector<int> m_undefined;
        unsigned m_offset;
    public:
        variant_iterator(const std::vector<int> &bits, unsigned offset=0)
            : m_bits(bits)
            , m_offset(offset)
        {
            for(unsigned i=0;i<m_bits.size();i++){
                if(m_bits[i]==-1) {
                    m_bits[i]=(m_offset>>m_undefined.size())&1;
                    m_undefined.push_back(i);
                }
            }
            assert(m_offset<=(1u<<m_undefined.size()));
        }

        bool operator==(const variant_iterator &o) const
        { return m_offset==o.m_offset; }

        bool operator!=(const variant_iterator &o) const
        { return m_offset!=o.m_offset; }

        variant_iterator &operator++()
        {
            assert(m_offset < (1u<<m_undefined.size()));
            ++m_offset;
            for(unsigned i=0;i<m_undefined.size();i++){
                m_bits[m_undefined[i]]=(m_offset>>i)&1;
            }
            return *this;
        }

        bit_vector operator*() const
        {
            return bit_vector(m_bits);
        }
    };

    variant_iterator variants_begin() const
    { return variant_iterator(m_bits, 0); }

    variant_iterator variants_end() const
    { return variant_iterator(m_bits, variants_count()); }
};

bit_vector to_bit_vector(unsigned x)
{
    std::vector<int> bits;
    while(x>0){
        bits.push_back(x&1);
        x=x>>1;
    }
    return bit_vector{bits};
}

unsigned to_unsigned(const bit_vector &x)
{
    unsigned acc=0;
    for(unsigned i=0;i<x.size();i++){
        if(x[i]==-1)
            throw std::logic_error("Value is abstract.");
        acc=acc | (unsigned(x[i])<<i);
    }
    return acc;
}

std::string to_string(const bit_vector &x)
{
    if(x.size()==0){
        return "0b0";
    }else{
        std::string res(x.size()+2,'0');
        res[0]='0';
        res[1]='b';
        for(unsigned i=0;i<x.size();i++){
            res[res.size()-i-1] = (x[i]==1) ? '1' : (x[i]==0 ? '0' : 'u');
        }
        return res;
    }
}


std::ostream &operator<<(std::ostream &dst, const bit_vector &x)
{
    dst<<to_string(x);
    return dst;
}

std::string trim(std::string x)
{
    // Trim all whitespace at beginning and end
    while(x.size()>0 && isspace(x[0]) ){
        x.erase(x.begin());
    }
    while(x.size()>0 && isspace(x.back())){
        x.pop_back();
    }

    // Make whole string lower-case
    for(unsigned i=0;i<x.size();i++){
        x[i]=std::tolower(x[i]);
    }

    return x;
}

template<class TRng>
bit_vector random_bit_vector(TRng &rng, unsigned w, double probUndefined=0.0)
{
    std::uniform_real_distribution<> udist;

    std::vector<int> res;
    res.reserve(w);

    for(unsigned i=0;i<w;i++){
        if(probUndefined!=0 && udist(rng)<probUndefined){
            res.push_back(-1);
        }else{
            res.push_back(rng()%2);
        }
    }
    return bit_vector{res};
}

bit_vector parse_bit_vector(std::string x)
{
    x=trim(x);

    std::vector<int> res;

    if(x.substr(0,2)=="0b"){
        res.resize(x.size()-2);

        for(unsigned i=0;i<res.size();i++){
            int ch=x[x.size()-i-1];
            if(ch=='1'){
                res[i]=1;
            }else if(ch=='0'){
                res[i]=0;
            }else if(ch=='u'){
                res[i]=-1;
            }else{
                throw std::runtime_error(std::string("Unexpected character '")+std::string(1, ch)+"' in binary string while parsing '"+x+"'.");
            }
        }
    }else if(x.substr(0,2)=="0x"){
        res.resize((x.size()-2)*4);
        for(unsigned i=0;i<x.size();i+=4){
            unsigned val=0;
            int ch=x[x.size()-i/4-1];
            if(std::isdigit(ch)){
                val=ch-'0';
            }else if(std::isxdigit(ch)){
                val=ch-'a'+10;
            }else{
                throw std::runtime_error("Unexpected character in hex string.");
            }

            for(int j=0;j<4;j++){
                res[i+j]=val&1;
                val=val>>1;
            }
        }
    }else{
        char *endptr=0;
        unsigned long long val=strtoull (x.c_str(), &endptr, 0);
        if(val==ULLONG_MAX && errno==ERANGE)
            throw std::runtime_error("Value exceeded range of an unsigned long long");

        if(*endptr != 0)
            throw std::runtime_error("Couldn't parse entire string.");

        while(val){
            res.push_back(val&1);
            val=val>>1;
        }
    }

    return bit_vector{res};
}

std::istream &operator>>(std::istream &src, bit_vector &x)
{
    std::string tmp;
    src>>tmp;
    x=parse_bit_vector(tmp);
    return src;
}

#endif //HLS_PARSER_BIT_VECTOR_HPP
