/*
 * Copyright (c) 2018, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <ctime>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <locale>
#include "datetime.hpp"
#include "vendor/date/date.h"
#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

using namespace std;
using namespace datetime;

const TimePoint kTimePointZero = TimePoint();

static constexpr const char* ISO8601_FORMAT_STRING = "%FT%TZ";

static constexpr const char* ISO8601_PARSE_STRING = "%FT%T%Z";

// NOTE: Limited to GMT
static constexpr const char* RFC7231_PARSE_STRING = "%a, %d %b %Y %T %Z"; // Wed, 03 Oct 2018 18:41:43 GMT

#ifdef _MSC_VER
#define timegm _mkgmtime
#endif

// TimePoints with different duration resolutions won't compare in an expected way, and can make
// testing and reasoning difficult. We'll make sure that all TimePoints that we produce use the
// same Duration.
datetime::TimePoint NormalizeTimePoint(const datetime::Clock::time_point& tp) {
    return chrono::time_point_cast<datetime::Duration>(tp);
}

bool FromString(const char* parseSpecifier, const string& s, TimePoint& tp) {
    TimePoint temp;
    istringstream ss(s);
    ss.imbue(std::locale::classic());

    ss >> date::parse(parseSpecifier, temp);
    if (ss.fail()) {
        // Parse failed.
        return false;
    }

    tp = temp;

    return true;
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
    ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << date::format(ISO8601_FORMAT_STRING, time_point_);
    return ss.str();
}

bool DateTime::FromISO8601(const string& s) {
    TimePoint temp;
    if (!FromString(ISO8601_PARSE_STRING, s, temp)) {
        return false;
    }

    time_point_ = NormalizeTimePoint(temp);
    return true;
}

bool DateTime::FromRFC7231(const string& s) {
    TimePoint temp;
    if (!FromString(RFC7231_PARSE_STRING, s, temp)) {
        return false;
    }

    time_point_ = NormalizeTimePoint(temp);
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

int64_t DateTime::MillisSinceEpoch() const {
    return time_point_.time_since_epoch().count();
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
    j = dt.ToISO8601();
}

void from_json(const json& j, DateTime& dt) {
    dt.FromISO8601(j.get<string>());
}

int64_t DurationToInt64(const Duration& d) {
    return d.count();
}

Duration DurationFromInt64(const int64_t d) {
    return Duration(d);
}
} // namespace datetime
