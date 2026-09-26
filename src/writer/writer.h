#pragma once
#include <chrono>
#include <string>

namespace dumper::writer {

    class IWriter {
      public:
        virtual ~IWriter() = default;

        auto write(const std::string& filename, std::chrono::milliseconds elapsed_time) -> bool;

      protected:
        virtual auto get_file_extension() -> std::string = 0;
        virtual auto generate_content() -> std::string = 0;
        virtual auto generate_header_comment(std::chrono::milliseconds elapsed_time) -> std::string;
    };

    class HeaderWriter : public IWriter {
      protected:
        auto get_file_extension() -> std::string override { return ".hpp"; }
        auto generate_content() -> std::string override;
    };

    class JsonWriter : public IWriter {
      protected:
        auto get_file_extension() -> std::string override { return ".json"; }
        auto generate_content() -> std::string override;
        auto generate_header_comment(std::chrono::milliseconds elapsed_time) -> std::string override;
    };

    inline HeaderWriter g_header_writer;
    inline JsonWriter g_json_writer;

} // namespace dumper::writer
