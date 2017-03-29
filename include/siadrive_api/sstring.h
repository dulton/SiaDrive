#ifndef _SSTRING_H
#define _SSTRING_H

#include <algorithm>
#include <vector>
#include <locale>
#include <sstream>
#include <istream>
#include <ostream>
#include <unordered_map>

class SIADRIVE_EXPORTABLE SString
{
public:
#ifdef _UNICODE

  typedef wchar_t SChar;

#else
  typedef char SChar;
#endif
  typedef std::basic_string<SChar> String;

  typedef std::vector<SString> SStringVector;

  typedef std::basic_stringstream<SChar> SStringStream;

  typedef std::basic_istringstream<SChar> ISStringStream;

  typedef std::vector<SChar> SCharVector;

  typedef std::basic_ifstream<SChar> SCharIfStream;

  typedef std::basic_ofstream<SChar> SCharOfStream;

  typedef std::unordered_map<SString, SString> SStringMap;

  typedef String::iterator SIterator;

  typedef String::const_iterator SConstIterator;

  typedef String::reverse_iterator SReverseIterator;

  typedef String::const_reverse_iterator SConstReverseIterator;

private:
  template<class Facet>
  struct deletable_facet :
      Facet
  {
    template<class ...Args>
    deletable_facet(Args &&...args) :
        Facet(std::forward<Args>(args)...)
    {
    }

    ~deletable_facet()
    {
    }
  };

public:
#ifdef _UNICODE
  static std::wstring ActiveString(const std::string& str)
  {
    return std::move(FromUtf8(str));
  }

  static inline std::wstring ActiveString(const std::wstring& str)
  {
    return str;
  }
#else
  static inline std::string ActiveString(const std::wstring& str)
  {
    return std::move(ToUtf8(str));
  }

  static inline std::string ActiveString(const std::string& str)
  {
    return str;
  }
#endif

  static inline std::string ToUtf8(const std::wstring &str)
  {
    std::wstring_convert<deletable_facet<std::codecvt<wchar_t, char, std::mbstate_t>>, wchar_t> conv;
    return conv.to_bytes(str);
  }

  static inline std::wstring FromUtf8(const std::string &str)
  {
    std::wstring_convert<deletable_facet<std::codecvt<wchar_t, char, std::mbstate_t>>, wchar_t> conv;
    return conv.from_bytes(str);
  }

  static inline String &LeftTrim(String &s)
  {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
  }

  static inline String &RightTrim(String &s)
  {
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
  }

  static inline String &Trim(String &s)
  {
    return LeftTrim(RightTrim(s));
  }

  static inline std::int32_t ToInt32(const SString &str)
  {
    return std::stoi(str.str());
  }

  static inline SString FromInt32(const std::int32_t &value)
  {
    return std::move(SString(std::to_string(value)));
  }

  static inline std::uint32_t ToUInt32(const SString &str)
  {
    return std::stoul(str.str());
  }

  static inline SString FromUInt32(const std::uint32_t &value)
  {
    return std::move(SString(std::to_string(value)));
  }

  static inline std::int64_t ToInt64(const SString &str)
  {
    return std::stoi(str.str());
  }

  static inline SString FromInt64(const std::int64_t &value)
  {
    return std::move(SString(std::to_string(value)));
  }

  static inline SString FromUInt64(const std::uint64_t &value)
  {
    return std::move(SString(std::to_string(value)));
  }

  static inline std::uint64_t ToUInt64(const SString &str)
  {
    return std::stoull(str.str());
  }

  static inline std::uint8_t ToUInt8(const SString &str)
  {
    return (std::uint8_t) std::stoi(str.str());
  }

  static inline SString FromUInt8(const std::uint8_t &value)
  {
    return std::move(SString(std::to_string(value)));
  }

  static inline float ToFloat(const SString &str)
  {
    return std::stof(str.str());
  }

  static inline SString FromFloat(const float &value)
  {
    return std::move(SString(std::to_string(value)));
  }

  static inline long ToLong(const SString &str)
  {
    return std::stol(str.str());
  }

  static inline SString FromLong(const long &value)
  {
    return std::move(SString(std::to_string(value)));
  }

  static inline double ToDouble(const SString &str)
  {
    return std::stod(str.str());
  }

  static inline SString FromDouble(const double &value)
  {
    return std::move(SString(std::to_string(value)));
  }

  static bool ToBool(const SString& value)
  {
    SString str = value.ToLowerCopy();

    SString::ISStringStream is(str);
    bool b;
    is >> std::boolalpha >> b;
    return b;
  }

  static inline SString FromBool(const bool& value)
  {
    return (value ? "True" : "False");
  }

