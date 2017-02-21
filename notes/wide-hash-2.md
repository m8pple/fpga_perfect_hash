Assume we have wI, wO, wA as normal.

Then we create one selector LUT, whose only job is to produce
a bit that partitions the keys into two equal size groups.

We then create a bit-hash as normal, but now there are (wA-1)
entries. Empirically:
- wO=6 and wA=4 seems fine
- wO=7 and wA=5 seems fine
- wO=8 and wA=6 is fine
- wO=9 and wA=6 is difficult

So in principle we could use this to extend up by one bit,
maybe into wO=10 with a fairly low load factor.