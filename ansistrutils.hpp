#ifndef ANSISTR_UTILS_HXX
#define ANSISTR_UTILS_HXX
//---------------------------------------------------------------------------------------------------------------------

/** @file ansistrutils.hxx
 *  @author Juan Carlos García Vázquez (gavajc@gmail.com)
 *  @date   2010-07-15 last update 2026-01-15
 *  @brief  Function prototypes to manipulate C/C++ strings
 *
 *  The library was developed to facilitate interaction with ANSI C/C++ strings
 *  and for work with views like std::string_view if use C++17 or later.
 *
 *  This library can works with boost views such as boost::string_view or boost::core::string_views
 *  if you define __uboostview.
 *
 *  You'll be able to:
 *
 *          tokenize,
 *          split strings,
 *          convert strings to toupper,
 *          convert strings to tolower,
 *          manipulate string dates and more.
 *
 *  The library is compatible with char *, std::string, and views (std and boost).
*/

#include <set>
#include <map>
#include <ctime>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include <iomanip>
#include <cstring>
#include <sstream>
#include <utility>
#include <iterator>
#include <typeinfo>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <sys/stat.h>

#if __cplusplus >= 201703L
    #include <string_view>
    #ifndef INLINE_C17
        #define INLINE_C17 inline // C++17 avoid to generate constexpr copies for each file that require the constexpr used by this library.
    #endif
#else
    #ifndef INLINE_C17
        #define INLINE_C17
    #endif
#endif

#ifdef __uboostview
    #include <boost/utility/string_view.hpp>
    #include <boost/core/detail/string_view.hpp>
#endif
//---------------------------------------------------------------------------------------------------------------------

namespace StrUtils
{
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
        INLINE_C17 constexpr const char CXP_OS_SLASH = '\\';
        #ifdef _MSC_VER
            #define strncasecmp _strnicmp
            #define strcasecmp _stricmp
        #endif
    #else
        INLINE_C17 constexpr const char CXP_OS_SLASH = '/';
    #endif

    INLINE_C17 constexpr const char *CXP_PERCENT_ENCODE_URI_CHARS = "!*'()<>%\"{}|\\^[]` ;/?+#$&,:=@"; // Don't change the order. See PercentEncode
    //---------------------------------------------------------------------------------------------------------------------

    ///< Enumeration to be used with PercentEncode methods. Not change the order. See PercentEncode
    enum class EUriStyles
    {
        ENCODE_ASCII_NOT_PRINTABLE, ///< Only encode not printable ASCII CHARS. i.e. Range 0x00 to 0x20 and 0x7E
        ENCODE_URI_COMPONENT_AWS,   ///< Encode all except A–Z a–z 0–9 - _ . ~
        ENCODE_URI_COMPONENT,       ///< Encode all except A–Z a–z 0–9 - _ . ~ ! * ' ( )
        ENCODE_URI_AWS,             ///< Encode all except A–Z a–z 0–9 - _ . ~ $ & , : = @
        ENCODE_URI,                 ///< Encode all except A–Z a–z 0–9 - _ . ~ ! * ' ( ) ; / ? : @ & = + $ , #
        ENCODE_NONE                 ///< Not encode so do nothing.
    };
    //---------------------------------------------------------------------------------------------------------------------

    /**
      * @class CStrView
      * @brief Class view for handle const string types like
      *
      *              const C string (const char *) i.e. null terminate strings
      *              const C++ strings (const std::string)
      *              String Views (std::string_views) C++17 or later is required.
      *              Boost Views  (boost::string_view and boost::core::string_view) Must define __uboostviewto accept these.
      *
      *        This class was created to address the need for implicit data type conversion between `std` and `boost`.
      *        Prior to Boost version 1.80, compatibility between these types was poor, which led to the creation of this class.
      *        This class does not replace std::string_view or boost::string_view; it is merely an auxiliary tool for interchanging types.
      * 
      *        So the goal is to assist in passing parameters of the string object type such:
      * 
      *             std::string_view, boost::string_view, std::string, or char buffers—as parameters.
      *
      *        This class is used in conjunction with certain `CAnsiStr` methods and `CNoCaseCmp`;
      *        it is compatible with the C++11 standard or later versions.
      *
      *        This class works like a view so the restrictions for a view apply to this class. i.e.
      *        The original string or character sequence must persists in memory when someone use this class.
      *
      *        This class is lightweight so it must be passed by value and not by reference, as std or boost views works.
    */
    class CStrView
    {
    private:

        const char *sdata; ///< Primitive string data that store a pointer to the string data location.
        size_t dsize;      ///< Stores the size of the data element.

    public:

        ///< Constructor from null terminate C string. If str is not null terminate, result is undefined behavior.
        CStrView(const char *str) : sdata(str), dsize(str ? strlen(str) : 0) {}
        ///< Constructor from buffer with specified size.
        CStrView(const char *buf, size_t size) : sdata(buf), dsize(size)     {}
        ///< Constructor from basic std::string.
        CStrView(const std::string &str) : sdata(&str[0]), dsize(str.size()) {}

        #if __cplusplus >= 201703L
            ///< Constructor from std::string_view that is implemented since C++17. We passed a cheap copy avoiding references.
            CStrView(std::string_view view) : sdata(view.data()), dsize(view.size()) {}
            ///< Operator to convert our view as std::string_view
            operator std::string_view() const noexcept { return std::string_view(sdata, dsize); }
        #endif

        #ifdef __uboostview
            ///< Constructor from old boost string_view. We passed a cheap copy avoiding references.
            CStrView(boost::string_view obview) : sdata(obview.data()), dsize(obview.size()) {}
            ///< Constructor from new boost string_view implementation, designed to be highly compatible with std::string_view. We passed a cheap copy avoiding references.
            CStrView(boost::core::string_view cbview) : sdata(cbview.data()), dsize(cbview.size()) {}
            ///< Operator to convert our view to old boost::string_view
            operator boost::string_view() const noexcept       { return boost::string_view(sdata, dsize);       }
            ///< Operator to convert our view to new boost::string_view
            operator boost::core::string_view() const noexcept { return boost::core::string_view(sdata, dsize); }
        #endif

        ///< Default copy constructor.
        constexpr CStrView(const CStrView&) noexcept = default;
        ///< Default asigment operator.
        CStrView& operator=(const CStrView&) noexcept = default;
        ///< Array index operator. If index > to length is undefined behavior.
        constexpr const char& operator[](size_t index) const noexcept { return sdata[index]; }
        ///< Operator to get a std::string
        explicit operator std::string() const { return std::string(sdata, dsize); }
        ///< Operator for not-equal-to comparison
        bool operator!=(const CStrView& other) const noexcept { return !(*this == other); }
        ///< Operator for equality comparison
        bool operator==(const CStrView& other) const noexcept {
            return dsize == other.dsize && (sdata == other.sdata || std::memcmp(sdata, other.sdata, dsize) == 0);
        }

        ///< Here get index or throw out range exception.
        const char& at(size_t index) const
        {
            if (index >= dsize)
                throw std::out_of_range("Index: " + std::to_string(index) + " is out of range");

            return *(this->sdata + index);
        }

        ///< Stream output operator. Formatting was implemented.
        template<typename _Traits = std::char_traits<char>>
        friend  inline std::basic_ostream<char, _Traits>& operator<<(std::basic_ostream<char, _Traits> &os, CStrView view)
        {
            using _osType                      = std::basic_ostream<char, _Traits>;
            typename _osType::iostate _ioState = _osType::goodbit;
            size_t _pad = (os.width() <= 0 || static_cast<size_t>(os.width()) <= view.size()) ? 0 : (static_cast<size_t>(os.width()) - view.size());

            const typename _osType::sentry state(os);

            if (state)
            {
                try
                {
                    if ((os.flags() & _osType::adjustfield) != _osType::left)
                    {
                        for (; 0 < _pad; --_pad)
                        {
                            if (_Traits::eq_int_type(_Traits::eof(), os.rdbuf()->sputc(os.fill()))) {
                                _ioState |= _osType::badbit;
                                break;
                            }
                        }
                    }

                    if (_ioState == _osType::goodbit && static_cast<size_t>(os.rdbuf()->sputn(view.data(), static_cast<size_t>(view.size()))) != view.size())
                        _ioState |= _osType::badbit;
                    else
                    {
                        for (; 0 < _pad; --_pad)
                        {
                            if (_Traits::eq_int_type(_Traits::eof(), os.rdbuf()->sputc(os.fill()))) {
                                _ioState |= _osType::badbit;
                                break;
                            }
                        }
                    }

                    os.width(0);
                }
                catch (...) {
                    os.setstate(_osType::badbit);
                    throw;
                }
            }
            else
                _ioState |= _osType::badbit;

            os.setstate(_ioState);
            return os;
        }

        ///< Member to obtain the view size.
        constexpr size_t size() const noexcept      { return this->dsize;      }
        ///< Member to obtain the view size. Is an alias for the above method.
        constexpr size_t length() const noexcept    { return this->dsize;      }
        ///< Member to check if view is empty.
        constexpr bool empty() const noexcept       { return this->dsize == 0; }
        ///< Member to obtain const data to the raw pointer.
        constexpr const char* data() const noexcept { return this->sdata;      }

