# Engine regression artifacts

`asrc-segmentation.csv` records the measured baseline and patched output error
for the deterministic 16,384-frame stream. The baseline runner uses non-halting
UBSan to quantify output even though it reports alignment UB; the patched run
uses halt-on-error. Both compile the native C core as `-funsigned-char` to
model ARM64.

`callback-before.csv` and `callback-after.csv` are full CSV distributions from
the same GCC 14 `-O2 -DNDEBUG -funsigned-char` harness. The benchmark changes
frame size every measured callback and includes the cost of disabled effect
refresh. The before diagnostics count 416 synchronous partition warning lines;
the after count is zero. Timings are from this host and are not Android device
performance claims.

`host-regression-unsigned-char.log` is the final ASan/UBSan run of the real
JamesDSP core and legacy native wrapper, with Steam Audio mocked only at its
API boundary. AIDL/FMQ integration and real HRTF output are not represented.
