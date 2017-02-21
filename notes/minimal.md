Let n be the number of keys, and wO be the number of output bits.

If n==2^wO, we are automatically minimal.

Assume n+1==2^wO. There is one key combination that cannot be allowed. We can
nuke it by saying:

    H[K1]!=11111111
    H[K2]!=11111111
    ...
    H[K3]!=11111111

For an individual key, that means:

    H[K1,1]!=1 | H[K1,2]!=1 | ... | H[K1,wO]!=1

which is:

    !H[K1,1] | !H[K1,2] | ... | !H[K1,wO]

So it will result in n new clauses, each of which have wO terms.

Scaling up doesn't look pretty. If we say that n+2==2^wO, then we
have to do:

    H[K1]!=11111111
    H[K1]!=11111110
    H[K2]!=11111111
    H[K2]!=11111110
    ...
    H[K3]!=11111111
    H[K3]!=11111110

So for an individual key:

    !H[K1,1] | !H[K1,2] | ... | !H[K1,wO]
    !H[K1,1] | !H[K1,2] | ... |  H[K1,wO]

Following this pattern will require (2^wO-n)*n clauses, each with wO terms.
In the worst case, where n=2^(wO-1)+1 ~= 2^(wO-1), we are going to need
a total of (2^(wO-1))^2 = 2^(2*wO-2) new terms!

(Pretty sure this is going to cover a bunch of really basic BSAT
stuff, but I'm too lazy to read a book.)

If we try re-expressing as:

    H[K1] <= ns, where ns=-1

then we get the function:

    Ok(K1,i)  = !H[K1,i] | Ok(K1,i+1),   if ns[i]
              = !H[K1,i] & Ok(K1,i+1),   if !ns[i]

    Ok(K1,wN) = 1,          if ns[wN]
              = !H[K1,wN],  if !ns[wN]

Pluggin in n=2^wO -> ns=n-1=111...111 we would get:

    Ok(K1,i)  = !H[K1,i] | Ok(K1,i+1)
    Ok(K1,wN) = 1
    ->
    satisfied

Pluggin in n+1=2^w -> ns=n-1=111...1110 we get:

    Ok(K1,i)  = !H[K1,i] | Ok(K1,i+1)
    Ok(K1,wN) = !H[K1,wN]
    ->
    Ok = !H[K1,1] | !H[K1,2] | ... | !H[K1,wN]

Going one more for n+2=2^w -> ns=n-1=111...1101 we get:

       Ok(K1,i)    = !H[K1,i] | Ok(K1,i+1)
       Ok(K1,wN-1) = !H[K1,wN-1] & Ok(K1,wN)
       Ok(K1,wN)   = 1
       ->
       Ok = !H[K1,1] | !H[K1,2] | ... | !H[K1,wN-1]

So this is actually reducing down to "The first wN-1 bits must not be all ones".

Taking a more useful version of minimal (weak-minimal?), we might require that the size
is less than 2^(wO)*3/4, e.g. to use 192 rather than 256 (e.g.  in order
to fit in 3 LUT-RAMs rather than 4.).

So if we say n=3/4*2^wO=1100...000 -> ns=10111..1111 we get:

    Ok(1) = !H[1] | Ok(2)
    Ok(2) = !H[2] & Ok(3)
    Ok(i) = !H[i] | Ok(i+1)
    Ok(wN)= 1
    ->
    Ok = !H[1] | (!H[2] & (!H[3] | ... | 1) )
       = !H[1] | (!H[2] & 1 )
       = !H[1] | !H[2]

So for weak-minimal hashes the number of clauses is probably ok
(it is some small multiple of the number of keys), at
least for the types of constraints needed in hardware where only
a few MSBs matter.
