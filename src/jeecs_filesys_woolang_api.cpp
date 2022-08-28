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
    std::error_code ec;
    if (fs::exists(wo_string(args + 0), ec))
        return wo_ret_bool(vm, fs::is_directory(wo_string(args + 0), ec));
    return wo_ret_bool(vm, false);
}

WO_API wo_api wojeapi_filesys_exist(wo_vm vm, wo_value args, size_t argc)
{
    std::error_code ec;
    return wo_ret_bool(vm, fs::exists(wo_string(args + 0), ec));
}

WO_API wo_api wojeapi_filesys_open_file_by_browser(wo_vm vm, wo_value args, size_t argc)
{
    system(wo_string(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_filesys_path_parent(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, fs::path(wo_string(args + 0)).parent_path().string().c_str());
}

WO_API wo_api wojeapi_filesys_file_readall(wo_vm vm, wo_value args, size_t argc)
{
    auto* file = jeecs_file_open(wo_string(args + 0));
    if (file)
    {
        char* buf = (char*)je_mem_alloc(file->m_file_length + 1);
        jeecs_file_read(buf, sizeof(char), file->m_file_length, file);
        buf[file->m_file_length] = 0;

        jeecs_file_close(file);
        return wo_ret_option_string(vm, jeecs::basic::make_cpp_string(buf).c_str());
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_filesys_file_writeall(wo_vm vm, wo_value args, size_t argc)
{
    FILE* f = fopen(wo_string(args + 0), "w");
    if (f)
    {
        wo_string_t str = wo_string(args + 1);
        fwrite(str, sizeof(char), strlen(str), f);

        fclose(f);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

const char* jeecs_filesys_woolang_api_path = "je/filesys.wo";
const char* jeecs_filesys_woolang_api_src = R"(
import woo.std;

namespace je::file
{
    extern("libjoyecs", "wojeapi_filesys_file_readall")
    func readall(path: string)=> option<string>;

    extern("libjoyecs", "wojeapi_filesys_file_writeall")
    func writeall(path: string, data: string)=> bool;
}

namespace je::filesys
{
    using path = gchandle;

    func parent(_path: string)
    {
        extern("libjoyecs", "wojeapi_filesys_path_parent")
        func _parent_path(_path: string)=> string;

        return _parent_path(_path);
    }

    namespace path
    {
        extern("libjoyecs", "wojeapi_filesys_path")
        func create(_path: string)=> path;

        func iter(self: path)
        {
            return self;
        }
        
        func next(self: path, ref out_path: string)
        {
            extern("libjoyecs", "wojeapi_filesys_path_next")
            func _next(self: path, ref out_path: string)=> bool;

            let result = _next(self, ref out_path);

            if (result)
                out_path = out_path->replace("\\", "/");

            return result;
        }
    }

    func filename(mut _path: string)
    {
        _path = _path->replace("\\", "/");
        while (_path!= "" && _path->endwith("/"))
            _path = _path->sub(0, _path->len() - 1);

        let find_place = _path->rfind("/");
        return _path->sub(find_place + 1);
    }

    func externname(_path: string)
    {
        let fname = filename(_path);
        let fndplace = fname->rfind(".");
        if (fndplace == -1)
            return "";
        return fname->sub(fname->rfind(".") + 1);
    }

    extern("libjoyecs", "wojeapi_filesys_is_dir")
    func isdir(_path: string)=> bool;

    extern("libjoyecs", "wojeapi_filesys_exist")
    func exist(_path: string)=> bool;

    extern("libjoyecs", "wojeapi_filesys_open_file_by_browser")
    func open(_path: string)=> void;
}
)";