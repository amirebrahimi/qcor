#include "QRTInitOpLowering.hpp"
#include "mlir/Conversion/AffineToStandard/AffineToStandard.h"
#include "mlir/Conversion/SCFToStandard/SCFToStandard.h"
#include "mlir/Conversion/StandardToLLVM/ConvertStandardToLLVM.h"
#include "mlir/Conversion/StandardToLLVM/ConvertStandardToLLVMPass.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Verifier.h"
#include "mlir/InitAllDialects.h"
#include "llvm/ADT/Sequence.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
namespace qcor {
// Match any Operation that is the QallocOp
LogicalResult
QRTInitOpLowering::matchAndRewrite(Operation *op, ArrayRef<Value> operands,
                                   ConversionPatternRewriter &rewriter) const {
  // Local Declarations, get location, parentModule
  // and the context
  auto loc = op->getLoc();
  ModuleOp parentModule = op->getParentOfType<ModuleOp>();
  auto context = parentModule->getContext();

  // First step is to get a reference to the Symbol Reference for the
  // qalloc QIR runtime function, this will only declare it once and reuse
  // each time it is seen
  FlatSymbolRefAttr symbol_ref;
  if (parentModule.lookupSymbol<LLVM::LLVMFuncOp>(qir_qrt_initialize)) {
    symbol_ref = SymbolRefAttr::get(qir_qrt_initialize, context);
  } else {
    // prototype is (Array*) -> void
    auto int_type = IntegerType::get(context, 32);
    std::vector<mlir::Type> arg_types{
        IntegerType::get(context, 32),
        LLVM::LLVMPointerType::get(
            LLVM::LLVMPointerType::get(IntegerType::get(context, 8)))};
    auto init_ftype = LLVM::LLVMFunctionType::get(
        int_type, llvm::makeArrayRef(arg_types), false);

    // Insert the function declaration
    PatternRewriter::InsertionGuard insertGuard(rewriter);
    rewriter.setInsertionPointToStart(parentModule.getBody());
    rewriter.create<LLVM::LLVMFuncOp>(parentModule->getLoc(),
                                      qir_qrt_initialize, init_ftype);
    symbol_ref = mlir::SymbolRefAttr::get(qir_qrt_initialize, context);
  }

  // create a CallOp for the new quantum runtime initialize
  // function.
  rewriter.create<mlir::CallOp>(loc, symbol_ref, IntegerType::get(context, 32),
                                operands);

  // Remove the old QuantumDialect QallocOp
  rewriter.eraseOp(op);

  return success();
}
} // namespace qcor