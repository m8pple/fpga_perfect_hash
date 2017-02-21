//
// Created by dt10 on 08/04/2016.
//

#ifndef FPGA_PERFECT_HASH_FAMILY_HASH_HPP
#define FPGA_PERFECT_HASH_FAMILY_HASH_HPP

/* Assume 2-bits of control per table, leaving 4 bits leftover for taps (wA).
 * Each entry gets 16 bits.
 * Total number of control bits will be 2*wO, which gives 2^(2+wO) configurations.
 * For wO=5 that will be 128...
 *
 * If we bump it to 3-bits of control, then we get 2^(3+wO), so for wO=5 thats 32768,
 * but we are down to wO*3 tap bits, or only 15, which is unlikely to span the keys
 * with enough overlap.
 *
 * Going to something bigger, if we do 8-LUTS:
 * For wO=5
 * - 3-bits control, 5-bits tap : combinations=32768, span=25
 * - 4-bits control, 4-bits tap : combinations=2^20, span=20
 *
 * For wO=6
 * - 3-bits control, 5-bits tap : combinations=32768, span=30
 * - 4-bits control, 4-bits tap : combinations=2^20, span=24
 *
 * So we can probably do wO=6 and wO=8 quite well for wI~=16,
 * especially if the load factor in each group is not 1.
 *
 * Another option is to use a concentrator, which brings the
 * key down to some smaller number of bits - essentially a very
 * loose perfect hash. For example, with wO=10 and wI=16, we could
 * first bring it down to wI'=12, which is quite feasible. This
 * could be done in parallel with the clustering hash.

 */

class FamilyHash
{
    std::vector<
};

#endif //FPGA_PERFECT_HASH_FAMILY_HASH_HPP
