#include <ctime>
#include <string>
#include <sstream>
#include <iomanip>
#include "datetime.h"
#include <chrono>

#include "nlohmann/json.hpp"
using json = nlohmann::json;

using namespace std;
using namespace datetime;

const TimePoint kTimePointZero = TimePoint();

static constexpr const char *ISO8601_FORMAT_STRING = "%FT%TZ";

// NOTE: Limited to GMT as "Z"
static constexpr const char *ISO8601_PARSE_STRING = "%Y-%m-%dT%H:%M:%SZ";

// NOTE: Limited to GMT
static constexpr const char *RFC7231_PARSE_STRING = "%a, %d %b %Y %H:%M:%S GMT"; // Wed, 03 Oct 2018 18:41:43 GMT

#ifdef _MSC_VER
#define timegm _mkgmtime
#endif

// TimePoints with different duration resolutions won't compare in an expected way, and can make
// testing and reasoning difficult. We'll make sure that all TimePoints that we produce use the
// same Duration.
datetime::TimePoint NormalizeTimePoint(const datetime::Clock::time_point& tp) {
  return chrono::time_point_cast<datetime::Duration>(tp);
}

tm TimePointToTm(const datetime::TimePoint& tp) {
  time_t tt = datetime::Clock::to_time_t(tp);
  return *gmtime(&tt);
}

datetime::TimePoint TmToTimePoint(tm t) {
  time_t tt = timegm(&t);
  return NormalizeTimePoint(datetime::Clock::from_time_t(tt));
}

/* Not needed at this time
tm TmNow() {
    time_t t = time(nullptr);
    tm tm = *gmtime(&t);
    return tm;
}
*/

string ToISO8601(const tm& t) {
  ostringstream ss;
  ss << put_time(&t, ISO8601_FORMAT_STRING);
  return ss.str();
}

bool FromString(const char *parseSpecifier, const string& s, tm& t) {
  tm temp = {};
  istringstream ss(s);
  ss >> get_time(&temp, parseSpecifier);
  if (ss.fail())
  {
    // Parse failed.
    return false;
  }

  t = temp;

  return true;
}

bool FromISO8601(const string& s, tm& t) {
  return FromString(ISO8601_PARSE_STRING, s, t);
}

bool FromRFC7231(const string& s, tm& t) {
  return FromString(RFC7231_PARSE_STRING, s, t);
}

DateTime::DateTime()
  : DateTime(kTimePointZero) {
}

DateTime::DateTime(const DateTime& src)
  : DateTime(src.time_point_) {
}

DateTime::DateTime(const TimePoint& src)
    : time_point_(src) {
}

DateTime DateTime::Zero() {
  return kTimePointZero;
}

DateTime DateTime::Now() {
  return NormalizeTimePoint(Clock::now());
}

bool DateTime::IsZero() const {
  // This makes the assumption that we won't be dealing with 1970-01-01 as a legit date.
  //return time_point_.time_since_epoch().count() == 0;
  return time_point_ == kTimePointZero;
}

string DateTime::ToISO8601() const {
  return ::ToISO8601(TimePointToTm(time_point_));
}

bool DateTime::FromISO8601(const string& s) {
  tm temp = {};
  if (!::FromISO8601(s, temp))
  {
    return false;
  }

  time_point_ = TmToTimePoint(temp);
  return true;
}

bool DateTime::FromRFC7231(const string& s) {
  tm temp = {};
  if (!::FromRFC7231(s, temp))
  {
    return false;
  }

  time_point_ = TmToTimePoint(temp);
  return true;
}

Duration DateTime::Diff(const DateTime& other) const {
  auto d = time_point_ - other.time_point_;
  return chrono::duration_cast<Duration>(d);
}

DateTime DateTime::Add(const Duration& d) const {
  return NormalizeTimePoint(time_point_ + d);
}

DateTime DateTime::Sub(const Duration& d) const {
  return NormalizeTimePoint(time_point_ - d);
}

bool DateTime::operator<(const DateTime& rhs) const {
  return time_point_ < rhs.time_point_;
}

bool DateTime::operator>(const DateTime& rhs) const {
  return time_point_ > rhs.time_point_;
}

namespace datetime {
bool operator==(const DateTime& lhs, const DateTime& rhs) {
  return lhs.time_point_ == rhs.time_point_;
}
void to_json(json& j, const DateTime& dt) {
  int64_t ticks = dt.time_point_.time_since_epoch().count();
  j = ticks;
}

void from_json(const json& j, DateTime& dt) {
  auto ticks = j.get<int64_t>();
  dt.time_point_ = TimePoint(Duration(ticks));
}

int64_t DurationToInt64(const Duration& d) {
  return d.count();
}

Duration DurationFromInt64(const int64_t d) {
  return Duration(d);
}
} // namespace datetime
