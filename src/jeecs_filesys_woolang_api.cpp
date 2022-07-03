#define JE_IMPL
#include "jeecs.hpp"

#include <filesystem>

namespace fs = std::filesystem;

WO_API wo_api wojeapi_filesys_path(wo_vm vm, wo_value args, size_t argc)
{
    auto* result = new fs::directory_iterator(wo_string(args + 0));
    return wo_ret_gchandle(vm, result, nullptr, [](void* ptr)
        {
            delete(fs::directory_iterator*)ptr;
        });
}

WO_API wo_api wojeapi_filesys_path_next(wo_vm vm, wo_value args, size_t argc)
{
    auto* result = (fs::directory_iterator*)wo_pointer(args + 0);
    if (*result == fs::directory_iterator())
        return wo_ret_bool(vm, false);

    wo_set_string(args + 1, (*((*result)++)).path().string().c_str());
    return wo_ret_bool(vm, true);
}

WO_API wo_api wojeapi_filesys_is_dir(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, fs::is_directory(wo_string(args + 0)));
}


const char* jeecs_filesys_woolang_api_path = "je/filesys.wo";
const char* jeecs_filesys_woolang_api_src = R"(
import woo.std;

namespace je
namespace filesys
{
    using path = gchandle;
    namespace path
    {
        extern("libjoyecs", "wojeapi_filesys_path")
        func create(var _path: string): path;

        func iter(var self: path)
        {
            return self;
        }

        extern("libjoyecs", "wojeapi_filesys_path_next")
        func next(var self: path, ref out_path: string): bool;
    }

    func filename(var _path: string)
    {
        _path = _path->replace("\\", "/");
        while (_path!= "" && _path->endwith("/"))
            _path = _path->sub(0, _path->len() - 1);

        var find_place = _path->rfind("/");
        return _path->sub(find_place + 1);
    }

    func externname(var _path: string)
    {
        var fname = filename(_path);
        var fndplace = fname->rfind(".");
        if (fndplace == -1)
            return "";
        return fname->sub(fname->rfind(".") + 1);
    }

    extern("libjoyecs", "wojeapi_filesys_is_dir")
    func isdir(var _path: string): bool;
}
)";