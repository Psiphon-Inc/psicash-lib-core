#ifndef PSICASHLIB_DATETIME_H
#define PSICASHLIB_DATETIME_H

#include "vendor/nlohmann/json.hpp"

namespace datetime
{
using Duration = std::chrono::milliseconds; // millisecond-resolution duration
using Clock = std::chrono::system_clock;
using TimePoint = std::chrono::time_point<Clock, Duration>;

class DateTime {
public:
  // By default, initializes to the "zero" value.
  DateTime();
  DateTime(const DateTime& src);
  DateTime(const TimePoint& src);

  static DateTime Zero();

  static DateTime Now();

  // Returns true if this DateTime is the zero value.
  bool IsZero() const;

  // These only support the "Z" timezone format.
  std::string ToISO8601() const;
  bool FromISO8601(const std::string &s);

  // Parses the HTTP Date header format
  bool FromRFC7231(const std::string &);

  Duration Diff(const DateTime &other) const;
  DateTime Add(const Duration &d) const;
  DateTime Sub(const Duration &d) const;

  bool operator<(const DateTime& rhs) const;
  bool operator>(const DateTime& rhs) const;

  friend bool operator==(const DateTime& lhs, const DateTime& rhs);

  friend void to_json(nlohmann::json& j, const DateTime& dt);
  friend void from_json(const nlohmann::json& j, DateTime& dt);

private:
  TimePoint time_point_;
};


// These are intended to help de/serialization of duration values.
int64_t DurationToInt64(const Duration &d);
Duration DurationFromInt64(const int64_t d);
} // namespace datetime

#endif //PSICASHLIB_DATETIME_H
