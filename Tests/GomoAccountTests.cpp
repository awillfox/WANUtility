#include "CppUnitTest.h"
#include "GomoAccount.h"
#include <winrt/base.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

namespace
{
    // Invented fixtures: no subscriber IDs, captured tokens or personal account data.
    constexpr auto usageFixture = LR"({"statusCode":200,"data":{
        "voice":{"remaining":540,"unit":"SECOND"},
        "displayVoice":{"remaining":"9","max":"20","unit":{"en":"mins","th":"นาที"}},
        "displayInternet":{"remaining":"8.75","max":"10","unit":{"en":"GB","th":"GB"}}
    }})";
    constexpr auto emptyPackages = LR"({"statusCode":200,"data":{"packages":[]}})";
    constexpr auto packages = LR"({"statusCode":200,"data":{"packages":[{
        "statusId":0,"startDate":"2026-01-02T03:04:05.000Z","endDate":"2027-01-02T03:04:05.000Z",
        "packageId":{
            "displayName":{"en":"Example plan","th":"แพ็กตัวอย่าง"},
            "detail":{"en":"Internet &amp; calls"},
            "dataVolume":{"value":"10","unit":"GB"},"speed":{"value":"100","unit":"Mbps"},
            "dataBalance":{"remain":10,"unit":"GB"},
            "remainingFreeUnit":{
                "detailInternet":[{"android":{"en":"<big><b>8.74 GB</b></big> remaining from 10 GB<br/>"}}],
                "detailVoice":[{"android":{"en":"<b>9 mins</b> remaining from 20 mins"}}]
            }
        }
    }]}})";
}

