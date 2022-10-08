#define JE_IMPL
#include "jeecs.hpp"

#include <filesystem>

namespace fs = std::filesystem;

std::string normalize_path_str(const fs::path& p)
{
    std::string path = p.lexically_normal().string();
    if (fs::is_directory(p) && !path.empty()
        && path.back() != '/' && path.back() != '\\')
    {
        path +=
#ifdef WIN32
            "\\"
#else
            "/"
#endif
            ;
    }
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

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

    wo_set_string(args + 1, normalize_path_str((*((*result)++)).path()).c_str());
    return wo_ret_bool(vm, true);
}

WO_API wo_api wojeapi_filesys_is_dir(wo_vm vm, wo_value args, size_t argc)
{
    std::error_code ec;
    if (fs::exists(wo_string(args + 0), ec))
        return wo_ret_bool(vm, fs::is_directory(wo_string(args + 0), ec));
    return wo_ret_bool(vm, false);
}

WO_API wo_api wojeapi_filesys_is_file(wo_vm vm, wo_value args, size_t argc)
{
    std::error_code ec;
    if (fs::exists(wo_string(args + 0), ec))
        return wo_ret_bool(vm, fs::is_regular_file(wo_string(args + 0), ec));
    return wo_ret_bool(vm, false);
}

WO_API wo_api wojeapi_filesys_mkdir(wo_vm vm, wo_value args, size_t argc)
{
    std::error_code ec;
    bool created = fs::create_directories(wo_string(args + 0), ec);
    if (ec)
        return wo_ret_err_string(vm, ec.message().c_str());
    if (!created)
        return wo_ret_err_string(vm, "Failed to create dirctory.");
    return wo_ret_ok_string(vm, normalize_path_str(wo_string(args + 0)).c_str());
}

WO_API wo_api wojeapi_filesys_exist(wo_vm vm, wo_value args, size_t argc)
{
    std::error_code ec;
    return wo_ret_bool(vm, fs::exists(wo_string(args + 0), ec));
}

WO_API wo_api wojeapi_filesys_open_file_by_browser(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_int(vm, system(wo_string(args + 0)));
}

WO_API wo_api wojeapi_filesys_get_env(wo_vm vm, wo_value args, size_t argc)
{
    const char* env = getenv(wo_string(args + 0));
    if (env)
        return wo_ret_string(vm, env);
    return wo_ret_string(vm, wo_string(args + 0));
}

WO_API wo_api wojeapi_filesys_path_parent(wo_vm vm, wo_value args, size_t argc)
{
    std::string p = wo_string(args + 0);
    if (!p.empty() && p.back() == '/' || p.back() == '\\')
        p = p.substr(0, p.size() - 1);
    return wo_ret_string(vm, normalize_path_str(fs::path(p).parent_path()).c_str());
}

WO_API wo_api wojeapi_filesys_normalize_path(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, normalize_path_str(fs::path(wo_string(args + 0))).c_str());
}

WO_API wo_api wojeapi_filesys_delete(wo_vm vm, wo_value args, size_t argc)
{
    std::error_code ec;
    auto remove_count = fs::remove_all(fs::path(wo_string(args + 0)), ec);
    if (ec)
        return wo_ret_err_string(vm, ec.message().c_str());
    return wo_ret_ok_int(vm, remove_count);
}

WO_API wo_api wojeapi_filesys_copy(wo_vm vm, wo_value args, size_t argc)
{
    std::error_code ec;
    fs::copy(fs::path(wo_string(args + 0)), fs::path(wo_string(args + 1)), fs::copy_options::recursive, ec);
    if (ec) // has error?
        return wo_ret_err_string(vm, ec.message().c_str());
    return wo_ret_ok_string(vm, normalize_path_str(wo_string(args + 1)).c_str());
}
WO_API wo_api wojeapi_filesys_rename(wo_vm vm, wo_value args, size_t argc)
{
    std::error_code ec;
    fs::rename(fs::path(wo_string(args + 0)), fs::path(wo_string(args + 1)), ec);
    if (ec) // has error?
        return wo_ret_err_string(vm, ec.message().c_str());
    return wo_ret_ok_string(vm, normalize_path_str(wo_string(args + 1)).c_str());
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
    public func readall(path: string)=> option<string>;

    extern("libjoyecs", "wojeapi_filesys_file_writeall")
    public func writeall(path: string, data: string)=> bool;
}

namespace je::filesys
{
    extern("libjoyecs", "wojeapi_filesys_path_parent")
    public func parent(_path: string)=> string;

    using path = gchandle
    {
        extern("libjoyecs", "wojeapi_filesys_path")
        public func create(_path: string)=> path;

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
    
    public func childs(_path: string)
    {
        using result;

        if (isdir(_path))
        {
            let result = []mut: vec<string>;
            for (let child : path(_path))
                result->add(child);

            return ok(result->toarray);
        }
        return err(F"{_path} not a valid directory.");
    }

    public func filename(mut _path: string)
    {
        _path = _path->replace("\\", "/");
        while (_path!= "" && _path->endwith("/"))
            _path = _path->sub(0, _path->len() - 1);

        let find_place = _path->rfind("/");
        return _path->sub(find_place + 1);
    }

    public func fullexternname(_path: string)
    {
        let fname = filename(_path);
        let fndplace = fname->rfind(".");
        if (fndplace == -1)
            return "";
        return fname->sub(fndplace);
    }

    public func externname(_path: string)
    {
        let fexname = fullexternname(_path);
        if (fexname->len == 0)
            return fexname;
        return fexname->sub(1);
    }

    public func purename(_path: string)
    {
        let fexname = fullexternname(_path);
        let fname = filename(_path);
        return fname->sub(0, fname->len - fexname->len);
    }

    extern("libjoyecs", "wojeapi_filesys_normalize_path")
    public func normalize(_path: string)=> string;

    extern("libjoyecs", "wojeapi_filesys_delete")
    public func delete(_path: string)=> result<int, string>;

    public func copy(_frompath: string, _aimpath: string)=> result<string, string>
    {
        extern("libjoyecs", "wojeapi_filesys_copy")
        func _copy(_frompath: string, _aimpath: string)=> result<string, string>;

        if (exist(_aimpath))
            return result::err(F"There is already a file/dir named '{_aimpath}'");
        return _copy(_frompath, _aimpath);
    }

    public func move(_frompath: string, _aimpath: string)=> result<string, string>
    {
        extern("libjoyecs", "wojeapi_filesys_rename")
        func _move(_frompath: string, _aimpath: string)=> result<string, string>;

        if (exist(_aimpath))
            return result::err(F"There is already a file/dir named '{_aimpath}'");
        return _move(_frompath, _aimpath);
    }

    extern("libjoyecs", "wojeapi_filesys_is_dir")
    public func isdir(_path: string)=> bool;

    extern("libjoyecs", "wojeapi_filesys_is_file")
    public func isfile(_path: string)=> bool;

    extern("libjoyecs", "wojeapi_filesys_mkdir")
    public func mkdir(_path: string)=> result<string, string>;

    extern("libjoyecs", "wojeapi_filesys_exist")
    public func exist(_path: string)=> bool;

    extern("libjoyecs", "wojeapi_filesys_open_file_by_browser")
    public func open(_path: string)=> int;

    extern("libjoyecs", "wojeapi_filesys_get_env")
    public func env(_path: string)=> string;
}
)";