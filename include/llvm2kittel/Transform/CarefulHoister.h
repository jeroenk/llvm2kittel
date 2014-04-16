// This file is part of llvm2KITTeL
//
// Copyright 2010-2014 Stephan Falke
//
// Licensed under the University of Illinois/NCSA Open Source License.
// See LICENSE for details.

#ifndef CAREFUL_HOISTER_H
#define CAREFUL_HOISTER_H

#include "llvm2kittel/Util/Version.h"

// llvm includes
#include "WARN_OFF.h"
#if LLVM_VERSION < VERSION(3, 3)
  #include <llvm/Function.h>
  #include <llvm/Instructions.h>
#else
  #include <llvm/IR/Function.h>
  #include <llvm/IR/Instructions.h>
#endif
#if LLVM_VERSION < VERSION(3, 5)
  #include <llvm/Analysis/Dominators.h>
#else
  #include <llvm/IR/Dominators.h>
#endif
#include <llvm/Pass.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/LoopPass.h>
#include "WARN_ON.h"

class CarefulHoister : public llvm::LoopPass
{

public:
    CarefulHoister();

    bool runOnLoop(llvm::Loop *L, llvm::LPPassManager &LPM);

    virtual const char *getPassName() const { return "Very Selective Hoister"; }

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    static char ID;

private:
    bool changed;

    void HoistRegion(llvm::Loop *L, llvm::DomTreeNode *N);

    bool doesNotAccessMemory(llvm::Function *F);

    bool canHoistInst(llvm::Instruction &I);

    bool inSubLoop(llvm::Loop *L, llvm::BasicBlock *BB);

    bool isLoopInvariantInst(llvm::Loop *L, llvm::Instruction &I);

    void hoist(llvm::BasicBlock *preheader, llvm::Instruction &I);

private:
    CarefulHoister(const CarefulHoister &);
    CarefulHoister &operator=(const CarefulHoister &);

};

llvm::LoopPass *createCarefulHoisterPass();

#endif // CAREFUL_HOISTER_H
