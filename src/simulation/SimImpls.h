#pragma once
#include "common/VariantIndex.h"
#include <tuple>
#include <variant>

template<class NewArg1, class Func>
struct RewriteArg1;

template<class NewArg1, class R, class Arg1, class ...Args>
struct RewriteArg1<NewArg1, R (*)(Arg1, Args...)>
{
	using Type = R (*)(NewArg1, Args...);
};

#define ALL_SIM_IMPLS(X, ...) \
	X(LegacyVariant __VA_OPT__(,) __VA_ARGS__) \
	X(ParallelVariant __VA_OPT__(,) __VA_ARGS__) \
	// last line of the macro, don't remove

#define DECLARE_VARIANT(Var) struct Var;
ALL_SIM_IMPLS(DECLARE_VARIANT)
#undef DECLARE_VARIANT

template<class, class ...Args>
struct SimImplsHelper
{
	using Type = std::variant<Args...>;
};

using SimImpls = SimImplsHelper<int
#define VARIANT_LIST_ITEM(Var) , Var
ALL_SIM_IMPLS(VARIANT_LIST_ITEM)
#undef VARIANT_LIST_ITEM
>::Type;
