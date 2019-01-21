#include <chrono>
#include <thread>

#include "gtest/gtest.h"
#include "datetime.hpp"
#include "vendor/date/date.h"
#include "vendor/nlohmann/json.hpp"
using json = nlohmann::json;

using namespace std;
using namespace datetime;

TEST(TestDatetime, Construction)
{
  DateTime dt1;
  ASSERT_TRUE(dt1.IsZero());

  auto dt2 = DateTime::Now();
  DateTime dt3(dt2);
  ASSERT_EQ(dt2, dt3);
  DateTime dt4 = dt2;
  ASSERT_EQ(dt2, dt4);
}

TEST(TestDatetime, IsZero)
{
  DateTime dt;
  ASSERT_TRUE(DateTime::Zero().IsZero());
  ASSERT_TRUE(DateTime().IsZero());
  ASSERT_EQ(DateTime::Zero(), DateTime::Zero());
  ASSERT_EQ(DateTime(), DateTime());

  ASSERT_FALSE(DateTime::Now().IsZero());
}

TEST(TestDatetime, Now)
{
  auto now1 = DateTime::Now();

  this_thread::sleep_for(chrono::milliseconds(10));

  auto now2 = DateTime::Now();

  auto diff_dur = now2.Diff(now1);
  auto diff_ms = datetime::DurationToInt64(diff_dur);

  ASSERT_GT(diff_ms, 0);
  ASSERT_LT(diff_ms, 100);
}

TEST(TestDatetime, ToISO8601)
{
  auto s = "2001-01-01T01:01:01.000Z";

  DateTime dt;
  auto ok = dt.FromISO8601(s);
  ASSERT_TRUE(ok);

  auto formatted = dt.ToISO8601();
  ASSERT_EQ(s, formatted);
}

TEST(TestDatetime, FromISO8601)
{
  // It's difficult to test this without relying on the parsing and formatting...

  // Low-precision time strings
  auto twothousandone = "2001-01-01T01:01:01Z";
  auto twothousandtwo = "2002-01-01T01:01:01Z";

  DateTime dt1;
  auto ok = dt1.FromISO8601(twothousandone);
  ASSERT_TRUE(ok);

  DateTime dt2;
  ok = dt2.FromISO8601(twothousandtwo);
  ASSERT_TRUE(ok);

  auto diff = dt2.Diff(dt1);
  auto year_of_millis = 1000LL*60*60*24*365;
  ASSERT_EQ(diff.count(), year_of_millis);

  // High precision times, with tenth-second difference
  auto oct14 = "2018-10-14T01:24:13.62396488Z";
  auto oct15 = "2018-10-15T01:24:13.72396488Z";

  DateTime dt3;
  ok = dt3.FromISO8601(oct14);
  ASSERT_TRUE(ok);

  DateTime dt4;
  ok = dt4.FromISO8601(oct15);
  ASSERT_TRUE(ok);

  diff = dt4.Diff(dt3);
  auto want_millis_diff = 1000LL*60*60*24 + 100;  // 100 for the tenth-second

  ASSERT_EQ(diff.count(), want_millis_diff);
}

TEST(TestDatetime, FromISO8601BadInput)
{
  auto s = "incorrect string here";

  DateTime dt;
  auto ok = dt.FromISO8601(s);
  ASSERT_FALSE(ok);
}

TEST(TestDatetime, FromRFC7231)
{
  auto rfc = "Wed, 03 Oct 2018 18:41:43 GMT";
  auto iso = "2018-10-03T18:41:43Z";

  DateTime dtISO, dtRFC;
  auto ok = dtRFC.FromRFC7231(rfc);
  ASSERT_TRUE(ok);
  ok = dtISO.FromISO8601(iso);
  ASSERT_TRUE(ok);

  ASSERT_EQ(dtISO, dtRFC);
  ASSERT_EQ(dtISO.ToISO8601(), dtRFC.ToISO8601());
}

TEST(TestDatetime, FromRFC7231BadInput)
{
  auto s = "incorrect string here";

  DateTime dt;
  auto ok = dt.FromRFC7231(s);
  ASSERT_FALSE(ok);
}

