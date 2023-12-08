#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_graphic_model_loader.hpp"

#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>

namespace jeecs
{
    class JoyEngineResourceIOStream : public Assimp::IOStream
    {
        JECS_DISABLE_MOVE_AND_COPY(JoyEngineResourceIOStream);

        jeecs_file* m_file;
    public: 
        JoyEngineResourceIOStream(jeecs_file* fres)
            : m_file(fres)
        {
            assert(m_file != nullptr);
        }

        virtual ~JoyEngineResourceIOStream()
        {
            jeecs_file_close(m_file);
        }

        virtual size_t Read(void* pvBuffer,
            size_t pSize,
            size_t pCount)override
        {
            return jeecs_file_read(pvBuffer, pSize, pCount, m_file);
        }

        virtual size_t Write(const void* pvBuffer,
            size_t pSize,
            size_t pCount)override
        {
            // NOT IMPL!
            abort();
        }

        virtual aiReturn Seek(size_t pOffset,
            aiOrigin pOrigin)
        {
            static_assert(aiOrigin::aiOrigin_CUR == je_read_file_seek_mode::JE_READ_FILE_CURRENT);
            static_assert(aiOrigin::aiOrigin_END == je_read_file_seek_mode::JE_READ_FILE_END);
            static_assert(aiOrigin::aiOrigin_SET == je_read_file_seek_mode::JE_READ_FILE_SET);

            jeecs_file_seek(m_file, (int64_t)pOffset, (je_read_file_seek_mode)pOrigin);
        }

        // -------------------------------------------------------------------
        /** @brief Get the current position of the read/write cursor
         *
         * See ftell() for more details */
        virtual size_t Tell() const override
        {
            return jeecs_file_tell(m_file);
        }

        // -------------------------------------------------------------------
        /** @brief Returns filesize
         *  Returns the filesize. */
        virtual size_t FileSize() const override
        {
            return m_file->m_file_length;
        }

        // -------------------------------------------------------------------
        /** @brief Flush the contents of the file buffer (for writers)
         *  See fflush() for more details.
         */
        virtual void Flush()
        {
            // ...
        }
    };
    class JoyEngineResourceIOSystem : public Assimp::IOSystem
    {
        bool Exists(const char* pFile) const override {
            auto* f = jeecs_file_open(pFile);
            if (f == nullptr)
                return false;

            jeecs_file_close(f);
            return true;
        }

        char getOsSeparator() const override {
            return '/';  // why not? it doesn't care
        }

        Assimp::IOStream* Open(const char* pFile, const char* pMode = "rb") override {
            auto* f = jeecs_file_open(pFile);
            if (f == nullptr)
                return nullptr;

            return new JoyEngineResourceIOStream(f);
        }

        void Close(Assimp::IOStream* pFile) override {
            delete pFile;
        }

        bool ComparePaths(const char* one, const char* second) const override {
            return strcmp(one, second) == 0;
        }

        bool PushDirectory(const std::string& path) override {
            return false;
        }

        const std::string& CurrentDirectory() const override {
            return jeecs_file_get_runtime_path();
        }

        size_t StackSize() const override {
            return 0;
        }

        bool PopDirectory() override {
            return false;
        }

        bool CreateDirectory(const std::string& path) override {
            return false;
        }

        bool ChangeDirectory(const std::string& path) override {
            return false;
        }

        bool DeleteFile(const std::string& file) override {
            return false;
        }
    };
}

//jegl_resource* jegl_load_vertex(const char* path)
//{
//    Assimp::Importer importer;
//    importer.SetIOHandler(new jeecs::JoyEngineResourceIOSystem);
//
//    auto* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals);
//    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
//    {
//        return nullptr;
//    }
//
//    // TODO:
//    return nullptr;
//}