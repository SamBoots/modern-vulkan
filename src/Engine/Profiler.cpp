#include "Profiler.hpp"
#include "Storage/Hashmap.h"

using namespace BB;

struct ProfilerSystem_inst
{
	OL_HashMap<const char*, ProfileResults, String_KeyComp> profile_entries;
	LinkedList<ProfileResults> profile_entry_linked;
};

static ProfilerSystem_inst* s_profiler;