TEST_CLASS(GomoAccountTests)
{
public:
    TEST_METHOD_INITIALIZE(Initialize) { winrt::init_apartment(winrt::apartment_type::single_threaded); }
    TEST_METHOD_CLEANUP(Cleanup) { winrt::uninit_apartment(); }

    TEST_METHOD(UsesDisplayedMinutesInsteadOfRawSeconds)
    {
        auto a = ParseGomoAccount(usageFixture, emptyPackages);
        Assert::AreEqual(std::wstring(L"9 mins"), a.calls.remaining);
        Assert::AreEqual(std::wstring(L"8.75 GB"), a.data.remaining);
        Assert::AreEqual(std::wstring(L"20 mins"), a.calls.maximum);
    }

    TEST_METHOD(RendersPackageBalancesWithoutHtmlOrRoundingReconciliation)
    {
        auto a = ParseGomoAccount(usageFixture, packages);
        Assert::AreEqual(size_t(1), a.packages.size());
        auto const& p = a.packages[0];
        Assert::AreEqual(std::wstring(L"Example plan"), p.name);
        Assert::AreEqual(std::wstring(L"Internet & calls"), p.description);
        Assert::AreEqual(std::wstring(L"10 GB"), p.allowance);
        Assert::AreEqual(std::wstring(L"100 Mbps"), p.speed);
        Assert::AreEqual(std::wstring(L"Active"), p.status);
        Assert::AreEqual(std::wstring(L"2027-01-02T03:04:05.000Z"), p.endDate);
        Assert::AreEqual(size_t(2), p.remaining.size());
        Assert::AreEqual(std::wstring(L"Data: 8.74 GB remaining from 10 GB"), p.remaining[0]);
        Assert::AreEqual(std::wstring(L"Calls: 9 mins remaining from 20 mins"), p.remaining[1]);
    }

    TEST_METHOD(FallsBackToThaiAndKeepsLegitimateZero)
    {
        auto a = ParseGomoAccount(LR"({"statusCode":200,"data":{
            "displayVoice":{"remaining":"0","unit":{"th":"นาที"}},
            "displayInternet":{"remaining":"0","unit":{"en":"GB"}}
        }})", emptyPackages);
        Assert::AreEqual(std::wstring(L"0 นาที"), a.calls.remaining);
        Assert::AreEqual(std::wstring(L"0 GB"), a.data.remaining);
        Assert::IsTrue(a.packages.empty());
    }

    TEST_METHOD(MissingTotalsAreNotDisplayedAsZero)
    {
        Assert::ExpectException<GomoError>([] { ParseGomoAccount(LR"({"statusCode":200,"data":{}})", emptyPackages); });
        Assert::ExpectException<GomoError>([] { ParseGomoAccount(LR"({"statusCode":200,"data":{"displayVoice":{"remaining":""}}})", emptyPackages); });
    }

    TEST_METHOD(RejectsMalformedFailedAndIncompleteResponses)
    {
        for (auto bad : { L"<html>login</html>", L"{}", LR"({"statusCode":500,"data":{}})", LR"({"statusCode":200,"data":null})" })
            Assert::ExpectException<GomoError>([&] { ParseGomoAccount(bad, emptyPackages); });
        Assert::ExpectException<GomoError>([] { ParseGomoAccount(usageFixture, LR"({"statusCode":200,"data":{}})"); });
        Assert::ExpectException<GomoError>([] { ParseGomoAccount(usageFixture, LR"({"statusCode":200,"data":{"packages":[{}]}})"); });
    }

    TEST_METHOD(RequiresSuccessfulUnredirectedLoginAndAHeaderSafeToken)
    {
        Assert::AreEqual(std::wstring(L"synthetic.test.token"), ParseGomoLogin(LR"({"statusCode":200,"rocketToken":"synthetic.test.token","shouldRedirect":false})"));
        for (auto bad : {
            LR"({"statusCode":401})", LR"({"statusCode":200})",
            LR"({"statusCode":200,"rocketToken":"synthetic","shouldRedirect":true})",
            LR"({"statusCode":200,"rocketToken":"synthetic","shouldOpenWithDeviceBrowser":true})",
            LR"({"statusCode":200,"rocketToken":"synthetic\r\nInjected: yes"})" })
            Assert::ExpectException<GomoError>([&] { ParseGomoLogin(bad); });
    }

    TEST_METHOD(BlockedAccountIsDistinguishedFromUnrecognizedLogin)
    {
        // Login status 4 was seen on a working account; only the myAISBlocked object means blocked.
        Assert::AreEqual(std::wstring(L"t"), ParseGomoLogin(LR"({"statusCode":200,"statusId":4,"rocketToken":"t","myAISBlocked":null})"));
        try { ParseGomoLogin(LR"({"statusCode":200,"rocketToken":"t","myAISBlocked":{"status":"x","message":"m"}})"); Assert::Fail(); }
        catch (GomoError const& e) { Assert::IsTrue(e.reason == GomoFailure::Blocked); }
    }

    TEST_METHOD(CarrierHtmlBecomesPlainTextWithoutMarkupOrControls)
    {
        auto a = ParseGomoAccount(usageFixture, LR"({"statusCode":200,"data":{"packages":[{"statusId":9,"packageId":{
            "displayName":{"en":"<font color=#F64EA5>A</font>&nbsp;&lt;B&gt; &#3588;&#x0E07; &unknown; x<y"},
            "detail":{"en":"<span style=\"x\">one</span><br/>two\u0007 <script>bad()</script>"}}}]}})");
        auto const& p = a.packages[0];
        Assert::AreEqual(std::wstring(L"A <B> \u0E04\u0E07 &unknown; x<y"), p.name);
        Assert::AreEqual(std::wstring(L"one two bad()"), p.description);
        Assert::AreEqual(std::wstring(L"Status unavailable"), p.status);
        Assert::IsTrue(p.remaining.empty());
    }

    TEST_METHOD(NullOptionalPackageFieldsAreLeftEmpty)
    {
        auto a = ParseGomoAccount(usageFixture, LR"({"statusCode":200,"data":{"packages":[{"statusId":0,"startDate":null,"packageId":{
            "displayName":{"en":"Plan"},"detail":null,"smsName":null,"speed":null,"dataVolume":null,"dataBalance":null,
            "remainingFreeUnit":{"detailInternet":null,"detailVoice":[null,{"android":null,"ios":{"en":"5 mins"}}]}}}]}})");
        auto const& p = a.packages[0];
        Assert::AreEqual(std::wstring(L"Plan"), p.name);
        Assert::IsTrue(p.description.empty() && p.speed.empty() && p.allowance.empty() && p.startDate.empty());
        Assert::AreEqual(size_t(1), p.remaining.size());
        Assert::AreEqual(std::wstring(L"Calls: 5 mins"), p.remaining[0]);
    }

    TEST_METHOD(DatesOutsideTheLocalTimeRangeAreShownAsSent)
    {
        Assert::AreEqual(std::wstring(L"0001-01-01T00:00:00.000Z"), FormatGomoDate(L"0001-01-01T00:00:00.000Z"));
        Assert::AreEqual(std::wstring(L"soon"), FormatGomoDate(L"soon"));
        Assert::AreEqual(size_t(16), FormatGomoDate(L"2026-10-20T11:09:15.350Z").size()); // dd/mm/yyyy HH:MM, local
    }

    TEST_METHOD(OnlyAisCarrierNamesEnableGomo)
    {
        for (auto name : { L"AIS", L"ais", L"AIS 5G", L"AIS-T" })
            Assert::IsTrue(IsAisCarrier(name), name);
        for (auto name : { L"", L"dtac.Better Together", L"dtac TriNet", L"TRUE-H", L"AISLE", L"MyAIS" })
            Assert::IsFalse(IsAisCarrier(name), name);
    }

    TEST_METHOD(ExpiredSessionIsDistinguishedFromMalformedData)
    {
        try { CheckGomoResponse(LR"({"statusCode":401})"); Assert::Fail(); }
        catch (GomoError const& e) { Assert::IsTrue(e.reason == GomoFailure::SessionExpired); }
    }
};