  static std::vector<const char*> ToCharArray(const std::vector<std::string>& v)
  {
    std::vector<const char*> ret;
    for (const auto& s : v)
    {
      ret.push_back(&s[0]);
    }
    return std::move(ret);
  }

#ifdef _UNICODE
  static std::vector<std::string> ToUtf8Array(const SStringVector& v)
  {
    std::vector<std::string> ret;
    for (const auto& s : v)
    {
      ret.push_back(ToUtf8(s.str()));
    }
    return std::move(ret);
  }
#else
  static std::vector<std::string> ToUtf8Array(const SStringVector& v)
  {
    std::vector<std::string> ret;
    for (const auto& s : v)
    {
      ret.push_back(s.str());
    }
    return std::move(ret);
  }
#endif

public:
  SString()
  {
  }

  SString(const std::string &str) :
#ifdef _UNICODE
  _str(std::move(SString::FromUtf8(str)))
#else
      _str(str)
#endif
  {
  }

  SString(const char *str) :
#ifdef _UNICODE
  _str(std::move(SString::FromUtf8(str)))
#else
      _str(str)
#endif
  {
  }

  SString(const std::wstring &str) :
#ifdef _UNICODE
  _str(str)
#else
      _str(std::move(SString::ToUtf8(str)))
#endif
  {
  }

  SString(const wchar_t *str) :
#ifdef _UNICODE
  _str(str)
#else
      _str(std::move(SString::ToUtf8(str)))
#endif
  {
  }

  SString(const char &c) :
#ifdef _UNICODE
  _str(1, (wchar_t)c)
#else
  _str(1, c)
#endif
  {
  }

  SString(const wchar_t &c) :
#ifdef _UNICODE
  _str(1, c)
#else
      _str(1, (wchar_t) c)
#endif
  {
  }

  SString(const SString &str) :
      _str(str.str())
  {
  }

  SString(SString &&str)  noexcept :
      _str(std::move(str.str()))
  {
  }

  SString(const SCharVector::const_iterator& begin, const SCharVector::const_iterator& end) :
      _str(begin, end)
  {

  }

  SString(const SCharVector::const_reverse_iterator& rbegin, const SCharVector::const_reverse_iterator& rend) :
      _str(rbegin, rend)
  {

  }

  SString(const SConstIterator& begin, const SConstIterator& end) :
      _str(begin, end)
  {

  }

  SString(const SConstReverseIterator& rbegin, const SConstReverseIterator& rend) :
      _str(rbegin, rend)
  {

  }

public:
  virtual ~SString()
  {
  }

private:
  String _str;

public:
  bool BeginsWith(const SString& str) const
  {
    return (IndexOf(str) == 0);
  }

  bool EndsWith(const SString& str) const
  {
    if (str.Length() > Length()) return false;
    return std::equal(str.str().rbegin(), str.str().rend(), _str.rbegin());
  }

  SString& Fit()
  {
    _str = _str.c_str();
    return *this;
  }

  SString &ToLower()
  {
    std::transform(_str.begin(), _str.end(), _str.begin(), ::tolower);
    return *this;
  }

  SString ToLowerCopy() const
  {
    String str = _str;
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return std::move(str);
  }

  SString &ToUpper()
  {
    std::transform(_str.begin(), _str.end(), _str.begin(), ::toupper);
    return *this;
  }

  SString ToUpperCopy() const
  {
    String str = _str;
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return std::move(str);
  }

  bool IsEmpty() const
  {
    return _str.empty();
  }

  bool IsNull() const
  {
    return _str.c_str() == nullptr;
  }

  bool IsNullOrEmpty() const
  {
    return (_str.c_str() == nullptr) || _str.empty();
  }

  size_t Length() const
  {
    return _str.size();
  }

  size_t ByteLength() const
  {
    return _str.size() * sizeof(SChar);
  }

  bool Contains(const SString &str) const
  {
    return (_str.find(str.str()) != _str.npos);
  }

  size_t IndexOf(const SString &str) const
  {
    return _str.find(str.str());
  }

  size_t LastIndexOf(const SString::SChar & c) const
  {
    return _str.find_last_of(c);
  }

  size_t LastIndexOf(const SString& str) const
  {
    return _str.find_last_of(str.str());
  }

#ifdef _UNICODE

  SString &Replace(const char &character, const char &with)
  {
    return Replace(SString(character)[0], SString(with)[0]);
  }

#else

  SString &Replace(const wchar_t &character, const wchar_t &with)
  {
    return Replace(SString(character)[0], SString(with)[0]);
  }

#endif

  SString &Replace(const SChar &character, const SChar &with)
  {
    std::replace(_str.begin(), _str.end(), character, with);
    return *this;
  }

#ifdef _UNICODE

  SString ReplaceCopy(const char &character, const char &with) const
  {
    SString copy = _str;
    return std::move(copy.Replace((SChar) character, (SChar) with));
  }

#endif

  SString ReplaceCopy(const SChar &character, const SChar &with) const
  {
    SString copy = _str;
    return std::move(copy.Replace(character, with));
  }

