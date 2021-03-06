// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLASS_REQUIRES_FINALIZATION_MIXIN_H_
#define CLASS_REQUIRES_FINALIZATION_MIXIN_H_

#include "heap/stubs.h"

namespace WebCore {

class OffHeap : public RefCounted<OffHeap> { };
class OnHeap : public GarbageCollected<OnHeap> { };

class MixinFinalizable : public GarbageCollectedMixin {
public:
    void trace(Visitor*);
private:
    RefPtr<OffHeap> m_offHeap; // Requires finalization
    Member<OnHeap> m_onHeap;
};

class MixinNotFinalizable : public GarbageCollectedMixin {
public:
    void trace(Visitor*);
private:
    Member<OnHeap> m_onHeap;
};

class NeedsFinalizer
    : public GarbageCollected<NeedsFinalizer>
    , public MixinFinalizable {
    USING_GARBAGE_COLLECTED_MIXIN(NeedsFinalizer);
public:
    void trace(Visitor*);
private:
    Member<OnHeap> m_obj;
};

class HasFinalizer : public GarbageCollectedFinalized<HasFinalizer>,
                     public MixinFinalizable {
    USING_GARBAGE_COLLECTED_MIXIN(HasFinalizer);
public:
    void trace(Visitor*);
private:
    Member<OnHeap> m_obj;
};

class NeedsNoFinalization
    : public GarbageCollected<NeedsNoFinalization>
    , public MixinNotFinalizable
    , public ScriptWrappable {
    USING_GARBAGE_COLLECTED_MIXIN(NeedsNoFinalization);
public:
    void trace(Visitor*);
private:
    Member<OnHeap> m_obj;
};

}

#endif
