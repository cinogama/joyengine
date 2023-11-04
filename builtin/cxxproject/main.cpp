#include "jeecs.h"
#include "loader.hpp"

extern "C" {
	JE_EXPORT void jeecs_module_entry()
	{
		jemodule::register_types();
		jeecs::entry::module_entry();
	}

	JE_EXPORT void jeecs_module_leave()
	{
		jeecs::entry::module_leave();
		jeecs::entry::module_preshutdown();
	}
}

static_assert(std::is_same<decltype(&jeecs_module_entry), 
	jeecs::typing::module_entry_t>::value);
static_assert(std::is_same<decltype(&jeecs_module_leave),
	jeecs::typing::module_leave_t>::value);