        ///< Method to compare this < than other without case sensitive. Lexicographical comparison
        ///< We avoid overloading the < operator because it might be used in the future
        bool CompareLexicographicalLessNoCase(CStrView other) const noexcept
        {
            auto case_insensitive_less = [](char c1, char c2) {
                return std::tolower(static_cast<unsigned char>(c1)) < std::tolower(static_cast<unsigned char>(c2));
            };

            return std::lexicographical_compare(sdata, sdata + dsize,
                                                other.data(), other.data() + other.size(), case_insensitive_less);
        }

        ///< Method to compare this < than other without case sensitive. By string length sort.
        ///< We avoid overloading the < operator because it might be used in the future
        bool CompareLessNoCase(CStrView other) const noexcept
        {
            if ((this->empty() && other.empty()) || other.empty() || this->size() > other.size())
                return false;
            else if (this->empty() || this->size() < other.size())
                return true;
            else
                return strncasecmp(this->data(), other.data(), this->size()) < 0;
        }

        ///< Method to compare this > than other without case sensitive. Lexicographical comparison
        ///< We avoid overloading the > operator because it might be used in the future
        bool CompareLexicographicalGreaterNoCase(CStrView other) const noexcept
        {
            auto case_insensitive_greater = [](char c1, char c2) {
                return std::tolower(static_cast<unsigned char>(c1)) > std::tolower(static_cast<unsigned char>(c2));
            };

            return std::lexicographical_compare(sdata, sdata + dsize,
                                                other.data(), other.data() + other.size(), case_insensitive_greater);
        }

        ///< Method to compare this > than other without case sensitive. By string length sort.
        ///< We avoid overloading the > operator because it might be used in the future
        bool CompareGreaterNoCase(CStrView other) const noexcept
        {
            if ((this->empty() && other.empty()) || other.empty() || this->size() < other.size())
                return false;
            else if (this->empty() || this->size() > other.size())
                return true;
            else
                return strncasecmp(this->data(), other.data(), this->size()) > 0;
        }
    };
    //---------------------------------------------------------------------------------------------------------------------

    /**
     * @struct SNoCaseCmpAsc
     * @brief Structure that help to compare/sort ascending string containers (set, map) without case sensitive.
     *        Since C++14, heterogeneous lookups are available for these containers. 
     *        This feature is enabled when using C++14 or later.
    */
    struct SNoCaseCmpAsc
    {
        using is_transparent = void;

        template <typename T, typename U>
        bool operator()(const T& lhs, const U& rhs) const {
            CStrView lhsView{ lhs };
            CStrView rhsView{ rhs };

            return lhsView.CompareLexicographicalLessNoCase(rhsView);
        }
    };

    /**
     * @struct SNoCaseCmpDsc
     * @brief Structure that help to compare/sort descending string containers (set, map) without case sensitive.
     *        Since C++14, heterogeneous lookups are available for these containers.
     *        This feature is enabled when using C++14 or later.
    */
    struct SNoCaseCmpDsc
    {
        using is_transparent = void;

        template <typename T, typename U>
        bool operator()(const T& lhs, const U& rhs) const {
            CStrView lhsView{ lhs };
            CStrView rhsView{ rhs };

            return lhsView.CompareLexicographicalGreaterNoCase(rhsView);
        }
    };
    //---------------------------------------------------------------------------------------------------------------------

    /**
     * @class CAnsiStr
     * @brief Class to manipulate C/C++ strings and dates.
     *
    */
    class CAnsiStr
    {
        private:

            /// @brief Private method to convert time point into date string using format. UTC or local.
            ///        Because this method internally uses legacy C functions, the parameter format must be a null-terminated C string.
            static std::string TimePointToString(bool asUTC, const std::chrono::system_clock::time_point &tpoint, const char *format = nullptr)
            {
                auto seconds      = std::chrono::time_point_cast<std::chrono::seconds>(tpoint);
                auto timeFraction = tpoint - seconds;
                std::time_t time  = std::chrono::system_clock::to_time_t(tpoint);
                const char *fmt   = (format) ? format : "%F %T (%Z)";
                size_t formatSize = strlen(fmt);
                std::string strDateTime(128,0);
                size_t bytesWrited;

                if (asUTC) {
                    struct tm gmtTimeStamp;

                    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
                        gmtime_s(&gmtTimeStamp, &time);
                    #else
                        gmtime_r(&time, &gmtTimeStamp);
                    #endif

                    bytesWrited = std::strftime(&strDateTime[0], strDateTime.capacity() - 9, fmt, &gmtTimeStamp);
                }
                else {
                    struct tm localTimeStamp;

                    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
                        localtime_s(&localTimeStamp, &time);
                    #else
                        localtime_r(&time, &localTimeStamp);
                    #endif

                    bytesWrited = std::strftime(&strDateTime[0], strDateTime.capacity() - 9, fmt, &localTimeStamp);
                }

                if (bytesWrited)
                {
                    size_t initPos = bytesWrited - 2;

                    strDateTime.resize(bytesWrited);

                    if      (fmt[formatSize - 3] == '.' && fmt[formatSize - 2] == 'm' && fmt[formatSize - 1] == 's')
                        CAnsiStr::StrReplaceAndFill(strDateTime,std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(timeFraction).count()),initPos,2,3,48);
                    else if (fmt[formatSize - 3] == '.' && fmt[formatSize - 2] == 'u' && fmt[formatSize - 1] == 's')
                        CAnsiStr::StrReplaceAndFill(strDateTime,std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(timeFraction).count()),initPos,2,6,48);
                    else if (fmt[formatSize - 3] == '.' && fmt[formatSize - 2] == 'n' && fmt[formatSize - 1] == 's')
                        CAnsiStr::StrReplaceAndFill(strDateTime,std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(timeFraction).count()) ,initPos,2,9,48);

