#ifndef HERMES_VM_YOUNGGENNC_H
#define HERMES_VM_YOUNGGENNC_H

#include "hermes/VM/AlignedHeapSegment.h"
#include "hermes/VM/AllocResult.h"
#include "hermes/VM/CompactionResult.h"
#include "hermes/VM/GCGeneration.h"
#include "hermes/VM/HasFinalizer.h"
#include "hermes/VM/HermesValue.h"
#include "hermes/VM/SweepResultNC.h"
#include "hermes/VM/YoungGenTraits.h"

#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/MathExtras.h"

#include <functional>

namespace hermes {
namespace vm {

// Forward declaration.
class OldGen;

/// A generation whose preferred mode of collection is a copying evacuation, to
/// be used as the young generation in a generational heap.
class YoungGen : public GCGeneration {
 public:
  /// Initialize the YoungGen as a generation in the given GenGC, with a minimum
  /// and maximum allocation region size (in bytes) of \p minSize and \p
  /// maxSize, respectively, and a later generation \p nextGen.
  YoungGen(GenGC *gc, size_t minSize, size_t maxSize, OldGen *nextGen);

  /// @name GCGeneration API Begins
  /// @{

  /// See GCGeneration.h for more information.
  AllocResult
  allocSlow(uint32_t allocSize, HasFinalizer hasFinalizer, bool fixedSizeAlloc);
  using GCGeneration::allocRaw;
  inline size_t size() const;
  inline size_t sizeDirect() const;
  inline size_t minSize() const;
  inline size_t maxSize() const;
  inline size_t used() const;
  inline size_t usedDirect() const;
  inline size_t available() const;
  inline size_t availableDirect() const;
  inline size_t adjustSize(size_t size) const;
  inline void growTo(size_t desired);
  inline void shrinkTo(size_t desired);
  inline bool growToFit(size_t amount);
  inline SegTraits<YoungGen>::Range allSegments();
  inline SegTraits<YoungGen>::Range usedSegments();
  gcheapsize_t bytesAllocatedSinceLastGC() const;
  void forAllObjs(const std::function<void(GCCell *)> &callback);
  void creditExternalMemory(uint32_t size);
  void debitExternalMemory(uint32_t size);
  void updateEffectiveEndForExternalMemory();

#ifndef NDEBUG
  inline bool dbgContains(const void *p) const final override;
  void forObjsAllocatedSinceGC(const std::function<void(GCCell *)> &callback);
#endif

  /// @}

#ifndef NDEBUG
  /// Assumes the generation owns its allocation context.  Attempts to allocate
  /// in that; if that fails, calls allocSlow.
  AllocResult alloc(
      uint32_t allocSize,
      HasFinalizer hasFinalizer,
      bool fixedSizeAlloc = false);
#endif

  /// segment.
  inline bool contains(const void *p) const;

  /// Returns the address at which the next allocation, if any, will occur,
  /// assuming it is smaller than the available space.  The first
  /// version may be used always; the availableDirect version may only
  /// be used when the generation owns its allocation context, but is faster.
  inline char *level() const;
  inline char *levelDirect() const;

  inline void setExternalMemory(uint64_t size);

  inline size_t effectiveSize() const;

  /// See GCGeneration.h for more information.
  void sweepAndInstallForwardingPointers(GC *gc, SweepResult *sweepResult);

  /// See GCGeneration.h for more information.
  void updateReferences(GC *gc, SweepResult::VTablesRemaining &vTables);

  /// Moves any objects on the young-gen's finalizable object list that have
  /// been moved to the old generation to that generation's finalizable object
  /// list.
  void compactFinalizableObjectList();

  /// See GCGeneration.h for more information.
  void recordLevelAfterCompaction(
      CompactionResult::ChunksRemaining &usedChunks);

#ifdef HERMES_SLOW_DEBUG
  void checkWellFormed(const GC *gc) const;
#endif

  /// If *hv is a pointer into the current generation, check whether the
  /// referent has already been evacuated.  If not, copy to the old gen, and
  /// install a forwarding pointer in the vtable slot of the referent.  If so,
  /// redirect *hv to point where the already-installed forwarding pointer
  /// points.
  void ensureReferentCopied(HermesValue *hv);