  SString& Replace(const SString& str, const SString& with, size_t startPos = 0)
  {
    if (!_str.empty() && (startPos < _str.size()))
    {
      while ((startPos = _str.find(str._str, startPos)) != String::npos)
      {
        _str.replace(startPos, str._str.length(), with._str);
        startPos += with.Length();
      }
    }

    return *this;
  }

  SString ReplaceCopy(const SString& str, const SString& with, size_t startPos = 0) const
  {
    return std::move(SString(_str).Replace(str, with, startPos));
  }

  inline void Resize(const String::size_type& size)
  {
	  _str.resize(size);
  }

#ifdef _UNICODE
  SStringVector Split(const char &delim, const bool &trim = true) const
  {
    return Split((wchar_t)delim, trim);
  }
#endif

  SStringVector Split(const SChar &delim, const bool &trim = true) const
  {
    SStringVector elems;

    SStringStream ss(_str);
    String item;
    while (std::getline(ss, item, delim))
    {
      elems.push_back(trim ? Trim(item) : item);
    }

    return std::move(elems);
  }

  SString& TrimLeft()
  {
    LeftTrim(_str);
    return *this;
  }

  SString TrimLeftCopy() const
  {
    String copy(_str);
    return std::move(LeftTrim(copy));
  }

  SString& TrimRight()
  {
    RightTrim(_str);
    return *this;
  }

  SString TrimRightCopy() const
  {
    String copy(_str);
    return std::move(RightTrim(copy));
  }

  SString& Trim()
  {
    Trim(_str);
    return *this;
  }

  SString TrimCopy() const
  {
    String copy(_str);
    return std::move(Trim(copy));
  }

  SString SubString(const size_t &start, const size_t &count) const
  {
    if (count && (start < _str.size()))
    {
      return _str.substr(start, count);
    }

#ifdef _UNICODE
    return SString(L"");
#else
    return SString("");
#endif
  }

  SString SubString(const size_t &start) const
  {
    if (start < _str.size())
    {
      return _str.substr(start);
    }

#ifdef _UNICODE
    return SString(L"");
#else
    return SString("");
#endif
  }

  inline SIterator begin() noexcept
  {
    return _str.begin();
  }

  inline SIterator end() noexcept
  {
    return _str.end();
  }

  inline SConstIterator begin() const noexcept
  {
    return _str.begin();
  }

  inline SConstIterator end() const noexcept
  {
    return _str.end();
  }

  inline SReverseIterator rbegin() noexcept
  {
    return _str.rbegin();
  }

  inline SReverseIterator rend() noexcept
  {
    return _str.rend();
  }

  inline SConstReverseIterator rbegin() const noexcept
  {
    return _str.rbegin();
  }

  inline SConstReverseIterator rend() const noexcept
  {
    return _str.rend();
  }

  inline SConstIterator cbegin() const noexcept
  {
    return _str.cbegin();
  }

  inline SConstIterator cend() const noexcept
  {
    return _str.end();
  }

  inline SConstReverseIterator crbegin() const noexcept
  {
    return _str.crbegin();
  }

  inline SConstReverseIterator crend() const noexcept
  {
    return _str.crend();
  }

public:
  const String& str() const { return _str; }

public:
  SString&operator=(const SString& str)
  {
    if (this != &str)
    {
      _str = str._str;
    }

    return *this;
  }

  SString&operator=(SString&& str) noexcept
  {
    if (this != &str)
    {
      _str = std::move(str._str);
    }

    return *this;
  }

  operator std::string() const
  {
#ifdef _UNICODE
    return std::move(SString::ToUtf8(_str));
#else
    return _str;
#endif
  }

  operator std::wstring() const
  {
#ifdef _UNICODE
    return _str;
#else
    return std::move(SString::FromUtf8(_str));
#endif
  }

  SString &operator+=(const SString &s)
  {
    _str += s.str();
    return *this;
  }

  bool operator==(const SString &s) const
  {
    return _str == s.str();
  }

  bool operator!=(const SString &s) const
  {
    return _str != s.str();
  }

  bool operator<(const SString &s) const
  {
    return _str < s.str();
  }

  bool operator<=(const SString &s) const
  {
    return _str <= s.str();
  }

  bool operator>(const SString &s) const
  {
    return _str > s.str();
  }

  bool operator>=(const SString &s) const
  {
    return _str >= s.str();
  }

  SChar &operator[](const size_t &idx)
  {
    return _str[idx];
  }

  const SChar &operator[](const size_t &idx) const
  {
    return _str[idx];
  }

  explicit operator const SChar *() const
  {
    return _str.c_str();
  }
};

static SString operator+(const SString &s1, const SString s2)
{
  return SString(s1.str() + s2.str());
}

namespace std
{
template<>
struct hash<SString>
{
  typedef SString argument_type;

  typedef std::size_t result_type;

  result_type operator()(argument_type const &s) const
  {
    return std::hash<SString::String>()(s.str());
  }
};
}

#endif //_SSTRING_H
