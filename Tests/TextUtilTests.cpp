#include "CppUnitTest.h"
#include "TextUtil.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

TEST_CLASS(TextUtilTests)
{
public:
    TEST_METHOD(TrimRemovesSpacesTabsNewlinesAndNbsp)
    {
        Assert::AreEqual(std::wstring(L"*101#"), Trim(L" \t *101#\r\n "));
    }

    TEST_METHOD(TrimOfBlankIsEmpty)
    {
        Assert::AreEqual(std::wstring(), Trim(L"  \n "));
    }

    TEST_METHOD(FirstLineStopsAtNewline)
    {
        Assert::AreEqual(std::wstring(L"ยอดคงเหลือ47.79บ."), FirstLine(L" ยอดคงเหลือ47.79บ.\r\nซื้อโปร"));
    }
};
