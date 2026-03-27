#pragma once
#include <stddef.h>
#include <type_traits>

template<class ClassMemberPtr>
struct ClassFromMemberPtr;

template<class ClassParam, class MemberParam>
struct ClassFromMemberPtr<MemberParam ClassParam::*>
{
    using Class = ClassParam;
};

template<class To, class From>
using CopyConst = std::conditional_t<std::is_const_v<From>, const To, To>;

template<auto ClassMember>
auto *ContainerOf(auto *ptr)
{
    using IntPtr = size_t;
    using Member = std::remove_pointer_t<decltype(ptr)>;
    using Class = ClassFromMemberPtr<decltype(ClassMember)>::Class;
    return reinterpret_cast<CopyConst<Class, Member> *>(
        reinterpret_cast<CopyConst<char, Member> *>(ptr) -
        reinterpret_cast<IntPtr>(&(reinterpret_cast<Class *>(IntPtr(0))->*ClassMember))
    );
}
