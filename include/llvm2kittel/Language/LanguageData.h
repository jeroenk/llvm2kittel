// This file is part of llvm2KITTeL
//
// Copyright 2014 Jeroen Ketema
//
// Licensed under the University of Illinois/NCSA Open Source License.
// See LICENSE for details.

// Contains code from Bugle

#ifndef LANGUAGE_DATA_H
#define LANGUAGE_DATA_H

#include "llvm2kittel/Util/Version.h"

#if LLVM_VERSION < VERSION(3, 4)
#error Only version 3.4 and up supported
#endif

namespace llvm {

class ConstantInt;
class Function;
class Module;

}

class LanguageData  {
public:
    enum SourceLanguage {
        SL_C,
        SL_CUDA,
        SL_OpenCL,

        SL_Count
    };

    typedef struct {
        llvm::ConstantInt* local[3];
        llvm::ConstantInt* group[3];
    } KernelDimensions;

private:
    static void extractOpenCLAxiom(llvm::Function *F, KernelDimensions &KD);
    static void extractCUDAAxiom(llvm::Function *F, KernelDimensions &KD);

public:
    static bool isGPUEntryPoint(llvm::Function *F, llvm::Module *M, SourceLanguage SL);

  static KernelDimensions extractOpenCLDimensions(llvm::Module *M);
  static KernelDimensions extractCUDADimensions(llvm::Module *M);
};

#endif
