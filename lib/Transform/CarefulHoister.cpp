// This file is part of llvm2KITTeL
//
// Copyright 2010-2014 Stephan Falke
//
// Licensed under the University of Illinois/NCSA Open Source License.
// See LICENSE for details.

// Contains code from LLVM's LICM.cpp

#include "llvm2kittel/Transform/CarefulHoister.h"
#include "llvm2kittel/Util/Version.h"

// llvm includes
#include <llvm/InitializePasses.h>
#include "WARN_OFF.h"
#if LLVM_VERSION < VERSION(3, 3)
  #include <llvm/LLVMContext.h>
  #include <llvm/Constants.h>
#else
  #include <llvm/IR/LLVMContext.h>
  #include <llvm/IR/Constants.h>
  #include <llvm/IR/IntrinsicInst.h>
#endif
#include <llvm/PassRegistry.h>
#include "WARN_ON.h"

// C++ includes
#include <iostream>

char CarefulHoister::ID = 0;

CarefulHoister::CarefulHoister()
  : llvm::LoopPass(ID),
    changed(false)
{}

void CarefulHoister::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
#if LLVM_VERSION < VERSION(3, 5)
    AU.addRequired<llvm::DominatorTree>();
#else
    AU.addRequired<llvm::DominatorTreeWrapperPass>();
#endif
    AU.addRequired<llvm::LoopInfo>();
}

/// Hoist expressions out of the specified loop.
bool CarefulHoister::runOnLoop(llvm::Loop *L, llvm::LPPassManager &)
{
    changed = false;

#if LLVM_VERSION < VERSION(3, 5)
    llvm::DominatorTree *DT = &getAnalysis<llvm::DominatorTree>();
#else
    llvm::DominatorTree *DT = &getAnalysis<llvm::DominatorTreeWrapperPass>().getDomTree();
#endif

    // Get the preheader block to move instructions into...
    llvm::BasicBlock *preheader = L->getLoopPreheader();

    // We want to visit all of the instructions in this loop... that are not parts
    // of our subloops (they have already had their invariants hoisted out of
    // their loop, into this loop, so there is no need to process the BODIES of
    // the subloops).
    if (preheader != NULL) {
        HoistRegion(L, DT->getNode(L->getHeader()));
    }

    return changed;
}

/// HoistRegion - Walk the specified region of the CFG (defined by all blocks
/// dominated by the specified block, and that are in the current loop) in depth
/// first order w.r.t the DominatorTree.  This allows us to visit definitions
/// before uses, allowing us to hoist a loop body in one pass without iteration.
void CarefulHoister::HoistRegion(llvm::Loop *L, llvm::DomTreeNode *N)
{
    llvm::BasicBlock *BB = N->getBlock();

    // If this subregion is not in the top level loop at all, exit.
    if (!L->contains(BB)) {
        return;
    }

    // Only need to process the contents of this block if it is not part of a
    // subloop (which would already have been processed).
    if (!inSubLoop(L, BB)) {
        for (llvm::BasicBlock::iterator II = BB->begin(), E = BB->end(); II != E; ) {
            llvm::Instruction &I = *II++;

            // Try hoisting the instruction out to the preheader.  We can only do this
            // if all of the operands of the instruction are loop invariant and if it
            // is safe to hoist the instruction.
            if (isLoopInvariantInst(L, I) && canHoistInst(I)) {
                hoist(L->getLoopPreheader(), I);
            }
        }

        const std::vector<llvm::DomTreeNode*> &children = N->getChildren();
        for (unsigned int i = 0, e = static_cast<unsigned int>(children.size()); i != e; ++i) {
            HoistRegion(L, children[i]);
        }
    }
}

