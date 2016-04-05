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
    dst<<indent<<"use ieee.numeric_std.all\n\n;";

    dst<<indent<<"entity "<<name<<"_hash is \n";
    dst<<indent<<"  port (\n";
    dst<<indent<<"    key : in std_logic_vector("<<(wI-1)<<" downto 0);\n";
    dst<<indent<<"    hash : out std_logic_vector("<<(wO-1)<<" downto 0)\n";
    dst<<indent<<"  );\n";
    dst<<indent<<"end entity "<<name<<"_hash;\n";
    dst<<"\n";

    dst<<indent<<"architecture RTL of "<<name<<"_hash is\n";
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
        dst<<indent<<"  addr_"<<i<<" <= ";
        for(int j=t.selectors.size()-1;j>=0;j--){
            dst<<"key("<<t.selectors[j]<<")";
            if(j!=0)
                dst<<" & ";
        }
        dst<<";\n";
        dst<<indent<<"  bit_"<<i<<" <= lut_"<<i<<"(to_integer(unsigned(addr_"<<i<<")));\n";
    }
    dst<<indent<<"  -- Final composition\n";
    dst<<indent<<"  hash <= ";
    for(int i=bh.tables.size()-1;i>=0;i--){
        dst<<"bit_"<<i;
        if(i!=0)
            dst<<" & ";
    }
    dst<<";\n";
    dst<<indent<<"end RTL;\n";
}

void write_vhdl_hit(const BitHash &bh, const key_value_set &keys, std::string name, std::string indent, std::ostream &dst)
{
    unsigned wI=bh.wI, wO=bh.wO;

    // Set up sentinel values which can never be equal to any input key
    std::vector<unsigned> tags(1<<wO, 1<<wI);
    std::vector<unsigned> masks(1<<wO, 0);

    bool hasMask=!keys.has_concrete_keys();
    unsigned wEntry=1 + (hasMask ? 2*wI : wI);

    // Mark out the valid tags
    for(auto kv : keys){
        auto key=* kv.first.variants_begin();
        auto hash=bh(key);
        std::cerr<<"  key="<<key<<" = "<<to_unsigned(key)<<"\n";
        tags.at(hash)=to_unsigned(key);

        masks.at(hash)=to_unsigned(key.get_concrete_mask(bh.wI));
    }

    dst<<indent<<"library ieee;\n";
    dst<<indent<<"use ieee.std_logic_1164.all;\n";
    dst<<indent<<"use ieee.numeric_std.all\n\n;";

    dst<<indent<<"entity "<<name<<"_hit is \n";
    dst<<indent<<"  port (\n";
    dst<<indent<<"    key : in std_logic_vector("<<(wI-1)<<" downto 0);\n";
    dst<<indent<<"    hit : out std_logic;\n";
    dst<<indent<<"    hash : out std_logic_vector("<<(wO-1)<<" downto 0)\n";
    dst<<indent<<"  );\n";
    dst<<indent<<"end "<<name<<"_hit;\n\n";

    dst<<indent<<"architecture RTL of "<<name<<"_hit is\n";
    dst<<indent<<"  component "<<name<<"_hash \n";
    dst<<indent<<"    port (\n";
    dst<<indent<<"      key : in std_logic_vector("<<(wI-1)<<" downto 0);\n";
    dst<<indent<<"      hash : out std_logic_vector("<<(wO-1)<<" downto 0)\n";
    dst<<indent<<"    );\n";
    dst<<indent<<"  end component;\n";

    dst<<indent<<"  type tag_array_t is array(0 to "<<((1<<wO)-1)<<") of std_logic_vector("<<(wEntry-1)<<" downto 0);\n";
    dst<<indent<<"  signal tags : tag_array_t := (\n";
    for(unsigned i=0;i<tags.size();i++){
        uint64_t val=(uint64_t(masks[i])<<(wI+1)) | tags[i];
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
    dst<<indent<<"  signal gotHash : std_logic_vector("<<(wO-1)<<" downto 0);\n";
    dst<<indent<<"  signal tag : std_logic_vector("<<(wEntry-1)<<" downto 0);\n";
    dst<<indent<<"begin\n";
    dst<<indent<<"   theHash : "<<name<<"_hash port map(key=>key,hash=>gotHash);\n";
    dst<<indent<<"\n";
    dst<<indent<<"  tag <= tags(to_integer(unsigned(gotHash)));\n";
    if(keys.has_concrete_keys()) {
        dst << indent << "  hit <= '1' when tag = (\"0\"&key) else '0';\n";
    }else{
        dst << indent << "  hit <= '1' when (key and mask) = tag else '0' ;\n";
    }
    dst<<indent<<"  hash <= gotHash;\n";
    dst<<indent<<"end RTL;\n";
}

void write_vhdl_lookup(const BitHash &bh, const key_value_set &keys, std::string name, std::string indent, std::ostream &dst)
{
    unsigned wI=bh.wI, wO=bh.wO;

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
    }


    dst<<indent<<"library ieee;\n";
    dst<<indent<<"use ieee.std_logic_1164.all;\n";
    dst<<indent<<"use ieee.numeric_std.all\n\n;";

    dst<<indent<<"entity "<<name<<"_lookup is \n";
    dst<<indent<<"  port (\n";
    dst<<indent<<"    key : in std_logic_vector("<<(wI-1)<<" downto 0);\n";
    dst<<indent<<"    hit : out std_logic;\n";
    dst<<indent<<"    hash : out std_logic_vector("<<(wO-1)<<" downto 0);\n";
    dst<<indent<<"    value : out std_logic_vector("<<(wV-1)<<" downto 0)\n";
    dst<<indent<<"  );\n";
    dst<<indent<<"end "<<name<<"_lookup;\n\n";

    dst<<indent<<"architecture RTL of "<<name<<"_lookup is\n";
    dst<<indent<<"  component "<<name<<"_hit \n";
    dst<<indent<<"    port (\n";
    dst<<indent<<"      key : in std_logic_vector("<<(wI-1)<<" downto 0);\n";
    dst<<indent<<"      hash : out std_logic_vector("<<(wO-1)<<" downto 0);\n";
    dst<<indent<<"      hit : out std_logic\n";
    dst<<indent<<"    );\n";
    dst<<indent<<"  end component;\n";

    dst<<indent<<"  type value_array_t is array(0 to "<<((1<<wO)-1)<<") of std_logic_vector("<<(wV-1)<<" downto 0);\n";
    dst<<indent<<"  signal values : value_array_t := (\n";
    for(unsigned i=0;i<values.size();i++){
        uint64_t val=values[i];
        dst<<indent<<"    "<<i<<" => \"";
        for(int i=wV-1;i>=0;i--){
            dst<<((val>>i)&1);
        }
        dst<<"\"";
        if(i!=values.size()-1)
            dst<<",";
        dst<<"\n";
    }
    dst<<indent<<"  );\n";
    dst<<indent<<"  signal gotHash : std_logic_vector("<<(wO-1)<<" downto 0);\n";
    //dst<<indent<<"  signal val : std_logic_vector("<<(wV-1)<<" downto 0);\n";
    dst<<indent<<"begin\n";
    dst<<indent<<"   theHash : "<<name<<"_hit port map(key=>key,hash=>gotHash,hit=>hit);\n";
    dst<<indent<<"\n";
    dst<<indent<<"  value <= values(to_integer(unsigned(gotHash)));\n";
    dst<<indent<<"  hash <= gotHash;\n";
    dst<<indent<<"end RTL;\n";
}


