//
// Created by dt10 on 29/03/2016.
//

#ifndef HLS_PARSER_BIT_HASH_CPP_HPP
#define HLS_PARSER_BIT_HASH_CPP_HPP

#include "key_value_set.hpp"

void write_cpp_hash(const BitHash &bh, std::string name, std::string indent, std::ostream &dst)
{
    dst<<indent<<"unsigned "<<name<<"_hash(unsigned x){\n";
    dst<<indent<<"  // ROMS\n";
    for(unsigned i=0;i<bh.tables.size();i++){
        const auto &t = bh.tables[i];
        dst<<indent<<"  static const unsigned lut_"<<i<<"["<<t.lut.size()<<"] = {";
        for(unsigned j=0;j<t.lut.size();j++){
            if(j!=0)
                dst<<",";
            dst<<(t.lut[j]==1?'1':'0');
        }
        dst<<"};\n";
    }
    dst<<indent<<"  // Addresses and bits\n";
    for(unsigned i=0;i<bh.tables.size();i++){
        const auto &t = bh.tables[i];
        dst<<indent<<"  unsigned addr_"<<i<<" = 0 ";
        for(unsigned j=0;j<t.selectors.size();j++){
            dst<<"| (((x>>"<<t.selectors[j]<<")&1)<<"<<j<<")";
        }
        dst<<";\n";
        dst<<indent<<"  unsigned bit_"<<i<<" = lut_"<<i<<"[addr_"<<i<<"];\n";
    }
    dst<<indent<<"  // Final composition\n";
    dst<<indent<<"  unsigned result= 0 ";
    for(unsigned i=0;i<bh.tables.size();i++){
        dst<<"| (bit_"<<i<<"<<"<<i<<")";
    }
    dst<<";\n";
    dst<<indent<<"  return result;\n";
    dst<<indent<<"}\n";
}

void write_cpp_hit(const BitHash &bh, const key_value_set &keys, std::string name, std::string indent, std::ostream &dst)
{
    if(bh.wI > 32){
        throw std::runtime_error("This is not tested (and may not work) for very large input bit widths.");
    }

    // Set up sentinel values which can never be equal to any input key
    std::vector<unsigned> tags(1<<bh.wO, 1<<bh.wI);
    std::vector<unsigned> masks(1<<bh.wO, 0);

    // Mark out the valid tags
    for(auto kv : keys){
        auto key=* kv.first.variants_begin();
        auto hash=bh(key);
        std::cerr<<"  key="<<key<<" = "<<to_unsigned(key)<<"\n";
        tags.at(hash)=to_unsigned(key);

        masks.at(hash)=to_unsigned(key.get_concrete_mask(bh.wI));
    }

    dst << indent << "unsigned " << name << "_hash(unsigned x);\n";
    dst<<"\n";
    dst<<indent<<"bool "<<name<<"_hit(unsigned x){\n";
    dst<<indent<<"  static const unsigned tags["<<(1<<bh.wO)<<"] = {\n";
    for(unsigned i=0;i<tags.size();i++){
        dst<<indent<<"    "<<tags[i];
        if(i!=tags.size()-1)
            dst<<",";
        dst<<"\n";
    }
    dst<<indent<<"  };\n";
    if(!keys.has_concrete_keys()){
        dst<<indent<<"  static const unsigned masks["<<(1<<bh.wO)<<"] = {\n";
        for(unsigned i=0;i<masks.size();i++){
            dst<<indent<<"    "<<masks[i];
            if(i!=masks.size()-1)
                dst<<",";
            dst<<"\n";
        }
        dst<<indent<<"  };\n";
    }
    dst<<indent<<"  unsigned hash="<<name<<"_hash(x);\n";
    dst<<indent<<"  unsigned tag=tags[hash];\n";
    if(keys.has_concrete_keys()) {
        dst << indent << "  return tag==x;\n";
    }else{
        dst<<indent<<"  unsigned mask=masks[hash];\n";
        dst << indent << "  return tag==(x&mask);\n";
    }
    dst<<indent<<"}\n";

}

