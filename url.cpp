#include <string>
#include <regex>
#include <iomanip>
#include "url.h"


using namespace std;

namespace psicash {

error::Error URL::Parse(const string& s) {
  regex url_regex("^(https?://[^?#]+)(\\?[^#]*)?(#.*)?$", regex_constants::ECMAScript);

  constexpr const int required_groups = 4; // 1 for the whole string and 3 for the capture groups
  constexpr const int scheme_host_path_group = 1;
  constexpr const int query_group = 2;
  constexpr const int fragment_group = 3;

  smatch match_pieces;
  if (!regex_match(s, match_pieces, url_regex)) {
    return MakeError("regex_match failed");
  }

  if (match_pieces.size() != required_groups) {
    return MakeError("incorrect regex_match pieces count");
  }

  scheme_host_path_ = match_pieces[scheme_host_path_group].str();
  query_ = match_pieces[query_group].str();
  fragment_ = match_pieces[fragment_group].str();

  // Strip the leading '?'
  if (query_.size() > 0 && query_[0] == '?') {
    query_ = query_.substr(1);
  }

  // Strip the leading '#'
  if (fragment_.size() > 0 && fragment_[0] == '#') {
    fragment_ = fragment_.substr(1);
  }

  return error::nullerr;
}

string URL::ToString() const {
  ostringstream out;
  out << scheme_host_path_;

  if (!query_.empty()) {
    out << "?" << query_;
  }

  if (!fragment_.empty()) {
    out << "#" << fragment_;
  }

  return out.str();
}

// Adapted from https://stackoverflow.com/a/17708801/729729
string URL::Encode(const string& s, bool full) {
  ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (string::const_iterator i = s.begin(); i != s.end(); ++i) {
    string::value_type c = (*i);

    // Keep alphanumeric and other accepted characters intact
    if (!full && (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')) {
      escaped << c;
      continue;
    }

    // Any other characters are percent-encoded
    escaped << std::uppercase;
    escaped << '%' << std::setw(2) << int((unsigned char) c);
    escaped << std::nouppercase;
  }

  return escaped.str();
}

} // namespace psicash