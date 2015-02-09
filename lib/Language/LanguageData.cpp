// This file is part of llvm2KITTeL
//
// Copyright 2014 Jeroen Ketema
//
// Licensed under the University of Illinois/NCSA Open Source License.
// See LICENSE for details.

// Contains code from Bugle

#include "llvm2kittel/Language/LanguageData.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

#include <iostream>

bool LanguageData::isGPUEntryPoint(llvm::Function *F, llvm::Module *M, SourceLanguage SL) {
    if (SL == SL_CUDA || SL_OpenCL) {
        if (llvm::NamedMDNode *NMD = M->getNamedMetadata("nvvm.annotations")) {
            for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
                llvm::MDNode *MD = NMD->getOperand(i);
#if LLVM_VERSION < VERSION(3, 6)
                if (MD->getOperand(0) == F)
#else
                    if (MD->getOperand(0) == llvm::ValueAsMetadata::get(F))
#endif
                    for (unsigned fi = 1, fe = MD->getNumOperands(); fi != fe; fi += 2)
#if LLVM_VERSION < VERSION(3, 6)
                        if (MD->getOperand(fi)->getName() == "kernel")
                            return true;
#else
                        if (llvm::cast<llvm::MDString>(MD->getOperand(fi))->getString() == "kernel")
                            return true;
#endif
            }
        }
    }

    if (SL == SL_OpenCL) {
        if (llvm::NamedMDNode *NMD = M->getNamedMetadata("opencl.kernels")) {
            for (unsigned i = 0, e =  NMD->getNumOperands(); i != e; ++i) {
                llvm::MDNode *MD = NMD->getOperand(i);
#if LLVM_VERSION < VERSION(3, 6)
                if (MD->getOperand(0) == F)
#else
                if (MD->getOperand(0) == llvm::ValueAsMetadata::get(F))
#endif
                    return true;
            }
        }
    }

    return false;
}

void LanguageData::extractOpenCLAxiom(llvm::Function *F, KernelDimensions &KD) {
    // Expect one basic block
    if (F->size() != 1)
        return;
    llvm::BasicBlock::iterator I = F->begin()->begin();

    // Call to get_local_size or get_num_groups
    llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(I++);
    // Expect call to function
    if (!C || !C->getCalledFunction())
        return;
    std::string fnName = C->getCalledFunction()->getName();

    llvm::ConstantInt *arg = llvm::dyn_cast<llvm::ConstantInt>(C->getArgOperand(0));
    // Expect constant argument less than 3
    if (!arg || arg->getZExtValue() >= 3)
        return;
    unsigned dim = arg->getZExtValue();

    llvm::ICmpInst *IC = llvm::dyn_cast<llvm::ICmpInst>(I++);
    // Expect cmp eq
    if (!IC || IC->getPredicate() != llvm::ICmpInst::ICMP_EQ)
        return;

    llvm::ConstantInt *dimVal = llvm::dyn_cast<llvm::ConstantInt>(IC->getOperand(1));
    // Expect constant operand
    if (!dimVal)
        return;

    // Expect return
    if (!llvm::dyn_cast<llvm::ReturnInst>(I))
        return;

    if (fnName == "get_local_size") {
        KD.local[dim] = dimVal;
        KD.bitwidth = llvm::cast<llvm::IntegerType>(IC->getOperand(1)->getType())->getBitWidth();
    } else if (fnName == "get_num_groups") {
        KD.group[dim] = dimVal;
        KD.bitwidth = llvm::cast<llvm::IntegerType>(IC->getOperand(1)->getType())->getBitWidth();
    } else
        return;
}