TEST(TestDatetime, Diff)
{
  auto now1 = DateTime::Now();

  this_thread::sleep_for(chrono::milliseconds(10));

  auto now2 = DateTime::Now();

  // now2 - now1
  auto diff_dur1 = now2.Diff(now1);
  auto diff_ms1 = datetime::DurationToInt64(diff_dur1);
  ASSERT_GT(diff_ms1, 0);
  ASSERT_LT(diff_ms1, 100);

  // now1 - now2
  auto diff_dur2 = now1.Diff(now2);
  auto diff_ms2 = datetime::DurationToInt64(diff_dur2);
  ASSERT_LT(diff_ms2, 0);
  ASSERT_GT(diff_ms2, -100);

  // x - x
  auto diff_dur3 = now1.Diff(now1);
  auto diff_ms3 = datetime::DurationToInt64(diff_dur3);
  ASSERT_EQ(diff_ms3, 0);
}

TEST(TestDatetime, Add)
{
  auto now1 = DateTime::Now();

  this_thread::sleep_for(chrono::milliseconds(10));

  auto now2 = DateTime::Now();

  auto diff_dur = now2.Diff(now1);
  auto diff_ms = datetime::DurationToInt64(diff_dur);
  ASSERT_GT(diff_ms, 0);
  ASSERT_LT(diff_ms, 100);

  auto res = now1.Add(diff_dur);
  ASSERT_EQ(now2, res);

  res = now2.Add(-diff_dur);
  ASSERT_EQ(now1, res);
}

TEST(TestDatetime, Sub)
{
  auto now1 = DateTime::Now();

  this_thread::sleep_for(chrono::milliseconds(10));

  auto now2 = DateTime::Now();

  auto diff_dur = now2.Diff(now1);
  auto diff_ms = datetime::DurationToInt64(diff_dur);
  ASSERT_GT(diff_ms, 0);
  ASSERT_LT(diff_ms, 100);

  auto res = now2.Sub(diff_dur);
  ASSERT_EQ(now1, res);

  res = now1.Sub(-diff_dur);
  ASSERT_EQ(now2, res);
}

TEST(TestDatetime, MillisSinceEpoch)
{
  auto now = DateTime::Now();
  ASSERT_GT(now.MillisSinceEpoch(), 1261440000000LL);

  auto later = now.Add(datetime::Duration(12345));
  ASSERT_EQ(later.MillisSinceEpoch(), now.MillisSinceEpoch()+12345);
}

TEST(TestDatetime, DurationToInt64)
{
  int64_t ms = 123456;
  auto d = datetime::Duration(ms);
  ASSERT_EQ(datetime::DurationToInt64(d), ms);
}

TEST(TestDatetime, DurationFromInt64)
{
  int64_t ms = 123456;
  auto exp = datetime::Duration(ms);
  auto got = datetime::DurationFromInt64(ms);
  ASSERT_EQ(exp, got);
}

TEST(TestDatetime, TimePointComparison)
{
  auto tp1 = DateTime::Now();
  auto tp2 = tp1;
  ASSERT_TRUE(tp1 == tp2);

  auto s = "2001-01-01T01:01:01Z";
  auto ok = tp1.FromISO8601(s);
  ASSERT_TRUE(ok);
  ok = tp2.FromISO8601(s);
  ASSERT_TRUE(ok);
  ASSERT_TRUE(tp1 == tp2);

  ASSERT_FALSE(tp1 < tp2);
  ASSERT_FALSE(tp1 > tp2);

  auto earlier = DateTime::Zero();
  auto later = DateTime::Now();
  ASSERT_TRUE(earlier < later);
  ASSERT_FALSE(later < earlier);
  ASSERT_TRUE(later > earlier);
  ASSERT_FALSE(earlier > later);
}

TEST(TestDatetime, JSON)
{
  auto dt = DateTime::Now();
  json j = dt;
  auto js = j.dump();
  j = json::parse(js);
  auto res = j.get<DateTime>();
  ASSERT_EQ(dt, res);

  dt = DateTime::Zero();
  j = dt;
  js = j.dump();
  j = json::parse(js);
  res = j.get<DateTime>();
  ASSERT_EQ(dt, res);
}
