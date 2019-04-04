# ReedSolomonSIMDKernel

Ported to C from the C++ library https://github.com/catid/cm256 by Christopher Taylor

Fast GF(256) Cauchy MDS Block Erasure Codec in C

The original data should be split up into equally-sized chunks.  If one of these chunks
is erased, the redundant data can fill in the gap through decoding.

The erasure code is parameterized by three values (`OriginalCount`, `RecoveryCount`, `BlockBytes`).  These are:

+ The number of blocks of original data (`OriginalCount`), which must be less than 256.
+ The number of blocks of redundant data (`RecoveryCount`), which must be no more than `256 - OriginalCount`.

For example, if a file is split into 3 equal pieces and sent over a network, `OriginalCount` is 3.
And if 2 additional redundant packets are generated, `RecoveryCount` is 2.
In this case up to 256 - 3 = 253 additional redundant packets can be generated.
