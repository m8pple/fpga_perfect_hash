Thinking about a better way of doing hit-checks. So far
nothing...

-

We have k[1]..k[n].

We want to assert that hit = 1 iff input is in k[1]..k[n].

Treat:
  H as hit
  K[1,1]..K[n,wI] as keys
  X[1]..x[wI] as keys

Clauses:
H = (
  K[1]==X || K[2]==X || ... || K[n]==X
)
equivalent to:
(
  H & K[1]==X
  |
  H & K[2]==X
  |
  ...
  |
  H & K[n]==X
  |
  !H
)


Choose m 1-bit tables:
  T[1]..T[m]
Plus an m-bit table:
  F

  H = F[ T[1](X)@T[2](X)@...@T[n](X) ]

Dynamic addresses are hard in BSAT. So
let's assume that F is just "and":

  H = T[1](X) & T[2](X) & ... & T[n](X)

(This is now a fancy bloom filter...)

For T[i](X), we'll assume that the index is
a random shuffle on the inputs, which means
that we can identify which bit of each table
is selected for each key. Call that T[i,j] = T[i](K[j])
for convenience, ignoring the shuffle.

Our problem is now:

(
  H
  |
  !H
)

where H = (
  ( T[1,1] & T[2,1] & ... & T[m,1] )
  &
  ( T[1,2] & T[2,2] & ... & T[m,2] )
  ...
  &
  ( T[1,n] & T[2,n] & ... & T[m,n] )
)

Encoding the !H is... difficult. Assume we have
all the non-keys O[1..o], we could say that
T[i,-o] is the bit for those, and end up with:

!H == (
  ( !T[1,-1] | !T[2,-1] | ... | !T[m,-1] )
  &
  ( !T[1,-2] | !T[2,-2] | ... | !T[m,-2] )
  &
  ...
  &
  ( !T[1,-o] | !T[2,-o] | ... | !T[m,-o] )
)


DOH! This doesn't work. The only choice we have
is on the shuffle, which can't be optimised under
BSAT. Lets change things so that F is "or":

  H = T[1](X) | T[2](X) | ... | T[n](X)

NOPE! This is just the inverse. So for "and" we just
end up starting with zeros and adding in all the
ones for keys, and for "or" we start with ones and
take out all the non-keys. BSAT will not be able
to do anything.

-

