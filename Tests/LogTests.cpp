#include "CppUnitTest.h"
#include "Log.h"

#include <fstream>
#include <sstream>
#include <windows.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

namespace
{
    std::string ReadAll(std::filesystem::path const& p)
    {
        std::ifstream f(p, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }
}

TEST_CLASS(LogTests)
{
    std::filesystem::path m_dir;

public:
    TEST_METHOD_INITIALIZE(Init)
    {
        m_dir = std::filesystem::temp_directory_path() / (L"wanutility_log_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64()));
        std::filesystem::remove_all(m_dir);
    }

    TEST_METHOD_CLEANUP(Cleanup)
    {
        std::error_code ec;
        std::filesystem::remove_all(m_dir, ec);
    }

    TEST_METHOD(ToUtf8EncodesThai)
    {
        // U+0E22 THAI CHARACTER YO YAK = E0 B8 A2
        Assert::AreEqual(std::string("\xE0\xB8\xA2" "47"), Log::ToUtf8(L"ย47"));
    }

    TEST_METHOD(WritesThaiAsUtf8AndCreatesDirectories)
    {
        auto file = m_dir / L"logs" / L"test.log";
        Log::Init(file);
        Log::Write(L"ยอดคงเหลือ47.79บ.");
        Log::Write(L"second line");
        auto text = ReadAll(file);
        Assert::IsTrue(text.find(Log::ToUtf8(L"ยอดคงเหลือ47.79บ.")) != std::string::npos);
        Assert::IsTrue(text.find("second line\n") != std::string::npos); // line after Thai is not lost
    }

    TEST_METHOD(RotatesWhenTooLarge)
    {
        auto file = m_dir / L"r.log";
        Log::Init(file, 200);
        for (int i = 0; i < 20; ++i) Log::Write(L"0123456789012345678901234567890123456789");
        Assert::IsTrue(std::filesystem::exists(file.wstring() + L".1"));
        Assert::IsTrue(std::filesystem::file_size(file) < 400);
    }
};
