// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_FUNCTION_BODY_DECODER_H_
#define V8_WASM_FUNCTION_BODY_DECODER_H_

#include "src/base/compiler-specific.h"
#include "src/base/iterator.h"
#include "src/common/globals.h"
#include "src/wasm/decoder.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class AccountingAllocator;
class BitVector;  // forward declaration

namespace wasm {

class WasmFeatures;
struct WasmModule;  // forward declaration of module interface.

// A wrapper around the signature and bytes of a function.
struct FunctionBody {
  const FunctionSig* sig;  // function signature
  uint32_t offset;         // offset in the module bytes, for error reporting
  const byte* start;       // start of the function body
  const byte* end;         // end of the function body

  FunctionBody(const FunctionSig* sig, uint32_t offset, const byte* start,
               const byte* end)
      : sig(sig), offset(offset), start(start), end(end) {}
};

enum class LoadTransformationKind : uint8_t { kSplat, kExtend, kZeroExtend };

V8_EXPORT_PRIVATE DecodeResult ValidateFunctionBody(
    AccountingAllocator* allocator, const WasmFeatures& enabled,
    const WasmModule* module, WasmFeatures* detected, const FunctionBody& body);

enum PrintLocals { kPrintLocals, kOmitLocals };
V8_EXPORT_PRIVATE
bool PrintRawWasmCode(AccountingAllocator* allocator, const FunctionBody& body,
                      const WasmModule* module, PrintLocals print_locals);

V8_EXPORT_PRIVATE
bool PrintRawWasmCode(AccountingAllocator* allocator, const FunctionBody& body,
                      const WasmModule* module, PrintLocals print_locals,
                      std::ostream& out,
                      std::vector<int>* line_numbers = nullptr);

// A simplified form of AST printing, e.g. from a debugger.
void PrintRawWasmCode(const byte* start, const byte* end);

struct BodyLocalDecls {
  // The size of the encoded declarations.
  uint32_t encoded_size = 0;  // size of encoded declarations

  uint32_t num_locals = 0;
  ValueType* local_types = nullptr;
};

V8_EXPORT_PRIVATE bool DecodeLocalDecls(const WasmFeatures& enabled,
                                        BodyLocalDecls* decls,
                                        const WasmModule* module,
                                        const byte* start, const byte* end,
                                        Zone* zone);

V8_EXPORT_PRIVATE BitVector* AnalyzeLoopAssignmentForTesting(
    Zone* zone, uint32_t num_locals, const byte* start, const byte* end);

// Computes the length of the opcode at the given address.
V8_EXPORT_PRIVATE unsigned OpcodeLength(const byte* pc, const byte* end);

// Computes the stack effect of the opcode at the given address.
// Returns <pop count, push count>.
// Be cautious with control opcodes: This function only covers their immediate,
// local stack effect (e.g. BrIf pops 1, Br pops 0). Those opcodes can have
// non-local stack effect though, which are not covered here.
// TODO(clemensb): This is only used by the interpreter; move there.
V8_EXPORT_PRIVATE std::pair<uint32_t, uint32_t> StackEffect(
    const WasmModule* module, const FunctionSig* sig, const byte* pc,
    const byte* end);

// Checks if the underlying hardware supports the Wasm SIMD proposal.
V8_EXPORT_PRIVATE bool CheckHardwareSupportsSimd();

// A simple forward iterator for bytecodes.
class V8_EXPORT_PRIVATE BytecodeIterator : public NON_EXPORTED_BASE(Decoder) {
  // Base class for both iterators defined below.
  class iterator_base {
   public:
    iterator_base& operator++() {
      DCHECK_LT(ptr_, end_);
      ptr_ += OpcodeLength(ptr_, end_);
      return *this;
    }
    bool operator==(const iterator_base& that) const {
      return this->ptr_ == that.ptr_;
    }
    bool operator!=(const iterator_base& that) const {
      return this->ptr_ != that.ptr_;
    }

   protected:
    const byte* ptr_;
    const byte* end_;
    iterator_base(const byte* ptr, const byte* end) : ptr_(ptr), end_(end) {}
  };

 public:
  // If one wants to iterate over the bytecode without looking at {pc_offset()}.
  class opcode_iterator
      : public iterator_base,
        public base::iterator<std::input_iterator_tag, WasmOpcode> {
   public:
    WasmOpcode operator*() {
      DCHECK_LT(ptr_, end_);
      return static_cast<WasmOpcode>(*ptr_);
    }

   private:
    friend class BytecodeIterator;
    opcode_iterator(const byte* ptr, const byte* end)
        : iterator_base(ptr, end) {}
  };
  // If one wants to iterate over the instruction offsets without looking at
  // opcodes.
  class offset_iterator
      : public iterator_base,
        public base::iterator<std::input_iterator_tag, uint32_t> {
   public:
    uint32_t operator*() {
      DCHECK_LT(ptr_, end_);
      return static_cast<uint32_t>(ptr_ - start_);
    }

   private:
    const byte* start_;
    friend class BytecodeIterator;
    offset_iterator(const byte* start, const byte* ptr, const byte* end)
        : iterator_base(ptr, end), start_(start) {}
  };

  // Create a new {BytecodeIterator}, starting after the locals declarations.
  BytecodeIterator(const byte* start, const byte* end);

  // Create a new {BytecodeIterator}, starting with locals declarations.
  BytecodeIterator(const byte* start, const byte* end, BodyLocalDecls* decls,
                   Zone* zone);

  base::iterator_range<opcode_iterator> opcodes() {
    return base::iterator_range<opcode_iterator>(opcode_iterator(pc_, end_),
                                                 opcode_iterator(end_, end_));
  }

  base::iterator_range<offset_iterator> offsets() {
    return base::iterator_range<offset_iterator>(
        offset_iterator(start_, pc_, end_),
        offset_iterator(start_, end_, end_));
  }

  WasmOpcode current() {
    return static_cast<WasmOpcode>(
        read_u8<Decoder::NoValidationTag>(pc_, "expected bytecode"));
  }

  void next() {
    if (pc_ < end_) {
      pc_ += OpcodeLength(pc_, end_);
      if (pc_ >= end_) pc_ = end_;
    }
  }

  bool has_next() { return pc_ < end_; }

  WasmOpcode prefixed_opcode() {
    return read_prefixed_opcode<Decoder::NoValidationTag>(pc_);
  }
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_BODY_DECODER_H_