void write_vhdl_test(const BitHash &bh, const key_value_set &keys, std::string name, std::string indent, std::ostream &dst)
{
    unsigned wI=bh.wI, wO=bh.wO, wV=keys.getKeyWidth();
    unsigned wEntry=wI+wO+wV;

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


    dst<<indent<<"library ieee;\n";
    dst<<indent<<"use ieee.std_logic_1164.all;\n";
    dst<<indent<<"use ieee.numeric_std.all;\n\n";

    dst<<indent<<"entity "<<name<<"_test is \n";
    dst<<indent<<"end "<<name<<"_test;\n\n";

    dst<<indent<<"architecture sim of "<<name<<"_test is\n";
    dst<<indent<<"  signal keyIn : std_logic_vector("<<(wI-1)<<" downto 0);\n";
    dst<<indent<<"  signal expHash, gotHash : std_logic_vector("<<(wO-1)<<" downto 0);\n";
    dst<<indent<<"  signal gotHit, expHit : std_logic;\n";
    dst<<indent<<"  signal gotValue, expValue : std_logic;\n";
    dst<<indent<<"  type test_data_type is array(0 to "<<values.size()<<") of std_logic_vector("<<(wEntry-1)<<" downto 0);\n";
    dst<<indent<<"  signal test_data : test_data_type := (\n";
    unsigned ti=0;
    for(const auto &khv : values){
        auto key = std::get<0>(khv);
        auto hash = std::get<1>(khv);
        auto value = std::get<2>(khv);

        dst<<indent<<"    "<<ti<<" => \"";
        for(int i=wI-1;i>=0;i--){ dst<<key[i]; }
        for(int i=wO-1;i>=0;i--){ dst<<hash[i]; }
        for(int i=wV-1;i>=0;i--){ dst<<value[i]; }
        dst<<"\",\n";
        ti++;
    }
    dst<<indent<<"    "<<ti<<" => \"";
    for(int i=0;i<wEntry;i++){ dst<<"0"; }
    dst<<indent<<"\"  );\n";

    dst<<indent<<"  component "<<name<<"_lookup \n";
    dst<<indent<<"    port (\n";
    dst<<indent<<"      key : in std_logic_vector("<<(wI-1)<<" downto 0);\n";
    dst<<indent<<"      hash : out std_logic_vector("<<(wO-1)<<" downto 0);\n";
    dst<<indent<<"      hit : out std_logic\n";
    dst<<indent<<"    );\n";
    dst<<indent<<"  end component;\n";

    dst<<indent<<"    signal data_key_sig : std_logic_vector("<<(wI-1)<<" downto 0);\n";
    dst<<indent<<"    signal data_hash_sig : std_logic_vector("<<(wO-1)<<" downto 0);\n";
    dst<<indent<<"    signal data_value_sig : std_logic_vector("<<(wV-1)<<" downto 0);\n";


    dst<<indent<<"begin\n";
    dst<<indent<<"  dut : "<<name<<"_hit port map(key=>keyIn,hash=>gotHash,hit=>gotHit);\n";

    dst<<indent<<"  process\n";
    dst<<indent<<"    variable data_entry : std_logic_vector("<<(wEntry-1)<<" downto 0);\n";
    dst<<indent<<"    variable data_key : std_logic_vector("<<(wI-1)<<" downto 0);\n";
    dst<<indent<<"    variable data_hash : std_logic_vector("<<(wO-1)<<" downto 0);\n";
    dst<<indent<<"    variable data_value : std_logic_vector("<<(wV-1)<<" downto 0);\n";
    dst<<indent<<"    variable current_index : integer := 0;\n";
    dst<<indent<<"    variable current_key : integer := 0;\n";
    dst<<indent<<"  begin\n";
    dst<<indent<<"    current_index := 0;\n";
    dst<<indent<<"    current_key := 0;\n";
    dst<<indent<<"    while (current_key < "<<(1<<wI)<<") loop\n";
    dst<<indent<<"      wait for 10 ns;\n";
    dst<<indent<<"      keyIn <= std_logic_vector(to_unsigned(current_key,"<<wI<<"));\n";
    dst<<indent<<"      wait for 10 ns;\n";
    //dst<<indent<<"      assert false report \"key = \"&integer'image(to_integer(unsigned(keyIn)))&\", hash = \"&integer'image(to_integer(unsigned(gothash)))&\", hit = \"&boolean'image(gotHit='1');\n";

    dst<<indent<<"      data_entry := test_data(current_index);\n";
    dst<<indent<<"      data_key := data_entry("<<(wEntry-1)<<" downto "<<(wO+wV)<<");\n";
    dst<<indent<<"      data_key_sig <= data_key;\n";
    dst<<indent<<"      data_hash := data_entry("<<(wO+wV-1)<<" downto "<<(wV)<<");\n";
    dst<<indent<<"      data_hash_sig <= data_hash;\n";
    dst<<indent<<"      data_value := data_entry("<<(wV-1)<<" downto 0);\n";
    //dst<<indent<<R"(      assert false report "dataKey="&integer'image(to_integer(unsigned(data_key)))&", dataHash="&integer'image(to_integer(unsigned(data_hash)));)"<<"\n";

    dst<<indent<<"      if data_key = keyIn then\n";
    dst<<indent<<"          assert gotHit='1' report \"Expected hit.\" severity failure;\n";
    dst<<indent<<"          assert gothash=data_hash report \"Hashes didn't match.\" severity failure;\n";
    dst<<indent<<"          current_index := current_index+1;\n";
    dst<<indent<<"      else\n";
    dst<<indent<<"         assert gotHit='0' report \"Expected no hit.\" severity failure;\n";
    dst<<indent<<"      end if;\n";

    dst<<indent<<"      current_key := current_key + 1;\n";
    dst<<indent<<"    end loop;\n";
    dst<<indent<<"    assert false report \"Testbench succeeded, got \"&integer'image(current_index)&\" hits.\" severity failure;\n";
    dst<<indent<<"  end process;\n";

    /*
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
*/


    dst<<indent<<"end sim;\n\n";
}

#endif //HLS_PARSER_BIT_HASH_CPP_HPP
