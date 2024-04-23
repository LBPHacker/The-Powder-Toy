#include "InitSimulationConfig.h"
#include "SimulationConfig.h"
#include "simulation/ElementDefs.h"
#include "common/String.h"
#include "prefs/GlobalPrefs.h"
#ifdef _WIN32
# include <Windows.h>
#else
# include <unistd.h>
# include <sys/mman.h>
#endif
#include <set>
#include <vector>
#include <iostream>

#define Debug 0
#ifndef Debug
# error where muh Debug D:
#endif

#if Debug
# include <iomanip>
#endif

#undef CELL
#undef CELL3
#undef CELLS
#undef RES
#undef XCELLS
#undef YCELLS
#undef NCELL
#undef XRES
#undef YRES
#undef NPART
#undef XCNTR
#undef YCNTR
#undef WINDOW
#undef WINDOWW
#undef WINDOWH

#define SIM_PARAMS_INT(X) \
	X(CELL) \
	X(CELL3) \
	X(XCELLS) \
	X(YCELLS) \
	X(NCELL) \
	X(XRES) \
	X(YRES) \
	X(NPART) \
	X(XCNTR) \
	X(YCNTR) \
	X(WINDOWW) \
	X(WINDOWH) \
	// last line of the macro

#define SIM_PARAMS_V2INT(X) \
	X(CELLS) \
	X(RES) \
	X(WINDOW) \
	// last line of the macro

enum ParamSlot
{
#define DEF_PARAMSLOT(n) paramSlot_mov_ ## n,
SIM_PARAMS_INT(DEF_PARAMSLOT)
SIM_PARAMS_V2INT(DEF_PARAMSLOT)
#undef DEF_PARAMSLOT
	paramSlot_rdiv_CELL,
	paramSlot_max,
};

static constexpr std::array<intptr_t, paramSlot_max> patchSizes = {{
#define DEF_PARAMSIZE(n) 4,
SIM_PARAMS_INT(DEF_PARAMSIZE)
#undef DEF_PARAMSIZE
#define DEF_PARAMSIZE(n) 8,
SIM_PARAMS_V2INT(DEF_PARAMSIZE)
#undef DEF_PARAMSIZE
	4, // paramSlot_rdiv_CELL
}};

#ifdef _WIN32
# define PREVIOUS "\t.text\n" // somehow this works even with -ffunction-sections, maybe they are subsections or something
#else
# define PREVIOUS "\t.previous\n"
#endif

#ifdef __APPLE__
# define ALIGN "\t.align 3\n" // (1 << 3)
# define SECTION "\t.section __DATA,trt_livepatch\n"
#else
# define ALIGN "\t.align 8\n" // (8)
# define SECTION "\t.section trt_livepatch,\"aw\"\n"
#endif

