// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-features.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/handles/handles-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

// static
WasmFeatures WasmFeatures::FromFlags() {
  WasmFeatures features = WasmFeatures::None();
#define FLAG_REF(feat, ...) \
  if (v8_flags.experimental_wasm_##feat) features.Add(kFeature_##feat);
  FOREACH_WASM_FEATURE_FLAG(FLAG_REF)
#undef FLAG_REF
#define NON_FLAG_REF(feat, ...) features.Add(kFeature_##feat);
  FOREACH_WASM_NON_FLAG_FEATURE(NON_FLAG_REF)
#undef NON_FLAG_REF
  return features;
}

// static
WasmFeatures WasmFeatures::FromIsolate(Isolate* isolate) {
  return FromContext(isolate, handle(isolate->context(), isolate));
}

// static
WasmFeatures WasmFeatures::FromContext(Isolate* isolate,
                                       Handle<Context> context) {
  WasmFeatures features = WasmFeatures::FromFlags();
  if (isolate->AreWasmExceptionsEnabled(context)) {
    features.Add(kFeature_eh);
  }
  return features;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
