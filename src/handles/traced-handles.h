// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_TRACED_HANDLES_H_
#define V8_HANDLES_TRACED_HANDLES_H_

#include "include/v8-embedder-heap.h"
#include "include/v8-traced-handle.h"
#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/objects/objects.h"
#include "src/objects/visitors.h"

namespace v8::internal {

class Isolate;
class TracedHandlesImpl;

// TracedHandles hold handles that must go through cppgc's tracing methods. The
// handles do otherwise not keep their pointees alive.
class V8_EXPORT_PRIVATE TracedHandles final {
 public:
  static void Destroy(Address* location);
  static void Copy(const Address* const* from, Address** to);
  static void Move(Address** from, Address** to);

  static void Mark(Address* location);
  static Object MarkConservatively(Address* inner_location,
                                   Address* traced_node_block_base);

  V8_INLINE static Object Acquire(Address* location) {
    return Object(reinterpret_cast<std::atomic<Address>*>(location)->load(
        std::memory_order_acquire));
  }

  explicit TracedHandles(Isolate*);
  ~TracedHandles();

  TracedHandles(const TracedHandles&) = delete;
  TracedHandles& operator=(const TracedHandles&) = delete;

  Handle<Object> Create(Address value, Address* slot,
                        GlobalHandleStoreMode store_mode);

  using NodeBounds = std::vector<std::pair<const void*, const void*>>;
  NodeBounds GetNodeBounds() const;

  void SetIsMarking(bool);
  void SetIsSweepingOnMutatorThread(bool);

  // Updates the list of young nodes that is maintained separately.
  void UpdateListOfYoungNodes();
  // Clears the list of young nodes, assuming that the young generation is
  // empty.
  void ClearListOfYoungNodes();

  void ResetDeadNodes(WeakSlotCallbackWithHeap should_reset_handle);

  // Computes whether young weak objects should be considered roots for young
  // generation garbage collections  or just be treated weakly. Per default
  // objects are considered as roots. Objects are treated not as root when both
  // - `is_unmodified()` returns true;
  // - the `EmbedderRootsHandler` also does not consider them as roots;
  void ComputeWeaknessForYoungObjects(WeakSlotCallback is_unmodified);

  void ProcessYoungObjects(RootVisitor* v,
                           WeakSlotCallbackWithHeap should_reset_handle);

  void Iterate(RootVisitor*);
  void IterateYoung(RootVisitor*);
  void IterateYoungRoots(RootVisitor*);

  START_ALLOW_USE_DEPRECATED()

  // Iterates over all traces handles represented by
  // `v8::TracedReferenceBase`.
  void Iterate(v8::EmbedderHeapTracer::TracedGlobalHandleVisitor* visitor);

  END_ALLOW_USE_DEPRECATED()

  size_t used_node_count() const;
  size_t total_size_bytes() const;
  size_t used_size_bytes() const;

 private:
  std::unique_ptr<TracedHandlesImpl> impl_;
};

}  // namespace v8::internal

#endif  // V8_HANDLES_TRACED_HANDLES_H_
