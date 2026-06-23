// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <string>

#include <boost/intrusive/rbtree.hpp>

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/kernel/k_class_token.h"

namespace Kernel {

class KernelCore;
class KProcess;

#define KERNEL_AUTOOBJECT_TRAITS_IMPL(CLASS, BASE_CLASS, ATTRIBUTE)                                \
                                                                                                   \
private:                                                                                           \
    friend class ::Kernel::KClassTokenGenerator;                                                   \
    static constexpr inline auto ObjectType = ::Kernel::KClassTokenGenerator::ObjectType::CLASS;   \
    static constexpr inline const char* const TypeName = #CLASS;                                   \
    static constexpr inline ClassTokenType ClassToken() { return ::Kernel::ClassToken<CLASS>; }    \
                                                                                                   \
public:                                                                                            \
    YUZU_NON_COPYABLE(CLASS);                                                                      \
    YUZU_NON_MOVEABLE(CLASS);                                                                      \
                                                                                                   \
    using BaseClass = BASE_CLASS;                                                                  \
    static constexpr TypeObj GetStaticTypeObj() {                                                  \
        constexpr ClassTokenType Token = ClassToken();                                             \
        return TypeObj(TypeName, Token);                                                           \
    }                                                                                              \
    static constexpr const char* GetStaticTypeName() { return TypeName; }                          \
    virtual TypeObj GetTypeObj() ATTRIBUTE { return GetStaticTypeObj(); }                          \
    virtual const char* GetTypeName() ATTRIBUTE { return GetStaticTypeName(); }                    \
                                                                                                   \
private:                                                                                           \
    constexpr bool operator!=(const TypeObj& rhs)

#define KERNEL_AUTOOBJECT_TRAITS(CLASS, BASE_CLASS)                                                \
    KERNEL_AUTOOBJECT_TRAITS_IMPL(CLASS, BASE_CLASS, const override)

class KAutoObject {
protected:
    class TypeObj {
    public:
        constexpr explicit TypeObj(const char* n, ClassTokenType tok)
            : m_name(n), m_class_token(tok) {}

        constexpr const char* GetName() const {
            return m_name;
        }
        constexpr ClassTokenType GetClassToken() const {
            return m_class_token;
        }

        constexpr bool operator==(const TypeObj& rhs) const {
            return this->GetClassToken() == rhs.GetClassToken();
        }

        constexpr bool operator!=(const TypeObj& rhs) const {
            return this->GetClassToken() != rhs.GetClassToken();
        }

        constexpr bool IsDerivedFrom(const TypeObj& rhs) const {
            return (this->GetClassToken() | rhs.GetClassToken()) == this->GetClassToken();
        }

        static constexpr bool IsClassTokenDerivedFrom(ClassTokenType self, ClassTokenType base) {
            return (self | base) == self;
        }

    private:
        const char* m_name;
        ClassTokenType m_class_token;
    };

private:
    KERNEL_AUTOOBJECT_TRAITS_IMPL(KAutoObject, KAutoObject, const);

public:
    explicit KAutoObject(KernelCore& kernel) {
        m_class_token = GetStaticTypeObj().GetClassToken();
        RegisterWithKernel(kernel);
    }
    virtual ~KAutoObject() = default;

    static KAutoObject* Create(KAutoObject* ptr);

    // Destroy is responsible for destroying the auto object's resources when ref_count hits zero.
    virtual void Destroy(KernelCore& kernel) {
        UNIMPLEMENTED();
    }

    // Finalize is responsible for cleaning up resource, but does not destroy the object.
    virtual void Finalize(KernelCore& kernel) {}

    virtual KProcess* GetOwner() const {
        return nullptr;
    }

    u32 GetReferenceCount() const {
        return m_ref_count.load();
    }

    bool IsDerivedFrom(const TypeObj& rhs) const {
        return TypeObj::IsClassTokenDerivedFrom(m_class_token, rhs.GetClassToken());
    }

    bool IsDerivedFrom(const KAutoObject& rhs) const {
        return TypeObj::IsClassTokenDerivedFrom(m_class_token, rhs.m_class_token);
    }

    template <typename Derived>
    Derived DynamicCast() {
        static_assert(std::is_pointer_v<Derived>);
        using DerivedType = std::remove_pointer_t<Derived>;
        if (this->IsDerivedFrom(DerivedType::GetStaticTypeObj()))
            return Derived(this);
        return nullptr;
    }

    template <typename Derived>
    const Derived DynamicCast() const {
        static_assert(std::is_pointer_v<Derived>);
        using DerivedType = std::remove_pointer_t<Derived>;
        if (this->IsDerivedFrom(DerivedType::GetStaticTypeObj()))
            return Derived(this);
        return nullptr;
    }

