#include "CppUnitTest.h"
#include "PhoneNumber.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

TEST_CLASS(PhoneNumberTests)
{
public:
    TEST_METHOD(ThaiLocalAndInternationalFormsShareAKey)
    {
        const std::wstring key = L"+66889605613";
        Assert::AreEqual(key, ThreadKeyFor(L"0889605613"));
        Assert::AreEqual(key, ThreadKeyFor(L"+66889605613"));
        Assert::AreEqual(key, ThreadKeyFor(L"66889605613"));
        Assert::AreEqual(key, ThreadKeyFor(L"088-960-5613"));
        Assert::AreEqual(key, ThreadKeyFor(L" 088 960 5613 "));
        Assert::AreEqual(key, ThreadKeyFor(L"+66 (88) 960.5613"));
    }

    TEST_METHOD(NineDigitLandlineGetsCountryCode)
    {
        Assert::AreEqual(std::wstring(L"+6621234567"), ThreadKeyFor(L"021234567"));
    }

    TEST_METHOD(AlphanumericSendersAreVerbatim)
    {
        Assert::AreEqual(std::wstring(L"dtac"), ThreadKeyFor(L"dtac"));
        Assert::AreEqual(std::wstring(L"dtacOnline"), ThreadKeyFor(L"  dtacOnline "));
    }

    TEST_METHOD(ShortCodesStayAsDigits)
    {
        Assert::AreEqual(std::wstring(L"1678"), ThreadKeyFor(L"1678"));
    }

    TEST_METHOD(BlankAddressGivesEmptyKey)
    {
        Assert::AreEqual(std::wstring(), ThreadKeyFor(L"   "));
    }

    TEST_METHOD(PhoneNumberKeyDetection)
    {
        Assert::IsTrue(IsPhoneNumberKey(L"+66889605613"));
        Assert::IsTrue(IsPhoneNumberKey(L"1678"));
        Assert::IsFalse(IsPhoneNumberKey(L"dtac"));
        Assert::IsFalse(IsPhoneNumberKey(L"+"));
        Assert::IsFalse(IsPhoneNumberKey(L""));
    }
};