bool CarefulHoister::doesNotAccessMemory(llvm::Function *F)
{
    if (F == NULL || F->isDeclaration()) {
        return false;
    }
    for (llvm::Function::iterator bb = F->begin(), fend = F->end(); bb != fend; ++bb) {
        for (llvm::BasicBlock::iterator inst = bb->begin(), bbend = bb->end(); inst != bbend; ++inst) {
            if (llvm::isa<llvm::LoadInst>(inst) || llvm::isa<llvm::StoreInst>(inst)) {
                return false;
            }
            if (llvm::isa<llvm::CallInst>(inst)) {
                if (llvm::IntrinsicInst *I = llvm::dyn_cast<llvm::IntrinsicInst>(inst)) {
                    if (I->getIntrinsicID() == llvm::Intrinsic::dbg_value)
                        continue;
                }
                llvm::CallInst *callInst = llvm::cast<llvm::CallInst>(inst);
                llvm::Function *calledFunction = callInst->getCalledFunction();
                if (calledFunction == NULL) {
                    return false;
                }
                if (!doesNotAccessMemory(calledFunction)) {
                    return false;
                }
            }
        }
    }
    return true;
}

/// canHoistInst - Return true if the hoister can handle this instruction.
bool CarefulHoister::canHoistInst(llvm::Instruction &I)
{
    // Loads cannot be hoisted due to concurrency in kernels
    if (llvm::dyn_cast<llvm::LoadInst>(&I)) {
        return false;
    } else if (llvm::CallInst *CI = llvm::dyn_cast<llvm::CallInst>(&I)) {
        if (doesNotAccessMemory(CI->getCalledFunction())) {
            return true;
        } else {
            return false;
        }
    }

    // Otherwise these instructions are hoistable.
    llvm::BinaryOperator *BO = llvm::dyn_cast<llvm::BinaryOperator>(&I);
    if (BO) {
        switch (BO->getOpcode()) {
            case llvm::BinaryOperator::SDiv:
            case llvm::BinaryOperator::UDiv:
            case llvm::BinaryOperator::FDiv:
            case llvm::BinaryOperator::SRem:
            case llvm::BinaryOperator::URem:
                return true;
            default:
                return false;
        }
    } else if (llvm::isa<llvm::ZExtInst>(I) || llvm::isa<llvm::SExtInst>(I) ||
        llvm::isa<llvm::TruncInst>(I)) {
        return true;
    } else if (llvm::isa<llvm::PtrToIntInst>(I)) {
        return true;
    } else if (llvm::isa<llvm::FPToSIInst>(I) || llvm::isa<llvm::FPToUIInst>(I)) {
        return true;
    } else {
        return false;
    }
}

/// isLoopInvariantInst - Return true if all operands of this instruction are loop invariant.
bool CarefulHoister::isLoopInvariantInst(llvm::Loop *L, llvm::Instruction &I)
{
    // The instruction is loop invariant if all of its operands are loop-invariant
    for (unsigned int i = 0, e = I.getNumOperands(); i != e; ++i) {
        if (!L->isLoopInvariant(I.getOperand(i))) {
            return false;
        }
    }

    // If we got this far, the instruction is loop invariant!
    return true;
}

/// hoist - When an instruction is found to only use loop invariant operands
/// that is safe to hoist, this instruction is called to do the dirty work.
void CarefulHoister::hoist(llvm::BasicBlock *preheader, llvm::Instruction &I)
{
    // Remove the instruction from its current basic block... but don't delete the
    // instruction.
    I.removeFromParent();

    // Insert the new node in Preheader, before the terminator.
    preheader->getInstList().insert(preheader->getTerminator(), &I);

    changed = true;
}

bool CarefulHoister::inSubLoop(llvm::Loop *L, llvm::BasicBlock *BB)
{
    for (llvm::Loop::iterator I = L->begin(), E = L->end(); I != E; ++I) {
        if ((*I)->contains(BB)) {
            return true;  // A subloop actually contains this block!
        }
    }
    return false;
}

llvm::LoopPass *createCarefulHoisterPass()
{
#if LLVM_VERSION < VERSION(3, 5)
    llvm::initializeDominatorTreePass(*llvm::PassRegistry::getPassRegistry());
#else
    llvm::initializeDominatorTreeWrapperPassPass(*llvm::PassRegistry::getPassRegistry());
#endif
    llvm::initializeLoopInfoPass(*llvm::PassRegistry::getPassRegistry());
    return new CarefulHoister();
}
