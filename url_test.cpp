#include "gtest/gtest.h"
#include "url.hpp"

using namespace std;
using namespace psicash;

TEST(TestURL, Parse)
{
  psicash::URL url;

  auto err = url.Parse("https://sfd.sdaf.fdsk:123/fdjirn/dsf/df?adf=sdf&daf=asdf#djlifd");
  ASSERT_FALSE(err);
  ASSERT_EQ(url.scheme_host_path_, "https://sfd.sdaf.fdsk:123/fdjirn/dsf/df");
  ASSERT_EQ(url.query_, "adf=sdf&daf=asdf");
  ASSERT_EQ(url.fragment_, "djlifd");

  err = url.Parse("https://sfd.sdaf.fdsk/fdjirn/dsf/df?adf=sdf&daf=asdf#djlifd");
  ASSERT_FALSE(err);
  ASSERT_EQ(url.scheme_host_path_, "https://sfd.sdaf.fdsk/fdjirn/dsf/df");
  ASSERT_EQ(url.query_, "adf=sdf&daf=asdf");
  ASSERT_EQ(url.fragment_, "djlifd");

  err = url.Parse("http://sfd.sdaf.fdsk/fdjirn/dsf/df?adf=sdf&daf=asdf#djlifd");
  ASSERT_FALSE(err);
  ASSERT_EQ(url.scheme_host_path_, "http://sfd.sdaf.fdsk/fdjirn/dsf/df");
  ASSERT_EQ(url.query_, "adf=sdf&daf=asdf");
  ASSERT_EQ(url.fragment_, "djlifd");

  err = url.Parse("http://sfd.sdaf.fdsk/fdjirn/dsf/df?adf=sdf&daf=asdf");
  ASSERT_FALSE(err);
  ASSERT_EQ(url.scheme_host_path_, "http://sfd.sdaf.fdsk/fdjirn/dsf/df");
  ASSERT_EQ(url.query_, "adf=sdf&daf=asdf");
  ASSERT_EQ(url.fragment_, "");

  err = url.Parse("http://sfd.sdaf.fdsk/fdjirn/dsf/df#djlifd");
  ASSERT_FALSE(err);
  ASSERT_EQ(url.scheme_host_path_, "http://sfd.sdaf.fdsk/fdjirn/dsf/df");
  ASSERT_EQ(url.query_, "");
  ASSERT_EQ(url.fragment_, "djlifd");

  err = url.Parse("http://sfd.sdaf.fdsk/fdjirn/dsf/df");
  ASSERT_FALSE(err);
  ASSERT_EQ(url.scheme_host_path_, "http://sfd.sdaf.fdsk/fdjirn/dsf/df");
  ASSERT_EQ(url.query_, "");
  ASSERT_EQ(url.fragment_, "");

  err = url.Parse("http://sfd.sdaf.fdsk/");
  ASSERT_FALSE(err);
  ASSERT_EQ(url.scheme_host_path_, "http://sfd.sdaf.fdsk/");
  ASSERT_EQ(url.query_, "");
  ASSERT_EQ(url.fragment_, "");

  err = url.Parse("https://sfd.sdaf.fdsk?adf=sdf&daf=asdf#djlifd");
  ASSERT_FALSE(err);
  ASSERT_EQ(url.scheme_host_path_, "https://sfd.sdaf.fdsk");
  ASSERT_EQ(url.query_, "adf=sdf&daf=asdf");
  ASSERT_EQ(url.fragment_, "djlifd");
}

TEST(TestURL, ParseError)
{
  URL url;

  auto err = url.Parse("NOT! A! URL!");
  ASSERT_TRUE(err);
}

TEST(TestURL, ToString)
{
  URL url;

  url = {"https://adsf.asdf.df", "", ""};
  auto s = url.ToString();
  ASSERT_EQ(s, "https://adsf.asdf.df");

  url = {"https://adsf.asdf.df", "asdf&qer=asdf", "qwer"};
  s = url.ToString();
  ASSERT_EQ(s, "https://adsf.asdf.df?asdf&qer=asdf#qwer");

  url = {"https://adsf.asdf.df", "asdf&qer=asdf", ""};
  s = url.ToString();
  ASSERT_EQ(s, "https://adsf.asdf.df?asdf&qer=asdf");

  url = {"https://adsf.asdf.df", "", "qwer"};
  s = url.ToString();
  ASSERT_EQ(s, "https://adsf.asdf.df#qwer");

  url = {"https://adsf.asdf.df", "a%25z", "%7B%22%6B%31%22%3A%20%22%76%22%2C%20%22%6B%32%22%3A%20%31%32%33%7D"};
  s = url.ToString();
  ASSERT_EQ(s, "https://adsf.asdf.df?a%25z#%7B%22%6B%31%22%3A%20%22%76%22%2C%20%22%6B%32%22%3A%20%31%32%33%7D");
}

TEST(TestURL, EncodeNotFull)
{
  auto enc = URL::Encode("", false);
  ASSERT_EQ(enc, "");

  enc = URL::Encode("abc", false);
  ASSERT_EQ(enc, "abc");

  enc = URL::Encode("Q!W@E#R$T%Y^U&I*O(P)", false);
  ASSERT_EQ(enc, "Q%21W%40E%23R%24T%25Y%5EU%26I%2AO%28P%29");

  enc = URL::Encode("a%z", false);
  ASSERT_EQ(enc, "a%25z");

  enc = URL::Encode("{\"k1\": \"v\", \"k2\": 123}", false);
  ASSERT_EQ(enc, "%7B%22k1%22%3A%20%22v%22%2C%20%22k2%22%3A%20123%7D");
}

TEST(TestURL, EncodeFull)
{
  auto enc = URL::Encode("", true);
  ASSERT_EQ(enc, "");

  enc = URL::Encode("abc", true);
  ASSERT_EQ(enc, "%61%62%63");

  enc = URL::Encode("Q!W@E#R$T%Y^U&I*O(P)", true);
  ASSERT_EQ(enc, "%51%21%57%40%45%23%52%24%54%25%59%5E%55%26%49%2A%4F%28%50%29");

  enc = URL::Encode("a%z", true);
  ASSERT_EQ(enc, "%61%25%7A");

  enc = URL::Encode("{\"k1\": \"v\", \"k2\": 123}", true);
  ASSERT_EQ(enc, "%7B%22%6B%31%22%3A%20%22%76%22%2C%20%22%6B%32%22%3A%20%31%32%33%7D");
}