    bool Open(KernelCore& kernel) {
        // Atomically increment the reference count, only if it's positive.
        u32 cur_ref_count = m_ref_count.load(std::memory_order_acquire);
        do {
            if (cur_ref_count == 0)
                return false;
            ASSERT(cur_ref_count < cur_ref_count + 1);
        } while (!m_ref_count.compare_exchange_weak(cur_ref_count, cur_ref_count + 1, std::memory_order_relaxed));
        return true;
    }

    void Close(KernelCore& kernel) {
        // Atomically decrement the reference count, not allowing it to become negative.
        u32 cur_ref_count = m_ref_count.load(std::memory_order_acquire);
        do {
            if (cur_ref_count == 0)
                return;
            ASSERT(cur_ref_count > 0);
        } while (!m_ref_count.compare_exchange_weak(cur_ref_count, cur_ref_count - 1, std::memory_order_acq_rel));
        // If ref count hits 1, destroy the object.
        if (cur_ref_count == 1) {
            this->Destroy(kernel);
            KAutoObject::UnregisterWithKernel(kernel, this);
        }
    }

private:
    void RegisterWithKernel(KernelCore& kernel);
    static void UnregisterWithKernel(KernelCore& kernel, KAutoObject* self);

    std::atomic<u32> m_ref_count{};
    ClassTokenType m_class_token{};
};

class KAutoObjectWithListContainer;

class KAutoObjectWithList : public KAutoObject, public boost::intrusive::set_base_hook<> {
public:
    explicit KAutoObjectWithList(KernelCore& kernel) : KAutoObject(kernel) {}

    static int Compare(const KAutoObjectWithList& lhs, const KAutoObjectWithList& rhs) {
        const uintptr_t lid = reinterpret_cast<uintptr_t>(std::addressof(lhs));
        const uintptr_t rid = reinterpret_cast<uintptr_t>(std::addressof(rhs));

        if (lid < rid) {
            return -1;
        } else if (lid > rid) {
            return 1;
        } else {
            return 0;
        }
    }

    friend bool operator<(const KAutoObjectWithList& left, const KAutoObjectWithList& right) {
        return KAutoObjectWithList::Compare(left, right) < 0;
    }

public:
    virtual u64 GetId() const {
        return reinterpret_cast<u64>(this);
    }

private:
    friend class KAutoObjectWithListContainer;
};

template <typename T>
class KScopedAutoObject {
public:
    YUZU_NON_COPYABLE(KScopedAutoObject);

    constexpr KScopedAutoObject(KernelCore& kernel_)
        : kernel{kernel_}
    {}

    constexpr KScopedAutoObject(KernelCore& kernel_, T* o)
        : kernel{kernel_}
        , m_obj(o)
    {
        if (m_obj != nullptr) {
            m_obj->Open(kernel);
        }
    }

    ~KScopedAutoObject() {
        if (m_obj != nullptr) {
            m_obj->Close(kernel);
        }
        m_obj = nullptr;
    }

    template <typename U>
        requires(std::derived_from<T, U> || std::derived_from<U, T>)
    constexpr KScopedAutoObject(KScopedAutoObject<U>&& rhs) {
        if constexpr (std::derived_from<U, T>) {
            // Upcast.
            m_obj = rhs.m_obj;
            rhs.m_obj = nullptr;
        } else {
            // Downcast.
            T* derived = nullptr;
            if (rhs.m_obj != nullptr) {
                derived = rhs.m_obj->template DynamicCast<T*>();
                if (derived == nullptr) {
                    rhs.m_obj->Close(rhs.kernel);
                }
            }

            m_obj = derived;
            rhs.m_obj = nullptr;
        }
    }

    constexpr KScopedAutoObject<T>& operator=(KScopedAutoObject<T>&& rhs) {
        rhs.Swap(*this);
        return *this;
    }

    constexpr T* operator->() {
        return m_obj;
    }
    constexpr T& operator*() {
        return *m_obj;
    }

    constexpr void SetObject(T* o) {
        if (m_obj)
            m_obj->Close(kernel);
        m_obj = o;
        if (m_obj)
            m_obj->Open(kernel);
    }

    constexpr void Reset(T* o) {
        KScopedAutoObject(kernel, o).Swap(*this);
    }

    constexpr T* GetPointerUnsafe() {
        return m_obj;
    }

    constexpr T* GetPointerUnsafe() const {
        return m_obj;
    }

    constexpr T* ReleasePointerUnsafe() {
        T* ret = m_obj;
        m_obj = nullptr;
        return ret;
    }

    constexpr bool IsNull() const {
        return m_obj == nullptr;
    }
    constexpr bool IsNotNull() const {
        return m_obj != nullptr;
    }

private:
    template <typename U>
    friend class KScopedAutoObject;

private:
    KernelCore& kernel;
    T* m_obj{};

private:
    constexpr void Swap(KScopedAutoObject& rhs) noexcept {
        std::swap(m_obj, rhs.m_obj);
    }
};

} // namespace Kernel