#define DEF_GETTER(n) \
	int n ## _Getter() \
	{ \
		int result; \
		__asm__( \
			"\tmovl $0x41414141, %0\n" \
			"simparam_mov_" #n "_%=:\n" \
			SECTION \
			ALIGN \
			"\t.quad simparam_mov_" #n "_%=\n" \
			"\t.quad %c1\n" \
			PREVIOUS \
			: "=r"(result) \
			: "i"(paramSlot_mov_ ## n) \
		); \
		return result; \
	}
SIM_PARAMS_INT(DEF_GETTER)
#undef DEF_GETTER

#define DEF_GETTER(n) \
	Vec2<int> n ## _Getter() \
	{ \
		Vec2<int> result; \
		__asm__( \
			"\tmovabsq $0x4141414141414141, %0\n" \
			"simparam_mov_" #n "_%=:\n" \
			SECTION \
			ALIGN \
			"\t.quad simparam_mov_" #n "_%=\n" \
			"\t.quad %c1\n" \
			PREVIOUS \
			: "=r"(result) \
			: "i"(paramSlot_mov_ ## n) \
		); \
		return result; \
	}
SIM_PARAMS_V2INT(DEF_GETTER)
#undef DEF_GETTER

int CELL_Div(int thing)
{
	int64_t thing64 = uint32_t(thing);
	int64_t result;
	__asm__(
		"\timulq $0x41414141, %1, %0\n"
		"simparam_rdiv_CELL_%=:\n"
		"\tshrq $32, %0\n"
		SECTION
		ALIGN
		"\t.quad simparam_rdiv_CELL_%=\n"
		"\t.quad %c2\n"
		PREVIOUS
		: "=r"(result)
		: "r"(thing64), "i"(paramSlot_rdiv_CELL)
	);
	return result;
}

namespace
{
	struct FullSimulationConfig
	{
#define GEN_MEMBER(t, n) t v ## n;
SIM_PARAMS(GEN_MEMBER)
#undef GEN_MEMBER

		FullSimulationConfig() = default;

		FullSimulationConfig(SimulationConfig config)
		{
			vCELL    = config.vCELL;
			vCELL3   = vCELL * 3;
			vCELLS   = config.vCELLS;
			vRES     = vCELLS * vCELL;
			vXCELLS  = vCELLS.X;
			vYCELLS  = vCELLS.Y;
			vNCELL   = vXCELLS * vYCELLS;
			vXRES    = vRES.X;
			vYRES    = vRES.Y;
			vNPART   = std::min(vXRES * vYRES, 1 << (31 - PMAPBITS));
			vXCNTR   = vXRES / 2;
			vYCNTR   = vYRES / 2;
			vWINDOW  = vRES + Vec2(BARSIZE, MENUSIZE);
			vWINDOWW = vWINDOW.X;
			vWINDOWH = vWINDOW.Y;
		}
	};
}

struct LivepatchEntry
{
	intptr_t label;
	intptr_t paramSlot;
};
static_assert(sizeof(LivepatchEntry) == 16);

#ifdef __APPLE__
# define START_ANCHOR(n) extern LivepatchEntry __start_ ## n __asm("section$start$__DATA$" #n);
# define STOP_ANCHOR(n) extern LivepatchEntry __stop_ ## n __asm("section$end$__DATA$" #n);
#else
# define START_ANCHOR(n) extern LivepatchEntry __start_ ## n;
# define STOP_ANCHOR(n) extern LivepatchEntry __stop_ ## n;
#endif
START_ANCHOR(trt_livepatch)
STOP_ANCHOR(trt_livepatch)

namespace
{
	SimulationConfig currentConfig;
	SimulationConfig nextConfig;

	class PatchHelper
	{
		intptr_t pageSize;
		struct MprotectRange
		{
			intptr_t begin;
			intptr_t end;
		};
		struct PatchRange
		{
			intptr_t begin;
			intptr_t end;
		};
		std::vector<MprotectRange> mprotectRanges;
		std::vector<PatchRange> patchRanges;

	public:
		PatchHelper()
		{
			{
#ifdef _WIN32
				SYSTEM_INFO si;
				GetSystemInfo(&si);
				pageSize = si.dwPageSize;
#else
				auto pageSizeRaw = sysconf(_SC_PAGE_SIZE);
				assert(pageSizeRaw != -1);
				pageSize = pageSizeRaw;
#endif
			}
			std::set<intptr_t> pagesToPatch;
			for (auto *entry = &__start_trt_livepatch; entry != &__stop_trt_livepatch; ++entry)
			{
				auto size = patchSizes[entry->paramSlot];
				patchRanges.emplace_back();
				patchRanges.back().begin = entry->label - size;
				patchRanges.back().end = entry->label;
				for (intptr_t i = 0; i < size; ++i)
				{
					pagesToPatch.insert((entry->label - size + i) / pageSize * pageSize);
				}
			}
			for (auto page : pagesToPatch)
			{
				if (!(mprotectRanges.size() && page == mprotectRanges.back().end))
				{
					mprotectRanges.emplace_back();
					mprotectRanges.back().begin = page;
					mprotectRanges.back().end = page;
				}
				mprotectRanges.back().end += pageSize;
			}
		}

		void Enter()
		{
			for (auto &range : mprotectRanges)
			{
				// rwx so if this code shares a page with the code being patched,
				// mprotect doesn't return into an rw (non-x) page and segfault
#if Debug
				std::cerr << "making range rwx " << std::hex << range.begin << " " << range.end << std::endl;
#endif
#ifdef _WIN32
				DWORD dontCare;
				assert(VirtualProtect(reinterpret_cast<char *>(range.begin), range.end - range.begin, PAGE_EXECUTE_READWRITE, &dontCare));
#else
				assert(!mprotect(reinterpret_cast<char *>(range.begin), range.end - range.begin, PROT_READ | PROT_EXEC | PROT_WRITE));
#endif
			}
		}

		void Exit()
		{
			for (auto &range : mprotectRanges)
			{
#if Debug
				std::cerr << "making range rw " << std::hex << range.begin << " " << range.end << std::endl;
#endif
#ifdef _WIN32
				DWORD dontCare;
				assert(VirtualProtect(reinterpret_cast<char *>(range.begin), range.end - range.begin, PAGE_EXECUTE_READ, &dontCare));
#else
				assert(!mprotect(reinterpret_cast<char *>(range.begin), range.end - range.begin, PROT_READ | PROT_EXEC));
#endif
			}
			for (auto &range : patchRanges)
			{
				// hopefully this calls FlushInstructionCache on windows
				__builtin___clear_cache(reinterpret_cast<char *>(range.begin), reinterpret_cast<char *>(range.end));
			}
		}
	};
}

void InitSimulationConfig(SimulationConfig newConfig)
{
	currentConfig = newConfig;
	nextConfig = currentConfig;
	auto full = FullSimulationConfig(currentConfig);
	uint64_t divBy = full.vCELL;
	uint32_t mulBy = (UINT64_C(0x100000000) + divBy - 1) / divBy;
	static PatchHelper patchHelper;
	patchHelper.Enter();
	for (auto *entry = &__start_trt_livepatch; entry != &__stop_trt_livepatch; ++entry)
	{
		auto size = patchSizes[entry->paramSlot];
#if Debug
		std::cerr << "patching sequence of type " << entry->paramSlot <<" at " << entry->label << ":";
		for (intptr_t i = 0; i < size; ++i)
		{
			std::cerr << " " << std::hex << std::setw(2) << std::setfill('0') << uint32_t(*reinterpret_cast<uint8_t *>(entry->label - size + i));
		}
		std::cerr << std::endl;
#endif
		switch (entry->paramSlot)
		{
#define INVOKE_PATCH(n) \
		case paramSlot_mov_ ## n: \
			*reinterpret_cast<decltype(full.v ## n) *>(entry->label - size) = full.v ## n; \
			break;
SIM_PARAMS_INT(INVOKE_PATCH)
#undef INVOKE_PATCH
#define INVOKE_PATCH(n) \
		case paramSlot_mov_ ## n: \
			*reinterpret_cast<decltype(full.v ## n) *>(entry->label - size) = full.v ## n; \
			break;
SIM_PARAMS_V2INT(INVOKE_PATCH)
#undef INVOKE_PATCH
		case paramSlot_rdiv_CELL:
			*reinterpret_cast<uint32_t *>(entry->label - size) = mulBy;
			break;
		}
	}
	patchHelper.Exit();
	std::cerr << "InitSimulationConfig succeeded, patched " << (&__stop_trt_livepatch - &__start_trt_livepatch) << " instructions" << std::endl;
}

const SimulationConfig &GetCurrentSimulationConfig()
{
	return currentConfig;
}

const SimulationConfig &GetNextSimulationConfig()
{
	return nextConfig;
}

void SetNextSimulationConfig(SimulationConfig newConfig)
{
	nextConfig = newConfig;
}

void SimulationConfig::Check() const
{
	auto checkBounds = [](const char *name, auto value, auto min, auto max) {
		if (!(value >= min && value <= max))
		{
			throw CheckFailed(ByteString::Build(name, " is ", value, ", expected to be between ", min, " and ", max));
		}
	};
	checkBounds("cell size"            , vCELL   , 1,   100);
	checkBounds("horizontal cell count", vCELLS.X, 1, 15000);
	checkBounds("vertical cell count"  , vCELLS.Y, 1, 15000);
	auto full = FullSimulationConfig(*this);
	checkBounds("simulation width" , full.vRES.X, 1, 15000);
	checkBounds("simulation height", full.vRES.Y, 1, 15000);
}

bool SimulationConfig::operator ==(const SimulationConfig &other) const
{
	return std::tie(vCELL, vCELLS) == std::tie(other.vCELL, other.vCELLS);
}

bool SimulationConfig::CanSave() const
{
	return vCELLS.X <= 255 && vCELLS.Y <= 255;
}
