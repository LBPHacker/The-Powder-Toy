#include "Platform.h"
#include <StackWalker.h>
#include <iostream>

class MyStackWalker : public StackWalker
{
	std::vector<String> result;

public:
	using StackWalker::StackWalker;

	std::vector<String> GetResult()
	{
		return result;
	}

protected:
	void OnCallstackEntry(CallstackEntryType eType, CallstackEntry &entry) final override
	{
		StringBuilder addr;
		addr << "0x" << Format::Hex() << uintptr_t(entry.offset) << " (" << entry.moduleName << "+0x" << Format::Hex() << (uintptr_t(entry.offset) - uintptr_t(entry.baseOfImage)) << ")";
		result.push_back(addr.Build());
		StackWalker::OnCallstackEntry(eType, entry);
	}
};
static std::unique_ptr<MyStackWalker> msw;

namespace Platform
{
std::optional<std::vector<String>> Backtrace(BacktraceType backtraceType)
{
	auto et = StackWalker::NonExcept;
	if (backtraceType == backtraceFromException)
	{
		et = StackWalker::AfterCatch;
	}
	MyStackWalker msw(et);
	msw.ShowCallstack();
	return msw.GetResult();
}
}
