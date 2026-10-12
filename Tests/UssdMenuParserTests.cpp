#include "CppUnitTest.h"
#include "UssdMenuParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

TEST_CLASS(UssdMenuParserTests)
{
public:
    TEST_METHOD(ParsesNumberedMenuLines)
    {
        auto o = ParseUssdMenu(L"เลือกโปร\n1. เน็ตไม่อั้น 4Mbps 34บ./24ชม.\n2. เน็ต 3GB 7วัน\n0. ถัดไป");
        Assert::AreEqual(3, static_cast<int>(o.size()));
        Assert::AreEqual(std::wstring(L"1"), o[0].key);
        Assert::AreEqual(std::wstring(L"เน็ตไม่อั้น 4Mbps 34บ./24ชม."), o[0].label);
        Assert::AreEqual(std::wstring(L"2"), o[1].key);
        Assert::AreEqual(std::wstring(L"0"), o[2].key);
        Assert::AreEqual(std::wstring(L"ถัดไป"), o[2].label);
    }

    TEST_METHOD(ParsesTwoOptionsOnOneLine)
    {
        auto o = ParseUssdMenu(L"ยืนยันสมัคร?\n1. ยืนยัน  2. ยกเลิก");
        Assert::AreEqual(2, static_cast<int>(o.size()));
        Assert::AreEqual(std::wstring(L"ยืนยัน"), o[0].label);
        Assert::AreEqual(std::wstring(L"2"), o[1].key);
    }

    TEST_METHOD(ParsesParenthesisStyleWithCrLf)
    {
        auto o = ParseUssdMenu(L"Confirm?\r\n1) Yes\r\n2) No");
        Assert::AreEqual(2, static_cast<int>(o.size()));
        Assert::AreEqual(std::wstring(L"No"), o[1].label);
    }

    TEST_METHOD(RealBalanceReplyIsNotAMenu)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"ยอดคงเหลือ47.79บ.ใช้ได้ถึง12/10/70\nซื้อโปร โทร*103#").size()));
    }

    TEST_METHOD(RealProcessingReplyIsNotAMenu)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"ระบบกำลังดำเนินการ กรุณารอรับ SMS ยืนยันการทำรายการค่ะ").size()));
    }

    TEST_METHOD(SingleOptionIsNotAMenu)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"1. OK").size()));
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"10 บาท").size()));
    }

    TEST_METHOD(DecimalsAreNotOptions)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"1.5GB โปรเสริม\n2.5GB โปรเสริม").size()));
    }

    TEST_METHOD(ThreeDigitNumbersAreNotOptions)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"100 บาท\n200 บาท").size()));
    }

    TEST_METHOD(DatesAreNotOptions)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"12/10/70 หมดอายุ\n13/10/70 หมดอายุ").size()));
    }

    TEST_METHOD(EmptyTextGivesNoOptions)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"").size()));
    }
};