                    return strDateTime;
                }

                return std::string();
            }

        public:

            /** @brief Method to toupper a C char * buffer
             *
             * @param str  The C char * buffer to toupper
             * @param size The size of the str buffer. If 0 the str is treated as null terminated string.
             * @return     Pointer to the str buffer toupper.
            */
            static char * StrToupper(char *str, size_t size = 0)
            {
                if (str == nullptr)
                    return nullptr;

                if (size)
                {
                    for (size_t i = 0; i < size; i++)
                         str[i] = toupper(static_cast<unsigned char>(str[i]));
                }
                else
                {
                    for (size_t i = 0; str[i] != 0; i++)
                         str[i] = toupper(static_cast<unsigned char>(str[i]));
                }

                return str;
            }

            /** @brief Method to toupper a C++ string
             *
             * @param str   The C++ string to toupper
             * @param begin Initial iterator to start toupper. nullptr to start at begin of the string.
             * @param end   Terminate iterator to finish toupper. nullptr to finish at end of string.
             * @return      Reference to the str string toupper.
            */
            static std::string &StrToupper(std::string &str, const std::string::iterator *begin = nullptr, const std::string::iterator *end = nullptr)
            {
                if (begin == nullptr || end == nullptr)
                    std::transform(str.begin(),str.end(),str.begin(), [](unsigned char c) { return std::toupper(c); });
                else
                    std::transform(*begin,*end,*begin, [](unsigned char c) { return std::toupper(c); });

                return str;
            }

            /** @brief Method to toupper a copy of C++ string.
             *
             * @param str   The C++ const string to toupper
             * @param begin Initial iterator to start toupper. nullptr to start at begin of the string.
             * @param end   Terminate iterator to finish toupper. nullptr to finish at end of string.
             * @return      A String copy toupper.
            */
            static std::string StrToupper(const std::string &str, const std::string::const_iterator* begin = nullptr, const std::string::const_iterator* end = nullptr)
            {
                std::string cpStr = str;

                if (begin == nullptr || end == nullptr)
                    std::transform(cpStr.begin(), cpStr.end(), cpStr.begin(), [](unsigned char c) { return std::toupper(c); });
                else {
                    auto offset_begin = std::distance(str.cbegin(), *begin);
                    auto offset_end   = std::distance(str.cbegin(), *end);
                    auto dest_begin   = cpStr.begin() + offset_begin;
                    auto dest_end     = cpStr.begin() + offset_end;

                    std::transform(dest_begin, dest_end, dest_begin, [](unsigned char c) { return std::toupper(c); });
                }

                return cpStr;
            }

            /** @brief Method to tolower a C++ string
             *
             * @param str   The C++ string to tolower
             * @param begin Initial iterator to start tolower. nullptr to start at begin of the string.
             * @param end   Terminate iterator to finish tolower. nullptr to finish at end of string.
             * @return      Reference to the str string tolower.
            */
            static std::string &StrTolower(std::string &str, const std::string::iterator *begin = nullptr, const std::string::iterator *end = nullptr)
            {
                if (begin == nullptr || end == nullptr)
                    std::transform(str.begin(),str.end(),str.begin(), [](unsigned char c) { return std::tolower(c); });
                else
                    std::transform(*begin,*end,*begin, [](unsigned char c) { return std::tolower(c); });

                return str;
            }

            /** @brief Method to tolower a copy of C++ string
             *
             * @param str   The C++ const string to tolower
             * @param begin Initial iterator to start tolower. nullptr to start at begin of the string.
             * @param end   Terminate iterator to finish tolower. nullptr to finish at end of string.
             * @return      String tolower.
            */
            static std::string StrTolower(const std::string &str, const std::string::const_iterator* begin = nullptr, const std::string::const_iterator* end = nullptr)
            {
                std::string cpStr = str;

                if (begin == nullptr || end == nullptr)
                    std::transform(cpStr.begin(), cpStr.end(), cpStr.begin(), [](unsigned char c) { return std::tolower(c); });
                else {
                    auto offset_begin = std::distance(str.cbegin(), *begin);
                    auto offset_end   = std::distance(str.cbegin(), *end);
                    auto dest_begin   = cpStr.begin() + offset_begin;
                    auto dest_end     = cpStr.begin() + offset_end;

                    std::transform(dest_begin, dest_end, dest_begin, [](unsigned char c) { return std::tolower(c); });
                }

                return cpStr;
            }

            /** @brief Method to tolower a C char * buffer
             *
             * @param str  The C string to tolower
             * @param size The size of the str char * buffer. If 0 the str is treated as null terminated string.
             * @return Pointer to the str buffer tolower.
             */
            static char *StrTolower(char *str, size_t size = 0)
            {
                if (str == nullptr)
                    return nullptr;

                if (size)
                {
                    for (size_t i = 0; i < size; i++)
                         str[i] = tolower(static_cast<unsigned char>(str[i]));
                }
                else
                {
                    for (size_t i = 0; str[i] != 0; i++)
                         str[i] = tolower(static_cast<unsigned char>(str[i]));
                }

                return str;
            }

            /** @brief Method to trim a C++ string. Remove all extra spaces
             *         including at begin & at end of string.
             *
             * @param str The string to trim
             * @return    Reference to the trimmed string
            */
            static std::string &StrTrim(std::string &str)
            {
                char c;
                size_t pi=0;

                while ((pi =str.find(' ',pi)) != std::string::npos)
                {
                    c = str.c_str()[pi+1];
                    if (c == ' ' || c == 0 || pi == 0)
                        str.erase(pi,1);
                    else
                        pi++;
                }

                return str;
            }

            /** @brief Method that removes all characters in char from de C++ str string.
             *
             * @param str       The string where the characters are removed.
             * @param chars     A custom view to a persistent buffer with the chars to remove in str. If empty then do nothing.
            */
            static void EraseChars(std::string &str, const CStrView chars)
            {
                for (size_t i = 0; i < str.length(); i++)
                {
                    for (size_t j = 0; j < chars.size(); j++)
                    {
                        if (str[i] == chars[j])
                        {
                            str.erase(i,1);
                            i--;
                            break;
                        }
                    }
                }
            }

            /** @brief Method that replace all occurrences of oChar with rChar in str object.
             *
             * @param str   The string where we replace chars.
             * @param oChar The char to replace
             * @param rChar The char that will be replaced.
             * @return      Reference to the str string.
            */
            static std::string & ReplaceChar(std::string &str, char oChar, char rChar)
            {
                for (int i = 0, j = 0; str.c_str()[i]; i++)
                     if (str.c_str()[i] == oChar)
                         str.operator [](i+j) = rChar;

                return str;
            }

            /** @brief Replace a portion of the string with other string.
             *
             * @param str     The string with the substring to replace.
             * @param oString The original string to be replaced.
             * @param rString The new string that will be replaced the old one.
             * @return        Reference to the original string with the substring replaced.
            */
            static std::string & StrReplaceString(std::string &str, CStrView oString, CStrView rString)
            {
                size_t start_pos = 0;

                while ((start_pos = str.find(oString.data(), start_pos, oString.size())) != std::string::npos) {
                    str.replace(start_pos, oString.length(), rString.data(), rString.size());
                    start_pos += rString.length();
                }

                return str;
            }

            /** @brief Replace a portion of the string with other string.
             *         Unlike the previous version, this version creates a new string.
             *
             * @param str     View to a const string with the substring to replace. If is std::string, then you must pass it with c_str to call this method, otherwise the compiler calls the method above.
             * @param oString The original string to be replaced.
             * @param rString The new string that will be replaced the old one.
             * @return        A new string object with the substring replaced.
            */
            static std::string StrReplaceString(CStrView str, CStrView oString, CStrView rString)
            {
                size_t start_pos = 0;
                std::string replaced(str.data(), str.size());

                while ((start_pos = replaced.find(oString.data(), start_pos, oString.size())) != std::string::npos) {
                    replaced.replace(start_pos, oString.length(), rString.data(), rString.size());
                    start_pos += rString.length();
                }

                return replaced;
            }

            /** @brief Method to replace & fill with a specified char if shorter.
             *
             *      Example how to call it:
             *
             *              StrReplaceAndFill("2026-01-21 09:48:58.ms", "32", 20, 2, 3, '0');
             *              The result will be: 2026-01-21 09:48:58.032
             *
             *      You can see another example in private method TimePointToString.
             *
             * @param str         The string with the substring to replace/filled.
             * @param replaceStr  View to the new string to set in str.
             * @param initPos     Index of first character to replace in str.
             * @param nrepChars   The total characters to replace in str.
             * @param maxChars    Indicate the number of chars to fill when replaceStr is shorter.
             * @param fillingChar It's the character that used to fill the replaced string.
            */
            static void StrReplaceAndFill(std::string &str, CStrView replaceStr, size_t initPos,
                                          size_t nrepChars, size_t maxChars, char fillingChar)
            {
                if (str.empty() || replaceStr.empty() || initPos > str.size() || !maxChars)
                    return;
                if (replaceStr.size() < maxChars)
                {
                    std::stringstream ss;

                    ss << std::setw(maxChars) << std::setfill(fillingChar) << replaceStr;
                    str.replace(initPos,nrepChars,ss.str());
                }
                else
                    str.replace(initPos,nrepChars,replaceStr.data(),replaceStr.size());
            }

            /** @brief Method that searches for a pattern in a buffer view. Useful when you need to find string/bytes.
             *
             * @param buffer   View where to find a pattern
             * @param pattern  The pattern to find in buffer.
             * @return         A pointer to the position of the first character found or null if not found.
            */
            static const char * FindSequence(CStrView buffer, CStrView pattern)
            {
                if (!buffer.empty() && !pattern.empty() && pattern.size() <= buffer.size())
                {
                    for (const char *ptr = buffer.data(), *mpos = (buffer.data()+buffer.size()-pattern.size()); ptr <= mpos; ptr++)
                    {
                         if (!memcmp(ptr,pattern.data(),pattern.size()))
                             return  ptr;
                    }
                }

                return nullptr;
            }

            /** @brief Method that searches for a pattern in a C buffer. It uses ranges. Useful when you need to find bytes.
             *
             *         strbegin & strend must be the same buffer but in diferent positions
             *         seqbegin & seqend must be the same buffer but in diferent positions
             *
             *         The buffer pointers must be pointing within a valid range; if they are out of range or
             *         if the buffers aren't the same, the result is undefined behavior.
             *
             * @param strbegin The begin position where find the sequence.
             * @param strend   The end position where find the sequence.
             * @param seqbegin The begin position of the pattern to find.
             * @param seqend   The end position of the pattern to find.
             * @return         A pointer to the position of the first character found or null if not found.
            */
            static const char * FindSequence(const char *strbegin, const char *strend, const char *seqbegin, const char *seqend)
            {
                if (strbegin && strend && seqbegin && seqend && strend >= strbegin && seqend >= seqbegin)
                {
                    for (const char *ptr = strbegin, *mpos = strend-(seqend-seqbegin); ptr <= mpos; ptr++)
                    {
                         if (!memcmp(ptr,seqbegin,seqend-seqbegin+1))
                             return  ptr;
                    }
                }

                return nullptr;
            }

            /** @brief Method to find a char in a C style string in reverse order
             *
             * @param c    The char to find in str.
             * @param str  The C style string where find a char
             * @param size The size of the str buffer. If 0 the str is treated as null terminated string so str must be a null terminate string or behavior is undefined.
             * @return     A pair. <true, position where founded> or <false,0> if not found.
            */
            static std::pair<bool,size_t> CharRFind(const char c, const char *str, size_t size = 0)
            {
                size_t pos;

                // Verify that it's not null; if size is 0, it's assumed str is a null-terminated string.
                // so we need to check that isn't an empty string.
                if (!str || (!size && *str == '\0'))
                    return {false,0};

                for (pos = size ? (size-1) : strlen(str)-1; str[pos] != c; pos--) {
                     if (!pos)
                         return {false,0};
                }

                return {true,pos};
            }

            /** @brief Method that return all positions of a pattern in View buffer.
             *
             * @param buffer  View where to find a pattern
             * @param pattern The pattern to find.
             * @return        A STL container with all number positions of a char in a string.
            */
            static std::vector<size_t> GetAllPatternPositions(CStrView buffer, CStrView pattern)
            {
                std::vector<size_t> positions;

                if (!buffer.empty() && !pattern.empty() && pattern.size() <= buffer.size())
                {
                    for (const char *ptr = buffer.data(), *mpos = (buffer.data()+buffer.size()-pattern.size()); ptr <= mpos;)
                    {
                         // If Pattern found, save his position.
                         if (!memcmp(ptr,pattern.data(),pattern.size())) {
                             size_t pos = ptr-buffer.data();
                             positions.push_back(pos);
                             ptr += pattern.size();
                         }
                         else
                             ptr++;
                    }
                }

                return positions;
            }

            /** @brief Templated method that convert a primitive C/C++ number to a string
             *         fillLength is used to fill with zeros before the number to convert.
             *
             *         Example:
             *
             *
             *          fillLength | number to convert | output string
             *              0                5                 5
             *              1                5                 5
             *              2                5                 05
             *              3                5                 005
             *              6              12.34               012.34
             *
             * @tparam T          The templated number.
             * @param  n          The number to convert to string
             * @param  fillLength Use it when you want to fill with the desired number of 0's before the number to convert to string.
             * @param  precision  Only used when the number to convert to string is float or double.
             * @return The number as string.
            */
            template <typename T>
            static std::string NumToStr(T n, uint16_t fillLength = 0, uint16_t precision = 1)
            {
                std::stringstream ss;
                std::string typeName = typeid(n).name();

                if (typeid(int) == typeid(n))
                    ss << std::setw(fillLength) << std::setfill('0') << (int)            n;
                else if (typeid(long) == typeid(n))
                    ss << std::setw(fillLength) << std::setfill('0') << (long)           n;
                else if (typeid(short) == typeid(n))
                    ss << std::setw(fillLength) << std::setfill('0') << (short)          n;
                else if (typeid(unsigned int) == typeid(n))
                    ss << std::setw(fillLength) << std::setfill('0') << (unsigned int)   n;
                else if (typeid(unsigned long) == typeid(n))
                    ss << std::setw(fillLength) << std::setfill('0') << (unsigned long)  n;
                else if (typeid(unsigned short) == typeid(n))
                    ss << std::setw(fillLength) << std::setfill('0') << (unsigned short) n;
                else if (typeid(float) == typeid(n))
                    ss << std::setw(fillLength) << std::setfill('0') << std::fixed << std::setprecision(precision) << (float)   n;
                else if (typeid(double) == typeid(n))
                    ss << std::setw(fillLength) << std::setfill('0') << std::fixed << std::setprecision(precision) << (double)  n;
                else if (typeid(n) == typeid(long long int) || typeName == "__int64")
                    ss << std::setw(fillLength) << std::setfill('0') << (long long int)           n;
                else if (typeid(n) == typeid(unsigned long long int) || typeName == "unsigned __int64")
                    ss << std::setw(fillLength) << std::setfill('0') << (unsigned long long int)  n;

                return ss.str();
            }

            /** @brief Method that checks the content of a buffer view is a number (ASCII VALUES FOR NUMBERS).
             *
             * @param buffer       The buffer view to check if is the content is a valid number.
             * @param base         The numerical base to use. i.e. 2(bin), 8(oct), 10(dec), 16(hex). An unknown base is treated as an error.
             * @param ignoreSpaces If it is true and the buffer is a string and contains spaces, then the spaces are ignored; otherwise set error if spaces found.
             * @return             0 on error, i.e. is not a number or buffer is empty; otherwise return the buffer length.
            */
            static size_t StrIsNum(CStrView buffer, uint8_t base = 10, bool ignoreSpaces = false)
            {
                std::string base2    = "01";
                std::string base8    = "01234567";
                std::string base16   = "0123456789ABCDEFabcdef";
                std::string &allowed = (base == 2) ? base2 : ((base == 8) ? base8 : base16);

                if (buffer.empty() || (base != 2 && base != 8 && base != 10 && base != 16))
                    return 0;

                if (base == 10) // Base 10 is handled differently
                {
                    char first = ' ';

                    for (size_t index=0, dots=0, sign=0; index < buffer.size(); index++)
                    {
                        if (buffer[index] == ' ' && ignoreSpaces)
                            continue;
                        if (buffer[index] != '-' && buffer[index] != '.' && (buffer[index] < 48 || buffer[index] > 57))
                            return 0;

                        first = (first == ' ') ? buffer[index] : first;

                        if (buffer[index] == '-' && (++sign > 1 || first != '-'))
                            return 0;
                        if (buffer[index] == '.' && ++dots > 1)
                            return 0;
                    }
                }
                else            // Here handle base 2, 8 and 16
                {
                    for (size_t aIndex, bIndex=0; bIndex < buffer.size(); bIndex++)
                    {
                         if  (buffer[bIndex] == ' ' && ignoreSpaces)
                              continue;

                         for (aIndex = 0; aIndex < allowed.size() && allowed[aIndex] != buffer[bIndex]; aIndex++);

                         if  (aIndex == allowed.size())
                              return 0;
                    }
                }

                return buffer.size();
            }

            /** @brief Method that check if a string is a number. Because str is non const object,
             *         this version can modify it.
             *
             * @param str          The string to check if is a valid number.
             * @param base         The numerical base to use. i.e. 2(bin), 8(oct), 10(dec), 16(hex). An unknown base is treated as an error.
             * @param removeSpaces If true and str contains spaces, the spaces are removed; otherwise spaces are treated as error.
             * @return             false on error or true if is a valid number.
            */
            static bool StrIsNum(std::string &str, uint8_t base = 10, bool removeSpaces = false)
            {
                std::string base2    = "01";
                std::string base8    = "01234567";
                std::string base16   = "0123456789ABCDEFabcdef";
                std::string &allowed = (base == 2) ? base2 : ((base == 8) ? base8 : base16);

                if (str.empty() || (base != 2 && base != 8 && base != 10 && base != 16))
                    return false;

                if (base == 10) // Base 10 is handled differently
                {
                    char first = ' ';

                    for (size_t index=0, dots=0, sign=0; index < str.size(); index++)
                    {
                        if (str[index] == ' ' && removeSpaces) {
                            str.erase(index--, 1);
                            continue;
                        }

                        if (str[index] != '-' && str[index] != '.' && (str[index] < 48 || str[index] > 57))
                            return false;

                        first = (first == ' ') ? str[index] : first;

                        if (str[index] == '-' && (++sign > 1 || first != '-'))
                            return false;
                        if (str[index] == '.' && ++dots > 1)
                            return false;
                    }
                }
                else            // Here handle base 2, 8 and 16
                {
                    for (size_t aIndex, sIndex=0; sIndex < str.size(); sIndex++)
                    {
                        if  (str[sIndex] == ' ' && removeSpaces) {
                            str.erase(sIndex--, 1);
                            continue;
                        }

                        for (aIndex = 0; aIndex < allowed.size() && allowed[aIndex] != str[sIndex]; aIndex++);

                        if  (aIndex == allowed.size())
                            return false;
                    }
                }

                return true;
            }

            /** @brief Templated method that convert a C style string to number. Only use with legacy code.
             *
             * @tparam T     The templated output number.
             * @param strNum The number as C style string. Must be a NULL terminated string.
             * @return       The templated number if strNum is ok or throw an error if not.
            */
            template <typename T> static
            T StrToNum(const char *strNum)
            {
                T result = 0;
                std::string typeName = typeid(result).name();

                if (!StrIsNum(strNum))
                    throw std::logic_error("[StrUtils::StrToNum] The string parameter isn't a number.");

                if (typeid(long) == typeid(result))
                    result = atol(strNum);
                else if (typeid(unsigned long) == typeid(result))
                    result = labs(atol(strNum));
                else if (typeid(int) == typeid(result) || typeid(short) == typeid(result))
                    result = atoi(strNum);
                else if (typeid(float) == typeid(result) || typeid(double) == typeid(result))
                    result = atof(strNum);
                else if (typeid(unsigned int) == typeid(result) || typeid(unsigned short) == typeid(result))
                    result = abs(atoi(strNum));
                else if (typeName == "__int64" || typeName == "unsigned __int64" ||
                         typeid(long long int) == typeid(result) || typeid(unsigned long long int) == typeid(result))
                {
                    unsigned long long int tmpNum = 0;
                    short i = (strNum[0] == '-') ? 1 : 0;

                    for (; strNum[i] != 0 && strNum[i] != '.' && i < 20; i++)
                        tmpNum = (tmpNum << 3) + (tmpNum << 1) + (strNum[i]-48);

                    if (strNum[0] == '-' && (typeName == "__int64" ||  typeid(long long int) == typeid(result)))
                        result = tmpNum * -1;
                    else
                        return tmpNum;
                }

                return result;
            }

            /** @brief Method to split a full path in path, name, extension.
             *
             *         Note: This method always clears path, fileName & ext.
             *         discardLastNull parameter is useful when pass a buffer in the view
             *         with the null terminate character at the end so if true that is discarded.
             *
             * @param cPath            The full path as a view.
             * @param path             [Out] The path part.
             * @param fileName         [Out] The filename part.
             * @param ext              [Out] The extension part.
             * @param fileNameWithExt  If true then not remove the extension if exists from the file name.
             * @param preserveDotInExt If true the dot in a extension is preserved.
             * @param discardLastNull  If true check if the view last char is null teminate character and discard it.
            */
            static void SplitPathNameExt(CStrView cPath, std::string &path, std::string &fileName, std::string &ext,
                                         bool fileNameWithExt = false, bool preserveDotInExt = false, bool discardLastNull = true)
            {
                path = fileName = ext = "";
                if (cPath.empty())
                    return;

                uint8_t decrement = (discardLastNull && cPath[cPath.size()-1] == 0) ? 1 : 0;
                fileName.assign(cPath.data(),cPath.size()-decrement);
                std::pair<bool,size_t> rfp = CAnsiStr::CharRFind('/',fileName.data(),fileName.size()); // Try to find using Unix slash.

                if (rfp.first == false)
                    rfp = CAnsiStr::CharRFind('\\',fileName.data(),fileName.size());                   // Try to find using Win slash.

                if (rfp.first)
                {
                    path = fileName;
                    fileName.erase(0,rfp.second+1);
                    path.erase(rfp.second,-1);
                }

                if ((rfp = CAnsiStr::CharRFind('.',fileName.c_str(),fileName.size())).first)          // Try to find the ext dot.
                {
                    ext  = fileName;
                    ext.erase(0,rfp.second+(preserveDotInExt ? 0 : 1)); // Check if we need to remove the dot from the file extension.

                    if (!fileNameWithExt)                               // Check if we need to remove the file name extension.
                        fileName.erase(rfp.second,-1);
                }
            }

            /** @brief Method to split in tokens a view buffer; each separator is of length 1.
             *         This method is preserved to maintain compatibility with legacy code.
             * 
             *         Example:
             *
             *              StrUtils::CAnsiStr::StrTokeniza("The user, is: USER|TEXT","|,: ",tokens);
             *
             * @param buffer     A view buffer with the data to split in tokens.
             * @param separators A view  with the separators characters to use when split in tokens.
             * @param tokens     [OUT] The splitted tokens.
             * @param addNulls   If have a nulls when split tokens if this is true then add and empty string in the output tokens.
             * @param trim       If true then trim string before save the token.
             * @param erase      If true the clear the output container before tokenize.
             * @param addSep     If true then add to the putput token container the separator.
             * @return           The number of tokens in the output container.
            */
            static size_t StrTokeniza(CStrView buffer, CStrView separators, std::vector <std::string> &tokens,
                                      bool addNulls = false, bool trim = false, bool erase = false, bool addSep = false)
            {
                std::string tmp, separator;
                size_t bufferIndex, sepIndex, max;

                if (buffer.empty() || separators.empty())
                    return 0;
                if (erase)
                    tokens.clear();

                for (bufferIndex = 0, max = buffer.size()-1; bufferIndex < buffer.size(); bufferIndex++)
                {
                     for (sepIndex = 0; (tolower(static_cast<unsigned char>(buffer[bufferIndex])) != tolower(static_cast<unsigned char>(separators[sepIndex]))) && sepIndex < separators.size(); sepIndex++);

                     if (sepIndex == separators.size())                       // Separator not found append new char at end
                         tmp += buffer[bufferIndex];
                     if (sepIndex != separators.size() || bufferIndex == max) // Separator found or we're at end.
                     {
                         if (!tmp.empty())
                         {
                             if (trim)
                                 tokens.emplace_back(CAnsiStr::StrTrim(tmp));
                             else
                                 tokens.push_back(std::move(tmp));

                             tmp.clear();
                         }
                         else if (addNulls)
                             tokens.emplace_back("");

                         // Save separator only if found it and isn't at the end.
                         if (sepIndex < separators.size() && addSep && bufferIndex != max) {
                             separator = separators[sepIndex];
                             tokens.push_back(std::move(separator));
                         }
                     }
                }

                return tokens.size();
            }

            /** @brief Tne nw Method to split in tokens a view buffer with variable lenght for separators.
             *
             *         Example:
             *
             *              StrUtils::CAnsiStr::StrTokeniza("valSize!=maxErrors && id==userId",{"&&","!=","=="},tokens);
             *
             * @param buffer        A view buffer with the data to split in tokens.
             * @param separators    Container with the separator to use when split in tokens
             * @param tokens        [OUT] The splitted tokens.
             * @param caseSensitive If you want to use case sensitive when looking for separator then set to true.
             * @param addNulls      If have a nulls when split tokens if this is true then add and empty string in the output tokens.
             * @param trim          If true then trim string before save the token.
             * @param erase         If true the clear the output container before tokenize.
             * @param addSep        If true then add to the putput token container the separator.
             * @return              The number of tokens in the output container.
            */
            static size_t StrTokeniza(CStrView buffer, const std::vector<std::string> &separators, std::vector <std::string> &tokens,
                                      bool caseSensitive = false, bool addNulls = false, bool trim = false, bool erase = false, bool addSep = false)
            {
                size_t bufferIndex, max;
                std::string tmp, currSep;
                std::function<bool(const char, const char)> compare;

                if (buffer.empty() || separators.empty())
                    return 0;
                if (erase)
                    tokens.clear();
                if (caseSensitive)
                    compare = [](const char lhc, const char rhc) { return tolower(static_cast<unsigned char>(lhc)) == tolower(static_cast<unsigned char>(rhc)); };
                else
                    compare = [](const char lhc, const char rhc) { return lhc == rhc;  };

                for (bufferIndex = 0, max = buffer.size() - 1; bufferIndex < buffer.size(); bufferIndex++)
                {
                    // Iterate for each separator, so try to find it.
                    for (const auto& separator : separators)
                    {
                        if (compare(buffer[bufferIndex], separator[0]) && bufferIndex + separator.length() <= buffer.size())
                        {
                            size_t sepIndex = 0, tmpBufferIndex = bufferIndex;

                            // Try to find the separator completely or stop in we 're at the end of separator.
                            for (; compare(buffer[tmpBufferIndex], separator[sepIndex]) && sepIndex < separator.length(); sepIndex++, tmpBufferIndex++);
                            // Separator matches completely set flag separator found, set padding to index
                            if (sepIndex == separator.length())
                            {
                                bufferIndex = tmpBufferIndex - 1;
                                currSep     = separator;
                                break;
                            }
                        }
                    }

                    // Separator not found append new char at end
                    if (currSep.empty())
                        tmp += buffer[bufferIndex];
                    // Separator found or we're at end.
                    if (!currSep.empty() || bufferIndex == max)
                    {
                        if (!tmp.empty())
                        {
                            if (trim)
                                tokens.emplace_back(CAnsiStr::StrTrim(tmp));
                            else
                                tokens.push_back(std::move(tmp));

                            tmp.clear();
                        }
                        else if (addNulls)
                            tokens.emplace_back("");

                        // Save separator only if found it and isn't at the end.
                        if (!currSep.empty() && addSep && bufferIndex != max)
                            tokens.push_back(currSep);

                        currSep.clear();
                    }
                }

                return tokens.size();
            }

            /** @brief Method to split in tokens a view buffer for unique tokens so we use and std::set container
             *         if you want case-insensitive pass our custom comparators SNoCaseCmpAsc or SNoCaseCmpDsc with the set container
             * 
             * @param buffer        A view buffer with the data to split in tokens.
             * @param separators    Container with the separator to use when split in tokens.
             * @param tokens        [OUT] The splitted tokens.
             * @param caseSensitive If you want to use case sensitive when looking for separator then set to true.
             * @param trim          If true then trim string before save the token.
             * @param erase         If true then clear the output container before tokenize.
             * @return              The number of tokens in the output container.
            */
            template <typename T = std::less<std::string>> static
            size_t StrTokeniza(CStrView buffer, const std::vector<std::string> &separators, std::set <std::string, T> &tokens, 
                               bool trim = false, bool caseSensitive = false, bool erase = false)
            {
                std::string tmp;
                bool separatorFound;
                size_t bufferIndex, max;
                std::function<bool(const char, const char)> compare;

                if (buffer.empty() || separators.empty())
                    return 0;
                if (erase)
                    tokens.clear();
                if (caseSensitive)
                    compare = [](const char lhc, const char rhc) { return tolower(static_cast<unsigned char>(lhc)) == tolower(static_cast<unsigned char>(rhc)); };
                else
                    compare = [](const char lhc, const char rhc) { return lhc == rhc;  };

                for (bufferIndex = 0, max = buffer.size() - 1; bufferIndex < buffer.size(); bufferIndex++)
                {
                    separatorFound = false;

                    // Iterate for each separator, so try to find it.
                    for (const auto& separator : separators)
                    {
                        if (compare(buffer[bufferIndex], separator[0]) && bufferIndex + separator.length() <= buffer.size())
                        {
                            size_t sepIndex = 0, tmpBufferIndex = bufferIndex;

                            // Try to find the separator completely or stop in we 're at the end of separator.
                            for (; compare(buffer[tmpBufferIndex], separator[sepIndex]) && sepIndex < separator.length(); sepIndex++, tmpBufferIndex++);
                            // Separator matches completely set flag separator found, set padding to index
                            if (sepIndex == separator.length())
                            {
                                bufferIndex = tmpBufferIndex - 1;
                                separatorFound = true;
                                break;
                            }
                        }
                    }

                    // Separator not found append new char at end
                    if (!separatorFound)
                        tmp += buffer[bufferIndex];
                    // Separator found or we're at end.
                    if (separatorFound || bufferIndex == max)
                    {
                        if (!tmp.empty())
                        {
                            if (trim)
                                tokens.emplace(CAnsiStr::StrTrim(tmp));
                            else
                                tokens.insert(std::move(tmp));

                            tmp.clear();
                        }
                    }
                }

                return tokens.size();
            }

            /** @brief Templated method to split in tokens a view buffer; that has the format Key=Value
             *         By default use std::less<std::string> comparator but you can use your custom or 
             *         the case-insensitive SNoCaseCmpAsc, SNoCaseCmpDsc comparators. When decode is true 
             *         then try to decode the pairs key=value from percent encode.
             * 
             *          Example:
             * 
             *                  std::map<std::string,std::string,StrUtils::SNoCaseCmpAsc> keys;
             *                  std::string_view bufferView = "a=b,d=c,x=z w=j good_example= A=b|planta=%C3%A1rbol";
             *                  size_t totalKeys = StrUtils::CAnsiStr::StrTokenizaKeys(bufferView,"=",", |",keys,false,false,true,false,true);
             * 
             *              The result will be:
             * 
             *                       a = b
             *                       d = c
             *                       w = j
             *                       x = z
             *                  planta = Árbol
             * 
             *              A=b is discarded because I set case-insensitive comparator.
             *              good_example= is discarded because not allow empty values.
             *
             *              On the other hand, if I were to use the default comparator i.e. std::less<std::string> with keys:
             *                  
             *                  std::map<std::string, std::string> keys
             * 
             *              Then only good_example will be discarded.
             *
             * @tparam T            Template for the comparator to use. Can be SNoCaseCmpAsc, SNoCaseCmpDsc or your custom class. By default is: std::less<std::string>
             * @param buffer        A view buffer  with the data to split in tokens.
             * @param keySep        View with the separator used by key|value. Example can be =
             * @param assocSep      View with the separator for the key pairs. Example can be , | space or other.
             * @param tokens        [OUT] The splitted tokens.
             * @param throwInvalid  Throws if detect and invalid pair key=value.
             * @param emptyValues   If true allows keys with empty value else ignored or throw if throwInvalid is true.
             * @param trim          If true then trim string before save the token.
             * @param erase         If true the clear the output container before tokenize.
             * @param decode        If true try to decode the key and the value from percent encode.
             * @return              The number of tokens in the output container.
            */
            template <typename T = std::less<std::string>> static
            size_t StrTokenizaKeys(CStrView buffer, CStrView keySep, CStrView assocSep, std::map <std::string,std::string, T> &tokens,
                                   bool throwInvalid = false, bool emptyValues = false, bool trim = false, bool erase = false, bool decode = false)
            {
                std::vector<std::string> assocTokens, pairTokens;

                if (erase)
                    tokens.clear();
                if (!buffer.empty() && !keySep.empty() && !assocSep.empty())
                {
                    CAnsiStr::StrTokeniza(buffer, assocSep, assocTokens, false, trim);

                    for (const auto &pairKeyValue : assocTokens)
                    {
                         if (CAnsiStr::StrTokeniza(pairKeyValue, keySep, pairTokens, emptyValues, trim, true) != 2 || pairTokens[0].empty())
                         {
                             if (throwInvalid)
                                 throw std::logic_error("Invalid pair key value: " + pairKeyValue);

                             continue;
                         }

                         if (decode) {
                             CAnsiStr::PercentDecode(pairTokens[0]);
                             CAnsiStr::PercentDecode(pairTokens[1]);
                         }

                         tokens.emplace(std::move(pairTokens[0]), std::move(pairTokens[1]));
                    }
                }

                return tokens.size();
            }

            /** @brief Templated method to split in tokens a view buffer; usually a string buffer as a number tokens.
             *
             * @tparam T              The template for the output container. Normally std::string. If not valid then throws.
             * @param buffer          A view buffer  with the data to split in tokens.
             * @param separators      A view  with the separators characters to use when split in tokens.
             * @param tokens          [OUT] The splitted tokens as numbers.
             * @param addNullsAsZero  If have a nulls when split tokens if this is true then add and 0 in the output tokens.
             * @param throwConvert    If true and token is not a number then throws otherwise ignore token.
             * @param erase           If true then clear the output container before tokenize.
             * @return                The number of tokens in the output container.
            */
            template <typename T> static
            size_t StrTokenizaToNum(CStrView buffer, CStrView separators, std::vector <T> &tokens,
                                    bool addNullsAsZero = false, bool throwConvert = false, bool erase = false)
            {
                T result;
                size_t strNumSize;
                size_t bufferIndex, sepIndex, max;
                std::string tmp, strNum;

                if (buffer.empty() || separators.empty())
                    return 0;
                if (erase)
                    tokens.clear();

                for (bufferIndex = 0, max = buffer.size()-1; bufferIndex < buffer.size(); bufferIndex++)
                {
                    for (sepIndex = 0; (tolower(static_cast<unsigned char>(buffer[bufferIndex])) != tolower(static_cast<unsigned char>(separators[sepIndex]))) && sepIndex < separators.size(); sepIndex++);

                    if (sepIndex == separators.size())                       // Separator not found append new char at end
                        tmp += buffer[bufferIndex];
                    if (sepIndex != separators.size() || bufferIndex == max) // Separator found or we're at end.
                    {
                        CAnsiStr::EraseChars(tmp, " "); // Remove all spaces

                        if (!(strNumSize = CAnsiStr::StrIsNum(tmp.c_str())) && throwConvert)
                            throw std::logic_error("Non-number characters found");
                        if (tmp.empty() || strNumSize)
                        {
                            strNum = (!tmp.empty()) ? tmp : "0";

                            if (typeid(long) == typeid(result))
                                result = atol(strNum.c_str());
                            else if (typeid(unsigned long) == typeid(result))
                                result = labs(atol(strNum.c_str()));
                            else if (typeid(int) == typeid(result) || typeid(short) == typeid(result))
                                result = atoi(strNum.c_str());
                            else if (typeid(float) == typeid(result) || typeid(double) == typeid(result))
                                result = atof(strNum.c_str());
                            else if (typeid(unsigned int) == typeid(result) || typeid(unsigned short) == typeid(result))
                                result = abs(atoi(strNum.c_str()));
                            else if (typeid(long long int) == typeid(result) || typeid(unsigned long long int) == typeid(result))
                            {
                                unsigned long long int tmpNum = 0;
                                short i = (strNum[0] == '-') ? 1 : 0;

                                for (; strNum[i] != 0 && strNum[i] != '.' && i < 20; i++)
                                    tmpNum = (tmpNum << 3) + (tmpNum << 1) + (strNum[i] - 48);

                                if (strNum[0] == '-' && typeid(long long int) == typeid(result))
                                    result = tmpNum * -1;
                                else
                                    result = tmpNum;
                            }

                            if (!tmp.empty())
                                tokens.push_back(result);
                            else if (addNullsAsZero)
                                tokens.push_back(result);

                            tmp.clear();
                        }
                    }
                }

                return tokens.size();
            }

            /** @brief Method to encode string using URL encode. This method may use
             *         some variations of this encoding. For more information see EUriStyles.
             *         This version replace the chars in a string with the encoded chars.
             *
             * @param str      The string to percent encode.
             * @param uriStyle The percent encode variation format to use.
             * @param reEncode If true and the str has a previous encoding then re encode this chars.
             * @return         A reference to the encoded string.
            */
            static std::string & PercentEncode(std::string &str, EUriStyles uriStyle = EUriStyles::ENCODE_ASCII_NOT_PRINTABLE, bool reEncode = true)
            {
                if (str.empty() || uriStyle > EUriStyles::ENCODE_URI)
                    return str;

                std::stringstream is;
                size_t uriCharsLength                            = strlen(CXP_PERCENT_ENCODE_URI_CHARS);
                std::vector<std::pair<size_t, size_t>> uriAllPos = {{0,1},{0,uriCharsLength},{5,uriCharsLength},{0,23},{5,18}};
                std::pair<size_t, size_t> uriPos                 = uriAllPos[static_cast<size_t>(uriStyle)];
                std::set <char> uriReserved(CXP_PERCENT_ENCODE_URI_CHARS+uriPos.first,CXP_PERCENT_ENCODE_URI_CHARS+uriPos.second);

                is.fill('0');
                for (std::size_t i = 0; i < str.size(); i++)
                {
                     if (str[i] < 0x20 || str[i] > 0x7E || (uriStyle != EUriStyles::ENCODE_ASCII_NOT_PRINTABLE && uriReserved.find(str[i]) != uriReserved.end()))
                     {
                         if (!reEncode && str[i] == 0x25 && (i+3) <= str.size() && isxdigit(str[i+1]) && isxdigit(str[i+2]))
                             continue;

                         is << std::uppercase << "%" << std::hex << std::setw(2) << static_cast<int>(static_cast<uint8_t>(str[i]));
                         str.replace(i,1,is.str());
                         is.str(std::string());
                         i+= 2;
                     }
                }

                return str;
            }

            /** @brief Method to encode string view using URL encode. This method may use
             *         some variations of this encoding. For more information see EUriStyles.
             *         This version creates a copy of the original string with the encoded chars.
             *
             * @param str      The string view to percent encode. If empty do nothing.
             * @param uriStyle The percent encode variation format to use. I is ENCODE_NONE or invalid value return a copy of the strView as std::string
             * @param reEncode If true and the str has a previous encoding then re encode this chars.
             * @return         A new string with the encoded chars.
            */
            static std::string PercentEncode(CStrView strView, EUriStyles uriStyle = EUriStyles::ENCODE_ASCII_NOT_PRINTABLE, bool reEncode = true)
            {
                if (strView.empty())
                    return std::string();
                if (uriStyle > EUriStyles::ENCODE_URI)
                    return std::string(strView.data(),strView.size());

                std::stringstream is;
                size_t uriCharsLength                            = strlen(CXP_PERCENT_ENCODE_URI_CHARS);
                std::vector<std::pair<size_t, size_t>> uriAllPos = {{0,1},{0,uriCharsLength},{5,uriCharsLength},{0,23},{5,18}};
                std::pair<size_t, size_t> uriPos                 = uriAllPos[static_cast<size_t>(uriStyle)];
                std::set <char> uriReserved(CXP_PERCENT_ENCODE_URI_CHARS+uriPos.first,CXP_PERCENT_ENCODE_URI_CHARS+uriPos.second);

                is.fill('0');
                for (std::size_t i = 0; i < strView.size(); i++)
                {
                     if (strView[i] < 0x20 || strView[i] > 0x7E || (!(!reEncode && strView[i] == 0x25 && (i+3) <= strView.size() && isxdigit(strView[i+1]) && isxdigit(strView[i+2])) &&
                                                            uriStyle != EUriStyles::ENCODE_ASCII_NOT_PRINTABLE && uriReserved.find(strView[i]) != uriReserved.end()))
                         is << std::uppercase << "%" << std::hex << std::setw(2) << static_cast<int>(static_cast<uint8_t>(strView[i]));
                     else
                         is << strView[i];
                }

                return is.str();
            }

            /** @brief  Method to decode a string that is encoded with URL encode.
             *          This version replace the chars in a string with the decoded chars.
             *
             * @param uri           The string with the URL encode.
             * @param erasePlusSign If true erase the + symbol. Some apps requires erase it.
             * @return               A reference to the decoded data
            */
            static std::string & PercentDecode(std::string &uri, bool erasePlusSign = true)
            {
                for (std::size_t i = 0; i < uri.size(); ++i)
                {
                    if (uri[i] == '%')
                    {
                        if (i + 3 <= uri.size())
                        {
                            int value = 0;
                            std::stringstream is;

                            if (isxdigit(uri[i+1]) && isxdigit(uri[i+2]))
                            {
                                is << uri[i+1] << uri[i+2];
                                is >> std::hex >> value;
                                uri[i] = static_cast<char>(value);
                                uri.erase(i+1, 2);

                                continue;
                            }
                        }
                    }
                    else if (uri[i] == '+' && erasePlusSign)
                        uri.erase(i, 1);
                }

                return uri;
            }

            /** @brief  Method to decode a string view that is encoded with URL encode.
             *          This version creates a copy of the original string with the decoded chars.
             *
             * @param uri           The string view with the URL encode. If empty then return a empty std::string
             * @param erasePlusSign If true, then erase the + symbol. Some apps requires erase it.
             * @return              A new std::string with the decoded data
            */
            static std::string PercentDecode(CStrView uri, bool erasePlusSign = true)
            {
                std::string decoded;

                for (std::size_t i = 0; i < uri.size(); ++i)
                {
                    if (uri[i] == '%')
                    {
                        if (i + 3 <= uri.size())
                        {
                            int value = 0;
                            std::stringstream is;

                            if (isxdigit(uri[i+1]) && isxdigit(uri[i+2]))
                            {
                                is << uri[i+1] << uri[i+2];
                                is >> std::hex >> value;
                                i += 2;

                                decoded += static_cast<char>(value);
                                continue;
                            }
                        }
                    }
                    else if (uri[i] == '+' && erasePlusSign)
                        continue;

                    decoded += uri[i];
                }

                return decoded;
            }

            /** @brief Method to validate a string date, time or both
             *
             * This method use a string format to indicate the order of the date-time
             * and the elements presents in the string date-time given. It must be set
             * with the following chars separated by space:
             *
             *          y: Year
             *          m: Month
             *          d: Days
             *          h: Hours
             *          n: Minutes
             *          s: Seconds
             *          t: Miliseconds
             *
             *  The order is irrelevant as long as it is the same in both the date string and the format.
             *  Example: y m d h n s t | d m y h n s t | m d y h n s t | y m d | ...
             *
             * The follow chars are valid separators:
             *
             *          /
             *          -
             *          :
             *          .
             *        space
             *
             * Example of valid formats and string dates:
             *
             *          y/m/d h:n:s.t ---> 2022/12/31 1:02:4.5
             *          d-m-y h-n-s.t ---> 29-02-2024 23-59-59.999
             *          m/d-y h/n/s   ---> 5/1-1981   12/50/20
             *          y.m.d h.n.s.t ---> 22.12.12   14.30.25.12
             *          d m y h n s t ---> 23 08 17   00 50 23 111
             *          d m y         ---> 01 05 1981
             *          h:n:s         --->  4:5:1
             *
             * @param  stringDate The date-time string view to validate.
             * @param  format     A string view with the order of the elements presents in the date-time string.
             * @param  strict     If true the separators must match between stringDate and format, if not match then return error.
             * @return            1 if the date-time string is valid, -1 if the format param is invalid or 0 if stringDate param is invalid.
            */
            static int ValidateDateTime(CStrView stringDate, CStrView format, bool strict = true)
            {
                bool isData = false;
                uint32_t value, cmonth = 0, cday = 0;
                std::vector<std::string> fmtTokens, dtTokens;
                std::set <char> spChar = {'/','-',':','.',' '};
                char dateTimeSymbols[] = {'y','m','d','h','n','s','t'};
                uint8_t daysPerMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};

                if (CAnsiStr::StrTokeniza(format    ,"/-:. ",fmtTokens,true,false,false,true) < 5 || fmtTokens.size() > 13)
                    return -1;
                if (CAnsiStr::StrTokeniza(stringDate,"/-:. ",dtTokens ,true,false,false,true) != fmtTokens.size())
                    return 0;

                try
                {
                    for (size_t i = 0; i < fmtTokens.size(); i++)
                    {
                        isData = !isData;
                        if (spChar.find(fmtTokens[i][0]) != spChar.end())
                        {
                            if (isData)
                                return -1;
                            else if (strict && fmtTokens[i] != dtTokens[i])
                                return 0;
                            else
                                continue;
                        }

                        value = std::stoi(dtTokens[i]);

                        if      (std::tolower(static_cast<unsigned char>(fmtTokens[i][0])) == dateTimeSymbols[0]) // It's Year
                        {
                            daysPerMonth[1] = (value % 400 == 0 || (value % 4 == 0 && value % 100 != 0)) ? 29 : 28;
                            dateTimeSymbols[0] = '-';
                        }
                        else if (std::tolower(static_cast<unsigned char>(fmtTokens[i][0])) == dateTimeSymbols[1]) // It's Month
                        {
                            if (!value || value > 12)
                                return 0;

                            cmonth = value;
                            dateTimeSymbols[1] = '-';
                        }
                        else if (std::tolower(static_cast<unsigned char>(fmtTokens[i][0])) == dateTimeSymbols[2]) // It's Day
                        {
                            if (!value || value > 31)
                                return 0;

                            cday   = value;
                            dateTimeSymbols[2] = '-';
                        }
                        else if (std::tolower(static_cast<unsigned char>(fmtTokens[i][0])) == dateTimeSymbols[3]) // It's hour
                        {
                            if (value > 23)
                                return 0;

                            dateTimeSymbols[3] = '-';
                        }
                        else if (std::tolower(static_cast<unsigned char>(fmtTokens[i][0])) == dateTimeSymbols[4]) // It's minute
                        {
                            if (value > 59)
                                return 0;

                            dateTimeSymbols[4] = '-';
                        }
                        else if (std::tolower(static_cast<unsigned char>(fmtTokens[i][0])) == dateTimeSymbols[5]) // It's second
                        {
                            if (value > 59)
                                return 0;

                            dateTimeSymbols[5] = '-';
                        }
                        else if (std::tolower(static_cast<unsigned char>(fmtTokens[i][0])) == dateTimeSymbols[6]) // It's miliseconds
                        {
                            dateTimeSymbols[6] = '-';
                            continue;
                        }
                        else                                                          // Bad format
                            return -1;
                    }
                }
                catch (...) { return 0; }

                // If have a day & month check again for a valid day.
                if (cday && cmonth && (cday > daysPerMonth[cmonth-1]))
                    return 0;

                return 1;
            }

            /** @brief Method to validate a string date, time or both. In this version you can trim the string before check
             *         so the parameter stringDate will be modified to removing spaces.
             *
             *         This method call the above method so for a complete description see
             *         ValidateDateTime with a View as stringDate param.
             *
             * @param  stringDate The date-time string to validate.
             * @param  format     A string view with the order of the elements presents in the date-time string.
             * @param  strict     If true the separators must match between stringDate and format, if not match then return error.
             * @return            1 if the date-time string is valid, -1 if the format param is invalid or 0 if stringDate param is invalid.
            */
            static int ValidateDateTime(std::string &stringDate, CStrView format, bool strict = true, bool trim = false)
            {
                if (trim)
                    CAnsiStr::StrTrim(stringDate);

                CStrView viewStrDate(stringDate.c_str(),stringDate.size());

                return ValidateDateTime(viewStrDate,format,strict);
            }

            /** @brief Method to convert string date into time point. To see all possible format see std::get_time.
             *
             *       Note: choose clang or gcc >= 12.1 to observe the output.
             *             libstdc++ before 12.1 does not correctly implement the %b specifier:
             *
             *               https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78714
             *
             *       Some formats such as ISO 8601 use the Zulu flag to indicate that the string date is in UTC.
             *       You can use this flag but this method does not use it. To indicate that the string date is
             *       in UTC format, set the flag parseAsUTC.
             *
             *       parseUTC possible values:
             *
             *       0: String date is not UTC, treat it in local time zone.
             *       1: String date is a UTC date, so treat it as UTC.
             *       2: String date is not UTC, is in local time zone but we want to convert to UTC
             *
             *       You can see these 3 options ase none, ahead & behind offsets UTC.
             *
             *       NOTE: Because this method use internally some legacy C functions the parameter format must be
             *             a const std::string and not a view. So if you have a view you must create a std::string
             *             with the view data & size before pass that param:
             *
             *                  std::string(view.data,view.size())
             *
             * @param  strDate  View with the string date to convert.
             * @param  format   The format of the strDate. For example "%Y-%m-%dT%H:%M:%SZ" -> 2022-08-22T10:34:05Z
             * @param  parseUTC It's a flag that indicate how to parse the strDate. As local, as UTC or convert local to UTC.
             * @return          If the conversion was successful the timepoint is returned othersie and empty timepoint returned.
            */
            static std::chrono::system_clock::time_point StringDateToTimePoint(CStrView strDate, const std::string &format, uint8_t parseUTC = 0)
            {
                std::tm tm = {};
                std::stringstream ss;
                std::time_t timestamp;
                std::chrono::system_clock::time_point tpoint;

                ss << strDate;
                ss >> std::get_time(&tm, format.c_str());

                if (!ss.fail() && (timestamp = std::mktime(&tm)) != -1)
                {
                    long diff_tz_utc;

                    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
                        if (_get_timezone(&diff_tz_utc) != 0)
                            return tpoint;
                    #else
                        diff_tz_utc = timezone;
                    #endif

                    if (parseUTC)
                        timestamp = (parseUTC == 1) ? (timestamp - diff_tz_utc) : (timestamp + diff_tz_utc);

                    tpoint = std::chrono::system_clock::from_time_t(timestamp);
                }

                return tpoint;
            }

            /** @brief Method to convert time point into date string as GMT.
             *         If you want the time part of milli, micro or nanos then at
             *         end of the format put:
             *               .ms --> for milliseconds
             *               .us --> for microseconds
             *               .ns --> for nanoseconds
             *
             *   Example: "%F %T.ms" represents the time as 2024-02-21 14:39:27.125 Where 125 is the miliseconds part.
             *   Because this method internally uses legacy C functions, the parameter format must be a null-terminated C string.
             *
             * @param  tpoint The time point object to convert to string date.
             * @param  format The format to use in the conversion. This is a null terminate string. See strftime formats. If NULL the default format is: "%F %T (%Z)"
             * @return        The string converted or empty if fails. This method fails if the format is incorrect.
            */
            static std::string TimePointToUTCString(const std::chrono::system_clock::time_point &tpoint, const char *format = nullptr)
            {
                return CAnsiStr::TimePointToString(true, tpoint, format);
            }

            /** @brief Method to convert timepoint into date string as Localtime.
             *         If you want the time part of milli, micro or nanos then at
             *         end of the format put:
             *              .ms --> for milliseconds
             *              .us --> for microseconds
             *              .ns --> for nanoseconds
             *
             *   Example: "%F %T.ms" represents the time as 2024-02-21 14:39:27.125 Where 125 is the miliseconds part.
             *   Because this method internally uses legacy C functions, the parameter format must be a null-terminated C string.
             *
             * @param  tpoint The time point object to convert to string date.
             * @param  format The format to use in the conversion. This is a null terminate string. See strftime formats. If NULL the default format is: "%F %T"
             * @return        The string converted or empty if fails. This method fails if the format is incorrect.
            */
            static std::string TimePointToLocalString(const std::chrono::system_clock::time_point& tpoint, const char* format = nullptr)
            {
                return CAnsiStr::TimePointToString(false, tpoint, format);
            }

            /** @brief Method to convert Unix Epoch timestamp into date string as GMT.
             *         If you want the time part of milli, micro or nanos then add at
             *         the end of the format:
             *              .ms --> for milliseconds
             *              .us --> for microseconds
             *              .ns --> for nanoseconds
             *
             *         Example: "%F %T.ms" represents the time as 2024-02-21 14:39:27.125 Where 125 is the miliseconds part.
             *
             *         You must specify the duration template param that represents the unit in which are the Unix Epoch timestamp.
             *         Example:
             *              If Unix epoch timestamps is in seconds then use  ---> std::chrono::seconds
             *              If Unix epoch timestamps is in seconds then use  ---> std::chrono::milliseconds
             *              If Unix epoch timestamps is in seconds then use  ---> std::chrono::microseconds
             *              If Unix epoch timestamps is in seconds then use  ---> std::chrono::nanoseconds
             *
             *              TimestampToUTCString<std::chrono::milliseconds>(1752263211128, "%F %T.ms");
             *
             *         Because this method internally uses legacy C functions, the parameter format must be a null-terminated C string.
             *
             * @param  timestamp The Unix Epoch timestamp as 64 bits unsigned integer.
             * @param  format    The format to use in the conversion. This is a null terminate string. See strftime formats. If NULL the default format is: "%F %T (%Z)"
             * @return           The string converted or empty if fails. This method fails if the format is incorrect.
            */
            template <typename Duration>
            static std::string TimestampToUTCString(uint64_t timestamp, const char *format = nullptr)
            {
                std::chrono::time_point<std::chrono::system_clock, Duration> tpoint{ Duration{timestamp} };
                std::chrono::system_clock::time_point systp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(tpoint);

                return CAnsiStr::TimePointToString(true, systp, format);
            }

            /** @brief Method to convert Unix Epoch timestamp into date string as LocalTime.
             *         If you want the time part of milli, micro or nanos then at
             *         end of the format put:
             *              .ms --> for milliseconds
             *              .us --> for microseconds
             *              .ns --> for nanoseconds
             *
             *         Example: "%F %T.ms" represents the time as 2024-02-21 14:39:27.125 Where 125 is the miliseconds part.
             *
             *         You must specify the duration template param that represents the unit in which are the Unix Epoch timestamp.
             *         Example:
             *              If Unix epoch timestamps is in seconds then use  ---> std::chrono::seconds
             *              If Unix epoch timestamps is in seconds then use  ---> std::chrono::milliseconds
             *              If Unix epoch timestamps is in seconds then use  ---> std::chrono::microseconds
             *              If Unix epoch timestamps is in seconds then use  ---> std::chrono::nanoseconds
             *
             *              TimestampToLocalString<std::chrono::milliseconds>(1752263211128, "%F %T.ms");
             *
             *         Because this method internally uses legacy C functions, the parameter format must be a null-terminated C string.
             *
             * @param  timestamp The Unix Epoch timestamp as 64 bits unsigned integer.
             * @param  format    The format to use in the conversion. This is a null terminate string. See strftime formats. If NULL the default format is: "%F %T (%Z)"
             * @return           The string converted or empty if fails. This method fails if the format is incorrect.
            */
            template <typename Duration>
            static std::string TimestampToLocalString(uint64_t timestamp, const char *format = nullptr)
            {
                std::chrono::time_point<std::chrono::system_clock, Duration> tpoint { Duration{timestamp} };
                std::chrono::system_clock::time_point systp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(tpoint);

                return CAnsiStr::TimePointToString(false, systp, format);
            }

            /** @brief Method to get the current timestamp in seconds, milliseconds, microseconds or nanoseconds.
             *         So you must use the duration template to specify the output timestamp format.
             *         Example:
             *
             *              GetCurrentTimeStamp<std::chrono::seconds>()      --> Get cuerrent time stamp in seconds
             *              GetCurrentTimeStamp<std::chrono::milliseconds>() --> Get cuerrent time stamp in milliseconds
             *              GetCurrentTimeStamp<std::chrono::microseconds>() --> Get cuerrent time stamp in microseconds
             *              GetCurrentTimeStamp<std::chrono::nanoseconds>()  --> Get cuerrent time stamp in nanoseconds
             *
             *         Remember that UNIX EPOCH is measures time by the number of non-leap
             *         seconds that have elapsed since 00:00:00 UTC on 1 January 1970
             *
             *         So by default is in UTC.
             *
             * @return The current 64 bits Unix Epoch Timestamp number as seconds or milli or micro or nano seconds.
            */
            template <typename Duration>
            static int64_t GetCurrentTimeStamp()
            {
                auto now = std::chrono::system_clock::now().time_since_epoch();

                return std::chrono::duration_cast<Duration>(now).count();
            }
    };
//---------------------------------------------------------------------------------------------------------------------
}
#endif
