//
// Created by dt10 on 29/03/2016.
//

#ifndef HLS_PARSER_BIT_HASH_VHDL_HPP
#define HLS_PARSER_BIT_HASH_VHDL_HPP

#include "key_value_set.hpp"

void write_vhdl_hash(const BitHash &bh, std::string name, std::string indent, std::ostream &dst)
{
    unsigned wI=bh.wI, wO=bh.wO;

    dst<<indent<<"library ieee;\n";
    dst<<indent<<"use ieee.std_logic_1164.all;\n";
    dst<<indent<<"entity "<<name<<"_hash is \n";
    dst<<indent<<"  port (\n";
    dst<<indent<<"    key : in std_logic_vector("<<(wI-1)<<" downto 0);\n";
    dst<<indent<<"    hash : out std_logic_vector("<<(wO-1)<<" downto 0);\n";
    dst<<indent<<"  );\n";
    dst<<indent<<"end entity "<<name<<"_hash;\n";
    dst<<"\n";

    dst<<indent<<"architecture behavioral of "<<name<<"_hash is\n";
    dst<<indent<<"  -- ROMS\n";
    for(unsigned i=0;i<bh.tables.size();i++){
        const auto &t = bh.tables[i];
        dst<<indent<<"  signal lut_"<<i<<" : std_logic_vector("<<(t.lut.size()-1)<<" downto 0) := \"";
        for(int j=t.lut.size()-1;j>=0;j--){
            dst<<(t.lut[j]==1?'1':'0');
        }
        dst<<"\";\n";
    }
    dst<<indent<<"  -- Addresses and bits\n";
    for(unsigned i=0;i<bh.tables.size();i++){
        const auto &t = bh.tables[i];
        dst<<indent<<"  signal addr_"<<i<<" : std_logic_vector("<<(t.selectors.size()-1)<<" downto 0);\n";
        dst<<indent<<"  signal bit_"<<i<<" : std_logic;\n";
    }
    dst<<indent<<"begin\n";
    dst<<indent<<"  -- Addresses and bits\n";
    for(unsigned i=0;i<bh.tables.size();i++){
        const auto &t = bh.tables[i];
        dst<<indent<<"  addr_"<<i<<" <= '0' ";
        for(unsigned j=0;j<t.selectors.size();j++){
            dst<<"or x("<<t.selectors[j]<<")";
        }
        dst<<";\n";
        dst<<indent<<"  bit_"<<i<<" <= lut_"<<i<<"(addr_"<<i<<");\n";
    }
    dst<<indent<<"  // Final composition\n";
    dst<<indent<<"  hash <= ";
    for(int i=bh.tables.size()-1;i>=0;i--){
        dst<<"bit_"<<i<<"<<"<<i;
        if(i!=0)
            dst<<" & ";
    }
    dst<<";\n";
    dst<<indent<<"end "<<name<<"_hash;\n";
}

void write_vhdl_hit(const BitHash &bh, const key_value_set &keys, std::string name, std::string indent, std::ostream &dst)
{
    unsigned wI=bh.wI, wO=bh.wO;

    // Set up sentinel values which can never be equal to any input key
    std::vector<unsigned> tags(1<<wO, 1<<wI);
    std::vector<unsigned> masks(1<<wO, 0);

    bool hasMask=!keys.has_concrete_keys();
    unsigned wEntry=hasMask ? 2*wI : wI;

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

    dst<<indent<<"library ieee;\n";
    dst<<indent<<"use ieee.std_logic_1164.all;\n";
    dst<<indent<<"entity "<<name<<"_hit is \n";
    dst<<indent<<"  port (\n";
    dst<<indent<<"    key : in std_logic_vector("<<(wI-1)<<" downto 0);\n";
    dst<<indent<<"    hit : out std_logic;\n";
    dst<<indent<<"    hash : out std_logic_vector("<<(wO-1)<<" downto 0);\n";
    dst<<indent<<"  );\n";
    dst<<indent<<"end "<<name<<"_hit;\n\n";

    dst<<indent<<"architecture "<<name<<"_hit is\n";
    dst<<indent<<"  type tag_array_t is array(0 to "<<((1<<wO)-1)<<" of std_logic_vector("<<(wEntry-1)<<" downto 0);\n";
    dst<<indent<<"  signal tags : tag_array_t := (\n";
    for(unsigned i=0;i<tags.size();i++){
        uint64_t val=(uint64_t(masks[i])<<wI) | tags[i];
        dst<<indent<<"    "<<i<<" => \"";
        for(int i=wEntry-1;i>=0;i--){
            dst<<((val>>i)&1);
        }
        dst<<"\"";
        if(i!=tags.size()-1)
            dst<<",";
        dst<<"\n";
    }
    dst<<indent<<"  );\n";
    dst<<indent<<"begin\n";
    dst<<indent<<"  "<<name<<"_hash : theHash port map(key=>key,hash=>hash);\n";
    dst<<indent<<"\n";
    dst<<indent<<"  tag <= tags(to_integer(hash));\n";
    if(keys.has_concrete_keys()) {
        dst << indent << "  hit <= '1' when tag=key else '0';\n";
    }else{
        dst << indent << "  hit <= '1' when (key and mask) = tag else '0' ;\n";
    }
    dst<<indent<<"end "<<name<<"_hit\n";
}
/*
void write_cpp_lookup(const BitHash &bh, const key_value_set &keys, std::string name, std::string indent, std::ostream &dst)
{
    // Fill with (hopefully) poison values
    std::vector<uint64_t> values(1<<bh.wO, 0xFFFFFFFFFFFFFFFFull);

    // Fill in the valid keys
    for(auto kv : keys){
        auto key=*kv.first.variants_begin();
        auto hash=bh(key);
        std::cerr<<"  key="<<key<<" = "<<to_unsigned(key)<<", value="<<kv.second<<" = "<<to_unsigned(kv.second)<<"\n";
        values.at(hash)=to_unsigned(kv.second);
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
*/
#endif //HLS_PARSER_BIT_HASH_CPP_HPP
