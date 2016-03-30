//
// Created by dt10 on 28/03/2016.
//

#ifndef HLS_PARSER_PARSE_KEYS_HPP
#define HLS_PARSER_PARSE_KEYS_HPP

#include "bit_vector.hpp"

#include <string>
#include <sstream>
#include <vector>
#include <set>

class key_value_set
{
public:
    typedef bit_vector key_type;
    typedef bit_vector value_type;
private:
    std::map<key_type,value_type> m_entries;

    std::set<key_type> m_keys;
    unsigned m_wKey;
    unsigned m_wValue;
    bool m_isKeyConcrete;
    unsigned m_nDistinctKeys;

    void setupProperties()
    {
        m_keys.clear();
        m_wKey=0;
        m_wValue=0;
        m_isKeyConcrete=true;
        m_nDistinctKeys=0;

        for(const auto &e : m_entries){
            for(const auto &x : m_keys){
                if(x.overlaps(e.first)){
                    throw std::runtime_error("Two keys in different groups overlap.");
                }
            }
            m_keys.insert(e.first);
            m_wKey=std::max(m_wKey, (unsigned)e.first.size());
            m_isKeyConcrete=m_isKeyConcrete && e.first.is_concrete();
            m_wValue=std::max(m_wValue, (unsigned)e.second.size());
            m_nDistinctKeys+=e.first.variants_count();
        }
    }
public:
    key_value_set()
        : m_wKey(0)
        , m_wValue(0)
        , m_isKeyConcrete(false)
    {}

    key_value_set(const std::map<key_type,value_type> &_entries)
        : m_entries(_entries)
    {
        setupProperties();
    }

    auto begin() const -> decltype(m_entries.begin())
    { return m_entries.begin(); }

    auto end() const -> decltype(m_entries.end())
    { return m_entries.end(); }

    const std::set<key_type> &keys() const
    { return m_keys; }

    unsigned keys_size() const
    { return m_keys.size(); }

    unsigned keys_size_distinct() const
    { return m_nDistinctKeys; }

    unsigned size() const
    { return m_entries.size(); }

    bool has_concrete_keys() const
    { return m_isKeyConcrete; }

    unsigned getKeyWidth() const
    { return m_wKey; }

    unsigned getValueWidth() const
    { return m_wValue; }

    void print(std::ostream &dst, std::string indent="") const
    {
        for(auto e : m_entries){
            dst<<indent<<e.first<<" : "<<e.second<<"\n";
        }
    }
};


template<class TRng>
inline key_value_set uniform_random_key_value_set(TRng &rng, unsigned wO, unsigned wI, unsigned wV, double loadFactor, double probUndefined=0.0)
{
    std::uniform_real_distribution<> udist;

    unsigned target=(1<<wO)*loadFactor;
    if(target > (1ull<<wO))
        throw std::runtime_error("Target number of keys is impossible to hit (not enough output span).");
    if(target > (1ull<<wI))
        throw std::runtime_error("Target number of keys is impossible to hit (not enough input span).");

    unsigned numKeys=0;
    std::map<bit_vector,bit_vector> keys;

    while(numKeys < target){
        auto key=random_bit_vector(rng, wI, probUndefined);

        bool distinct=true;
        for(const auto &kv : keys){
            if(kv.first.overlaps(key)){
                distinct=false;
                break;
            }
        }

        if(distinct) {
            auto value=random_bit_vector(rng, wV);

            keys.insert(std::make_pair(key, value));
            numKeys+=key.variants_count();
        }
    }

    return key_value_set{keys};
}

template<class TRng>
inline key_value_set exponential_random_key_value_set(TRng &rng, unsigned wO, unsigned wI, unsigned wV, double loadFactor, double probUndefined=0.0)
{
    std::uniform_real_distribution<> udist;

    unsigned target=(1<<wO)*loadFactor;
    if(target > (1ull<<wO))
        throw std::runtime_error("Target number of keys is impossible to hit (not enough output span).");
    if(target > (1ull<<wI))
        throw std::runtime_error("Target number of keys is impossible to hit (not enough input span).");

    unsigned numKeys=0;
    std::map<bit_vector,bit_vector> keys;

    while(numKeys < target){
        int w=(rng()%wI)+1;

        auto key=random_bit_vector(rng, w, probUndefined);

        bool distinct=true;
        for(const auto &kv : keys){
            if(kv.first.overlaps(key)){
                distinct=false;
                break;
            }
        }

        if(distinct) {
            auto value=random_bit_vector(rng, wV);

            keys.insert(std::make_pair(key, value));
            numKeys+=key.variants_count();
        }
    }

    return key_value_set{keys};
}

inline key_value_set parse_key_value_set( std::istream &src)
{
    std::map<bit_vector,bit_vector> entries;

    int lineNum=0;
    while(src.good()){
        std::string line;
        std::getline(src, line);
        lineNum++;

        line=trim(line);
        if(line.empty())
            continue;

        try {

            bit_vector key, value;

            auto mid = line.find(':');
            if (mid != std::string::npos) {
                key = parse_bit_vector(line.substr(0, mid));
                value = parse_bit_vector(line.substr(mid + 1));
            } else {
                key = parse_bit_vector(line);
            }

            auto insert=entries.insert(std::make_pair(key,value));
            if(!insert.second)
                throw std::runtime_error("Duplicate key value.");

            entries[key]=value;
        }catch(const std::exception &e) {
            std::stringstream tmp;
            tmp<<"Exception while parsing line "<<lineNum;
            std::throw_with_nested( std::runtime_error(tmp.str()) );
        }
    }

    return key_value_set{entries};
}

#endif //HLS_PARSER_PARSE_KEYS_HPP