  /// If *ptrLoc is a pointer into the current generation, check
  /// whether the referent has already been evacuated.  If not, copy
  /// to the old gen, and install a forwarding pointer in the vtable
  /// slot of the referent.  If so, redirect *ptrLoc to point where the
  /// already-installed forwarding pointer points.
  void ensureReferentCopied(GCCell **ptrLoc);

  /// Print stats (in JSON format) specific to young-gen collection to an output
  /// stream.
  /// \p os Is the output stream to print the stats to.
  /// \p trailingComma determines whether the output includes a trailing comma.
  void printStats(llvm::raw_ostream &os, bool trailingComma) const;

  /// Finalizes all unreachable cells with finalizers. If the cell was moved to
  /// the old generation, moves a reference to the cell from a list containing
  /// references to cells with finalizers in the young gen to a list containing
  /// references to cells with finalizers in the old gen.
  void finalizeUnreachableAndTransferReachableObjects();

  /// Static override of GCGeneration::didFinishGC().
  void didFinishGC();

  /// Called after the GC's heap has been copied to a new location, in order to
  /// update the references in this space (and the space's own limits) to the
  /// new location.
  ///
  /// This function assumes that \p gc is a valid pointer to this space's owning
  /// GC.
  void moveHeap(GC *gc, ptrdiff_t moveHeapDelta);

  /// Forward declaration of the acceptor used to evacuate the young generation.
  struct EvacAcceptor;

 private:
  /// Slow path taken when we can't attempt young-gen collection
  /// because there is insufficient free space in the older generation
  /// to allow worst-case survival.  Do a full collection.  If this
  /// frees enough memory in the old generation to allow worst-case
  /// young-gen evacuation, retry the allocation from scratch.  If
  /// not, try to grow the old generation to allow young-gen
  /// collection.  If this succeeds, also retry the allocation from
  /// scratch.  In both cases, the \p fixedSizeAlloc argument
  /// indicates that the allocation is of a small fixed size, and
  /// should always be satisfied from the young generation.
  /// Otherwise, pay attention for allocations larger than the
  /// young-generation, where allocations in the young generation will
  /// never succeed.  If the strategies above fail, or the allocation
  /// is too large, try allocating directly in the old generation.
  AllocResult fullCollectThenAlloc(
      uint32_t allocSize,
      HasFinalizer hasFinalizer,
      bool fixedSizeAlloc);

#ifndef NDEBUG
  /// In debug builds, we expose the collect call to tests.
 public:
#endif
  /// Do an evacuating collection of the young generation, copying
  /// reachable objects into the nextGen.
  void collect();
#ifndef NDEBUG
 private:
#endif

  /// Assumes that ptr is a pointer into the current space.  If the
  /// vtable slot of *ptr already contains a forwarding pointer,
  /// return that.  Otherwise, copy *ptr into the old gen, install a
  /// forwarding pointer in that vtable slot, and return the address
  /// of the copied GCCell.
  GCCell *forwardPointer(GCCell *ptr);

  /// See GCGeneration.h for more information.
  inline size_t adjustSizeWithBounds(size_t desired, size_t min, size_t max)
      const;

  /// The minimum size of the allocation region, in bytes.  This value will not
  /// exceed \c AlignedHeapSegment::maxSize().
  const size_t minSize_;

  /// The maximum size of the allocation region, in bytes.  This value will not
  /// exceed \c AlignedHeapSegment::maxSize().
  const size_t maxSize_;

  /// The low and high limits of the young gen, respectively.  These
  /// are the same as the initial segment's limits; we copy them
  /// because the allocation context holding the segment may be
  /// claimed by the containing GC.
  char *lowLim_{nullptr};
  char *hiLim_{nullptr};

  /// The older generation.
  OldGen *nextGen_;

  /// This is originally start_, and records the level at the end of the last
  /// GC.  At the start of a young-gen GC, objects at addresses at or greater
  /// than this were allocated directly in the generation since the last GC.
  char *levelAtEndOfLastGC_;

