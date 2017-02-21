import random

tab4=[ [ random.getrandbits(4) for i in range(64)] for j in range(64) ]
for t in tab4:
    random.shuffle(t)

tab5=[ [ i for i in range(32)]*2 for j in range(64) ]
for t in tab5:
    random.shuffle(t)

def ha_4(p,x):
    assert(x<65536)
    assert(p<65536)

    acc=0

    acc=acc^tab4[0][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    acc=acc^tab4[1][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    acc=acc^tab4[2][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    acc=acc^tab4[3][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    acc=acc^tab4[4][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    acc=acc^tab4[5][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    acc=acc^tab4[6][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    return acc

def ha_5(p,x):
    assert(x<65536)
    assert(p<65536)

    acc=0

    acc=acc^tab5[0][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    acc=acc^tab5[1][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    acc=acc^tab5[2][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    acc=acc^tab5[3][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    acc=acc^tab5[4][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    acc=acc^tab5[5][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    acc=acc^tab5[6][ ((p%8)<<3) | (x%8) ]
    p=p>>3
    x=x>>3

    return acc


def ha_8(p,x):
    assert(x<65536)
    assert(p<65536)

    acc=0

    acc=acc^tabs[0][ ((p%16)<<4) | (x%16) ]
    p=p>>4
    x=x>>4
    acc=acc^tabs[1][ ((p%16)<<4) | (x%16) ]
    p=p>>4
    x=x>>4
    acc=acc^tabs[2][ ((p%16)<<4) | (x%16) ]
    p=p>>4
    x=x>>4
    acc=acc^tabs[3][ ((p%16)<<4) | (x%16) ]
    p=p>>4
    x=x>>4

    return acc

def test_param_hash(h, wParam, wIn, wOut, nKeys):
    keys=[random.getrandbits(wIn) for i in range(nKeys)]
    scores={}
    for p in [i for i in range(65536)]:
        hits=[0]*(2**wOut)
        score=0
        for k in keys:
            hash=h(p,k)
            curr=hits[hash]
            score+=curr
            hits[hash]=curr+1

        if score not in scores:
            scores[score]=1
        else:
            scores[score]+=1

    return scores

for i in range(0,10):
    print( sorted(list(test_param_hash(ha_5, 16, 16, 5, 20).items())) )
