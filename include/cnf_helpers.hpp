//
// Created by dt10 on 05/04/2016.
//

#ifndef FPGA_PERFECT_HASH_CNF_HELPERS_HPP
#define FPGA_PERFECT_HASH_CNF_HELPERS_HPP

struct cnf_expr_t
{
    typedef Minisat::Lit Lit;

    Minisat::Solver &m_s;
    Lit m_lit;

    // This never escapes into any minisat clauses
    static const int var_False = -3;
    static const int var_True = -4;



    cnf_expr_t(Minisat::Solver &s, Minisat::Lit lit )
            : m_s(s)
            , m_lit(lit)
    { }

    cnf_expr_t(Minisat::Solver &s, bool val )
            : m_s(s)
            , m_lit(Minisat::mkLit(val ? var_True : var_False))
    { }

    bool isTrue() const
    { return m_lit==Minisat::mkLit(var_True); }

    bool isFalse() const
    { return m_lit==Minisat::mkLit(var_False); }

    cnf_expr_t operator&(const cnf_expr_t &o) const
    {
        if(isFalse() || o.isFalse())
            return cnf_expr_t(m_s, false);

        if(isTrue())
            return o;

        if(o.isTrue())
            return *this;

        Lit a(m_lit), b(o.m_lit), c(Minisat::mkLit(m_s.newVar()));

        m_s.addClause(~a,~b,c);
        m_s.addClause(a,~c);
        m_s.addClause(b,~c);

        return cnf_expr_t{m_s,c};
    }

    cnf_expr_t operator~() const
    {
        if(isTrue())
            return cnf_expr_t(m_s, false);
        if(isFalse())
            return cnf_expr_t(m_s, true);
        return cnf_expr_t(m_s, ~m_lit);
    }

    cnf_expr_t operator|(const cnf_expr_t &o) const
    {
        if(isTrue() || o.isTrue())
            return cnf_expr_t{m_s, true};

        if(isFalse())
            return o;

        if(o.isFalse())
            return *this;

        Lit a(m_lit), b(o.m_lit), c(Minisat::mkLit(m_s.newVar()));

        m_s.addClause(a,b,~c);
        m_s.addClause(~a,c);
        m_s.addClause(~b,c);

        return cnf_expr_t{m_s,c};
    }

    void requireTrue()
    {
        if(isTrue()){
            // do nothing
        }else if(isFalse()){
            throw std::runtime_error("Constraint cannot be satisfied.");
        }else {
            m_s.addClause(m_lit);
        }
    }

    void requireFalse()
    {
        if(isFalse()){
            // do nothing
        }else if(isTrue()){
            throw std::runtime_error("Constraint cannot be satisfied.");
        }else {
            m_s.addClause(~m_lit);
        }
    }
};

cnf_expr_t makeLessThanOrEqual(Minisat::Solver &s, const std::vector<Minisat::Lit> &x, const bit_vector &val, int i)
{
    if(i==0){ // LSB
        if(val[0]==1){
            return cnf_expr_t(s,true);
        }else if(val[0]==0){
            return cnf_expr_t(s,~x[0]);
        }else{
            throw std::runtime_error("Bits must be concrete.");
        }
    }else{
        cnf_expr_t next=makeLessThanOrEqual(s, x, val, i-1);

        if(val[i]==1){
            return cnf_expr_t(s,~x[i]) | next;
        }else if(val[i]==0){
            return cnf_expr_t(s,~x[i]) & next;
        }else{
            throw std::runtime_error("Bits must be concrete.");
        }
    }
}

void requireLessThanOrEqual(Minisat::Solver &s, const std::vector<Minisat::Lit> &x, const bit_vector &val)
{
    auto ok=makeLessThanOrEqual(s, x, val, x.size()-1);
    ok.requireTrue();
}

#endif //FPGA_PERFECT_HASH_CNF_HELPERS_HPP