LanguageData::KernelDimensions LanguageData::extractOpenCLDimensions(llvm::Module *M) {
    KernelDimensions KD = {{0, 0, 0}, {0, 0, 0}, 0};

    for (llvm::Module::iterator i = M->begin(), e = M->end(); i != e; ++i) {
        if (!i->getName().startswith("__axiom"))
            continue;

        extractOpenCLAxiom(&*i, KD);
    }

    for (unsigned i = 0; i < 3; ++i) {
        if (KD.local[i] == 0 || KD.local[i]->getSExtValue() <= 0) {
            std::cerr << "Unsupported kernel dimension" << std::endl;
            std::exit(111);
        }
        if (KD.group[i] == 0 || KD.group[i]->getSExtValue() <= 0) {
            std::cerr << "Unsupported kernel dimension" << std::endl;
            std::exit(111);
        }
    }

    return KD;
}

void LanguageData::extractCUDAAxiom(llvm::Function *F, KernelDimensions &KD) {
    // Expect one basic block
    if (F->size() != 1)
        return;
    llvm::BasicBlock::iterator I = F->begin()->begin();

    llvm::AddrSpaceCastInst *A = llvm::dyn_cast<llvm::AddrSpaceCastInst>(I++);
    // Expect address space cast
    if (!A)
        return;

    llvm::Type *T = A->getOperand(0)->getType()->getPointerElementType();
    // Expect struct type 3D vector
    if(!T->isStructTy() || T->getStructName() != "struct._3DimensionalVector")
        return;

    std::string structName = A->getOperand(0)->getName();
    llvm::GetElementPtrInst *GEP = llvm::dyn_cast<llvm::GetElementPtrInst>(I++);
    // Expect get element pointer to address space cast with three operands
    if (!GEP || GEP->getOperand(0) != A || GEP->getNumOperands() != 3)
        return;

    llvm::ConstantInt *op1 = llvm::dyn_cast<llvm::ConstantInt>(GEP->getOperand(1));
    // Expect constant integer with 0 value
    if (!op1 || op1->getZExtValue() != 0)
        return;

    llvm::ConstantInt *op2 = llvm::dyn_cast<llvm::ConstantInt>(GEP->getOperand(2));
    // Expect constant integer smaller than 3
    if (!op2 || op2->getZExtValue() >= 3)
        return;
    unsigned dim = op2->getZExtValue();

    llvm::LoadInst *L = llvm::dyn_cast<llvm::LoadInst>(I++);
    // Expect load instuction from get element pointer
    if (!L || L->getOperand(0) != GEP)
        return;

    llvm::CmpInst *IC = llvm::dyn_cast<llvm::CmpInst>(I++);
    // Expect cmp eq with
    if (!IC || IC->getPredicate() != llvm::CmpInst::ICMP_EQ)
        return;

    llvm::ConstantInt *dimVal = llvm::dyn_cast<llvm::ConstantInt>(IC->getOperand(1));
    // Expect constant operand
    if (!dimVal)
        return;

    // Expect return
    if (!llvm::dyn_cast<llvm::ReturnInst>(I))
        return;

    if (structName == "blockDim") {
        KD.local[dim] = dimVal;
        KD.bitwidth = llvm::cast<llvm::IntegerType>(IC->getOperand(1)->getType())->getBitWidth();
    } else if (structName == "gridDim") {
        KD.group[dim] = dimVal;
        KD.bitwidth = llvm::cast<llvm::IntegerType>(IC->getOperand(1)->getType())->getBitWidth();
    } else
        return;
}

LanguageData::KernelDimensions LanguageData::extractCUDADimensions(llvm::Module *M) {
    KernelDimensions KD = {{0, 0, 0}, {0, 0, 0}, 0};

    for (llvm::Module::iterator i = M->begin(), e = M->end(); i != e; ++i) {
        if (!i->getName().startswith("__axiom"))
            continue;

        extractCUDAAxiom(&*i, KD);
    }

    for (unsigned i = 0; i < 3; ++i) {
        if (KD.local[i] == 0 || KD.local[i]->getSExtValue() <= 0) {
            std::cerr << "Unsupported kernel dimension" << std::endl;
            std::exit(111);
        }
        if (KD.group[i] == 0 || KD.group[i]->getSExtValue() <= 0) {
            std::cerr << "Unsupported kernel dimension" << std::endl;
            std::exit(111);
        }
    }

    return KD;
}
