# Based on Practical Perfect Hashing by
# Cormack, Horspool, and Kaiserweth

import random

taps=[random.randbits(32) for i in range(32)]

def msr(x):
     return (16807*x)%2147483647

def h_i(i,k,r):
    hash=0
    while i>0:


    return x % r

class Hash:
    def __init__(self,s):
        self.s=s
        self.H=[(0,0,0)]*s
        self.D=[]

    def lookup(self,k):
        x=h(k,s)
        (p,i,r)=self.H[x]
        if r==0:
            return None
        y=p+h_i(i,k,r)
        (key,data)=self.D[y]
        if key!=k:
            return None
        return data

    def _are_distinct(self,m,ks,r):
        #print("      m={0}, r={1}".format(m,r))
        hit=[False]*r
        for k in ks:
            hash=h_i(m,k,r)
            #print("        hash({0}) = {1}, {2}".format(k,hash,hit[hash]))
            if hit[hash]:
                return False
            hit[hash]=True
        return True

    def _find_free(self,r):
        length=0
        start=len(self.D)
        for y in range(0,len(self.D)):
            if self.D[y] is None:
                if length==0:
                    start=y
                length=length+1
                if length==r:
                    return start
            else:
                length=0
        need=r-length
        self.D.extend( [None]*need )
        return start


    def insert(self,k,d):
        print("insert({0},{1})".format(k,d))

        s=self.s

        x=h_i(1,k,s)
        (p,i,r)=self.H[x]
        print("  (p,i,r)=({0},{1},{2})".format(p,i,r))
        if r==0:
            y=self._find_free(1)
            self.H[x]=(y,0,1)
            self.D[y]=(k,d)
            print("  p={0}".format(y))
        else:
            ks = [ self.D[j][0] for j in range(p,p+r) ]
            ks.append(k)
            for m in range(1,10000000):
                print("    test {0}".format(m))
                if self._are_distinct(m,ks,r+1):
                    break
            assert(self._are_distinct(m,ks,r+1))
            y = self._find_free(r+1)
            self.H[x]=(y,m,r+1)
            for (kj,dj) in [self.D[j] for j in range(p,p+r) ]:
                self.D[ y+h_i(m,kj,r+1) ] = (kj,dj)
            self.D[ y+h_i(m,k,r+1) ] = (k,d)
            self.D[ p:(p+r) ] = [None]*r

h=Hash(16)
for i in range(0,100):
    h.insert( random.getrandbits(32), random.getrandbits(32) )
