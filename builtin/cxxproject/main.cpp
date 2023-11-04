#include "jeecs.hpp"
#include "loader.hpp"

jeecs::typing::type_unregister_guard* _module_guard;

extern "C" {
	JE_EXPORT void jeecs_module_entry()
	{
		_module_guard = new jeecs::typing::type_unregister_guard();
		jemodule::register_types(_module_guard);
		jeecs::entry::module_entry(_module_guard);
	}

	JE_EXPORT void jeecs_module_leave()
	{
		jeecs::entry::module_leave(_module_guard);
		delete _module_guard;
	}
}

static_assert(std::is_same<decltype(&jeecs_module_entry), 
	jeecs::typing::module_entry_t>::value);
static_assert(std::is_same<decltype(&jeecs_module_leave),
	jeecs::typing::module_leave_t>::value);