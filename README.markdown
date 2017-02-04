# llvm2KITTeL

llvm2KITTeL is a converter from LLVM's intermediate representation
into a format that can be handled by the automatic termination prover
[KITTeL](https://github.com/s-falke/kittel-koat).

This is an adapted version of llvm2KITTeL which handles OpenCL and CUDA
kernels. The bitcode for these kernels should be produced by
[GPUVerify](http://multicore.doc.ic.ac.uk/tools/GPUVerify/) with
GPUVerify's `--stop-at-opt` option.

## Author

Stephan Falke, Jeroen Ketema, Marc Brockschmidt

The OpenCL and CUDA related changes are by Jeroen Ketema.

## Papers

Stephan Falke, Deepak Kapur, Carsten Sinz:
[Termination Analysis of C Programs Using Compiler Intermediate Languages](http://dx.doi.org/10.4230/LIPIcs.RTA.2011.41).
RTA 2011: 41-50

Stephan Falke, Deepak Kapur, Carsten Sinz:
[Termination Analysis of Imperative Programs Using Bitvector Arithmetic](http://dx.doi.org/10.1007/978-3-642-27705-4_21).
VSTTE 2012: 261-277
