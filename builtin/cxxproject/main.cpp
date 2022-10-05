#include "jeecs.hpp"
#include "loader.hpp"

// Do not modify this file!
extern "C" {
	JE_EXPORT void jeecs_module_entry()
	{
		jeecs::enrty::module_entry();
		jemodule::register_types();
	}

	JE_EXPORT void jeecs_module_leave()
	{
		jeecs::enrty::module_leave();
	}
}

static_assert(std::is_same<decltype(&jeecs_module_entry), 
	jeecs::typing::module_entry_t>::value);
static_assert(std::is_same<decltype(&jeecs_module_leave),
	jeecs::typing::module_leave_t>::value);