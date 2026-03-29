#pragma once
#include <stddef.h>
#include <type_traits>

template<class ClassMemberPtr>
struct ClassFromMemberPtr;

template<class ClassParam, class MemberParam>
struct ClassFromMemberPtr<MemberParam ClassParam::*>
{
    using Class = ClassParam;
    using Member = MemberParam;
};

template<class To, class From>
using CopyConst = std::conditional_t<std::is_const_v<From>, const To, To>;

template<auto ClassMember>
auto *ContainerOf(auto *ptr)
{
    using IntPtr = size_t;
    using Member = std::remove_pointer_t<decltype(ptr)>;
    using Class = ClassFromMemberPtr<decltype(ClassMember)>::Class;
    static_assert(std::is_same_v<std::remove_const_t<Member>, typename ClassFromMemberPtr<decltype(ClassMember)>::Member>);
    return reinterpret_cast<CopyConst<Class, Member> *>(
        reinterpret_cast<CopyConst<char, Member> *>(ptr) -
        reinterpret_cast<IntPtr>(&(reinterpret_cast<Class *>(IntPtr(0))->*ClassMember))
    );
}