void write_cpp_lookup(const BitHash &bh, const key_value_set &keys, std::string name, std::string indent, std::ostream &dst)
{
    // Fill with (hopefully) poison values
    std::vector<uint64_t> values(1<<bh.wO, 0xFFFFFFFFFFFFFFFFull);

    unsigned wV = keys.getKeyWidth();
    bool minimalHash=false;
    if(wV==0){
        minimalHash=true;
        wV=(unsigned)ceil(log2(keys.size()));
    }

    // Fill in the valid keys
    unsigned pi=0;
    for(auto kv : keys){
        auto key=*kv.first.variants_begin();
        auto hash=bh(key);
        if(minimalHash){
            values.at(hash) = pi;
            pi++;
        }else {
            values.at(hash) = to_unsigned(kv.second);
        }
        std::cerr<<"  key="<<key<<" = "<<to_unsigned(key)<<", value="<<values[hash]<<"\n";

    }

    dst << indent << "unsigned " << name << "_hash(unsigned x);\n";
    dst<<"\n";
    dst<<indent<<"unsigned long long "<<name<<"_lookup(unsigned x){\n";
    dst<<indent<<"  static const unsigned long long values["<<(1<<bh.wO)<<"] = {\n";
    for(unsigned i=0;i<values.size();i++){
        dst<<indent<<"    "<<values[i]<<"ull";
        if(i!=values.size()-1)
            dst<<",";
        dst<<"\n";
    }
    dst<<indent<<"  };\n";
    dst<<indent<<"  unsigned long long value=values["<<name<<"_hash(x)];\n";
    dst<<indent<<"  return value;\n";
    dst<<indent<<"}\n";
}

void write_cpp_test(const BitHash &bh, const key_value_set &keys, std::string name, std::string indent, std::ostream &dst)
{
    std::vector<std::tuple<bit_vector,bit_vector,bit_vector> > values;

    // Fill in the valid keys
    for(auto kv : keys){
        auto it=kv.first.variants_begin();
        auto key=*it;
        auto hash=bh(key);
        auto value=kv.second;

        auto end=kv.first.variants_end();
        while(it!=end){
            values.push_back(std::make_tuple(*it, to_bit_vector(hash),value));
            ++it;
        }
    }

    dst << indent << "unsigned " << name << "_hash(unsigned x);\n";
    dst << indent << "bool " << name << "_hit(unsigned x);\n";
    dst << indent << "unsigned long long" << name << "_lookup(unsigned x);\n";
    dst<<"\n";
    dst<<"#include <stdlib.h>\n";
    dst<<"#include <stdio.h>\n";
    dst<<indent<<"int main(){\n";
    dst<<indent<<"  static const struct { unsigned key; unsigned hash; unsigned long long value; } aPositive ["<<(values.size()+1)<<"] = {\n";
    for(const auto &khv : values){
        dst<<indent<<"    {"<<to_unsigned(std::get<0>(khv))<<", "<<to_unsigned(std::get<1>(khv))<<", "<<to_unsigned(std::get<2>(khv))<<"ull },\n";
        dst<<"\n";
    }
    dst<<indent<<"    { "<<(1u<<bh.wI)<<", 0, 0 }\n";
    dst<<indent<<"  };\n";
    dst<<indent<<"  unsigned curr=0;\n";
    dst<<indent<<"  for(unsigned i=0; i<"<<(1u<<bh.wI)<<"; i++){\n";
    dst<<indent<<"    unsigned gotHash="<<name<<"_hash(i);\n";
    dst<<indent<<"    bool gotHit="<<name<<"_hit(i);\n";
    dst<<indent<<"    unsigned long long gotValue="<<name<<"_lookup(i);\n";
    dst<<indent<<"    if(i==aPositive[curr].key){\n";
    dst<<indent<<"      if(!gotHit) { fprintf(stderr, \"Fail - hit is false for key=%u\",i); exit(1); }\n";
    dst<<indent<<"      if(gotHash!=aPositive[curr].hash) { fprintf(stderr, \"Fail - key %u maps to hash=%u, expected hash=%u\",i,gotHash,aPositive[curr].hash); exit(1); }\n";
    dst<<indent<<"      if(gotValue!=aPositive[curr].value) { fprintf(stderr, \"Fail - key %u results in value=%u, expected hash=%u\",i,gotValue,aPositive[curr].value); exit(1); }\n";
    dst<<indent<<"      curr++;\n";
    dst<<indent<<"    }else{\n";
    dst<<indent<<"      if(gotHit){ fprintf(stderr, \"Fail - hit on key=%u\",i); exit(1); }\n";
    dst<<indent<<"    }\n";
    dst<<indent<<"  }\n";
    dst<<indent<<"  fprintf(stderr, \"Pass\");\n";
    dst<<indent<<"  return true;\n";
    dst<<indent<<"}\n";
}

#endif //HLS_PARSER_BIT_HASH_CPP_HPP