  /// Cumulative by-phase times within young-gen collection.
  double markOldToYoungSecs_ = 0.0;
  double markRootsSecs_ = 0.0;
  double scanTransitiveSecs_ = 0.0;
  double updateWeakRefsSecs_ = 0.0;
  double finalizersSecs_ = 0.0;

  /// The sum of the pre-collection sizes of the young gen before
  /// collection, and the number of bytes promoted.  The latter over
  /// the former will yield the survival rate.
  gcheapsize_t cumPreBytes_ = 0;
  gcheapsize_t cumPromotedBytes_ = 0;
};

size_t YoungGen::size() const {
  return trueActiveSegment().size();
}
size_t YoungGen::sizeDirect() const {
  return activeSegment().size();
}

size_t YoungGen::minSize() const {
  return minSize_;
}

size_t YoungGen::maxSize() const {
  return maxSize_;
}

size_t YoungGen::used() const {
  return trueActiveSegment().used() + externalMemory();
}
size_t YoungGen::usedDirect() const {
  return activeSegment().used() + externalMemory();
}

size_t YoungGen::available() const {
  return trueActiveSegment().available();
}
size_t YoungGen::availableDirect() const {
  return activeSegment().available();
}

size_t YoungGen::adjustSize(size_t desired) const {
  return adjustSizeWithBounds(desired, minSize_, maxSize_);
}

size_t YoungGen::adjustSizeWithBounds(size_t desired, size_t min, size_t max)
    const {
#ifndef NDEBUG
  const size_t PS = hermes::oscompat::page_size();
#endif

  assert(
      2 * PS <= min &&
      "The young generation's size must be at least two pages wide.");
  assert(min <= max && "The max must be at least the min size.");

  // The young generation's size must be
  //  - heap aligned
  //  - at most max bytes wide (which in turn is at most one segment's
  //    allocation region wide), up to alignment.
  assert(
      max <= AlignedHeapSegment::maxSize() &&
      "segment must be able to hold at least 2 pages");
  auto clamped = std::max(min, std::min(desired, max));
  return llvm::alignTo(clamped, HeapAlign);
}

void YoungGen::growTo(size_t desired) {
  assert(ownsAllocContext());
  activeSegment().growTo(desired);
  updateEffectiveEndForExternalMemory();
}

void YoungGen::shrinkTo(size_t desired) {
  assert(ownsAllocContext());
  assert(desired >= usedDirect());
  // Note that this assertion implies that desired >= minSize_.
  assert(desired == adjustSize(desired) && "Size must be adjusted.");

  // No-op if the size is already smaller than the desired size.
  if (sizeDirect() <= desired) {
    return;
  }

  activeSegment().shrinkTo(desired);
  updateEffectiveEndForExternalMemory();
}

bool YoungGen::growToFit(size_t amount) {
  assert(ownsAllocContext());
  bool res = activeSegment().growToFit(amount);
  updateEffectiveEndForExternalMemory();
  return res;
}

SegTraits<YoungGen>::Range YoungGen::allSegments() {
  assert(ownsAllocContext());
  return llvm::make_range(&activeSegment(), &activeSegment() + 1);
}

SegTraits<YoungGen>::Range YoungGen::usedSegments() {
  assert(ownsAllocContext());
  return llvm::make_range(&activeSegment(), &activeSegment() + 1);
}

#ifndef NDEBUG
bool YoungGen::dbgContains(const void *p) const {
  return contains(p);
}
#endif // !NDEBUG

bool YoungGen::contains(const void *p) const {
  return AlignedStorage::start(p) == lowLim_;
}

char *YoungGen::level() const {
  return trueActiveSegment().level();
}
char *YoungGen::levelDirect() const {
  return activeSegment().level();
}

void YoungGen::setExternalMemory(uint64_t size) {
  externalMemory_ = size;
}

size_t YoungGen::effectiveSize() const {
  if (sizeDirect() > externalMemory_) {
    return sizeDirect() - externalMemory_;
  } else {
    return 0;
  }
}

} // namespace vm
} // namespace hermes

#endif // HERMES_VM_YOUNGGENNC_H
