#ifndef EXERROR_DEF_HPP
#define EXERROR_DEF_HPP
//---------------------------------------------------------------------------------------

/** @file exerrdef.hpp
 *  @author Juan Carlos García Vázquez (gavajc@gmail.com)
 *  @date   2025-03-20
 *  @brief  File that contain classes to handle error codes & custom exceptions.
 *          The error code structure store the error information like error code number,
 *          description message, and a category name char * pointer to a valid an persistent
 *          memory block like std::error_category name.
 * 
 *          We define it this way because many C++ classes do not handle std::error_codes and 
 *          because before boost 1.77 there was no interoperability between boost and std.
 * 
 *          Therefore, creating a custom std::error_category is not useful; this method is 
 *          better and easier for exchanging error information between boost and std.
 * 
 *          We define as well a custom exception class derived from std::exception
 *          with the composition of the SEECode structure.
 * 
 *          Both, the structure and the class use the default movement constructor an assignment operator.
 */

#include <string>
#include <memory>
#include <cstdint>
#include <ostream>
#include <exception>

#if __cplusplus >= 201703L
    #include <string_view>
#endif
 //---------------------------------------------------------------------------------------

namespace ExecErr
{
    /**
     *
     * @struct ErrorFamily
     * @brief  Structure that store the 3 default error families to avoid duplicates before C++17
     * 
     *              NO ERROR
     *           LOGIC ERROR
     *          STDLIB ERROR
     *
     *         In your libraries, source files you can define your own families.
     *         But remember you must define as persistent memory until the program ends like constexpr.
    */
    struct ErrorFamily
    {
        static const char* None() noexcept {
            ///< Without category. It is used to express that there aren't errors.
            static constexpr const char* name = "not.family";
            return name;
        }

        static const char* Logic() noexcept {
            ///< Represent a logic error. For example invalid params 
            static constexpr const char* name = "logic.error";
            return name;
        }

        static const char* StdLib() noexcept {
            ///< Represent a C++ runtime or logic exception or error like std::map.at or file could't opened, ...
            static constexpr const char* name = "stdlib.error";
            return name;
        }
    };
    //---------------------------------------------------------------------------------------

    /**
     *
     * @struct SEECode
     * @brief  Structure that store execution error information.
     *         i.e. If fail, the code number, category name & message.
     *
    */
    struct SEECode
    {
        bool fail;           ///< True if the error code was set or false otherwise.
        int32_t code;        ///< The error code number. 
        uint32_t line;       ///< Store the file line. Set when use the EXECERR_THROW macro.
        const char *file;    ///< Store the filename. Set when use the EXECERR_THROW macro.
        const char *func;    ///< the function/method name. Set when use the EXECERR_THROW macro.
        const char *name;    ///< Pointer to a const & persistent block of memory with the name of the category error.
        std::string message; ///< The error message to show.

        /** @brief Constructor that fill the error code.
         *
         * @param failed  True if has error or false if not.
         * @param ecode   The error code number.
         * @param catName The category string name. Is a pointer to a static or const string and persisten block of memory.
         * @param emsg    The error message string.
        */
        #if __cplusplus >= 201703L
            SEECode(bool failed = false, int32_t ecode = 0, const char *catName = ErrorFamily::None(), 
                    std::string_view emsg = {}, const char *srcFile = "", const char *funcName = "", uint32_t srcLine = 0) :
                fail(failed), code(ecode), line(srcLine), file(srcFile ? srcFile : ""), func(funcName ? funcName : ""), name(catName ? catName : ""), message(emsg) {
            }
        #else
            SEECode(bool failed = false, int32_t ecode = 0, const char *catName = ErrorFamily::None(),
                    std::string emsg = "", const char *srcFile = "", const char *funcName = "", uint32_t srcLine = 0) :
                fail(failed), code(ecode), line(srcLine), file(srcFile ? srcFile : ""), func(funcName ? funcName : ""), name(catName ? catName : ""), message(std::move(emsg)) {
            }
        #endif

        /// @brief Operator that return true if error code is set or false if not.
        explicit operator bool() const noexcept {
            return fail;
        }

        /// @brief Operator to format the error to output stream.
        friend inline std::ostream& operator<<(std::ostream &stream, const SEECode &error)
        {
            if (stream && error.fail)
            {
                stream << '[' << error.name << ':' << error.code << "] " << error.message;
                if (error.file[0] != '\0')
                {
                    stream << " (at ";
                    if (error.func[0] != '\0')
                        stream << error.func << " in ";
                    
                    stream << error.file << ':' << error.line << ')';
                }
            }
            return stream;
        }

        /** @brief Method to set an error information.
         *
         * @param failed  True if error is set or false if not.
         * @param ecode   Error code number.
         * @param catName Static string or const expression for the category name.
         * @param emsg    The error message string.
        */
        #if __cplusplus >= 201703L
            void Set(bool failed, int32_t ecode, const char *catName, std::string_view emsg, const char *srcFile = "", const char *srcFunc = "", uint32_t srcLine = 0)
        #else
            void Set(bool failed, int32_t ecode, const char *catName, std::string emsg, const char *srcFile = "", const char *srcFunc = "", uint32_t srcLine = 0)
        #endif
        {
            fail = failed;
            code = ecode;
            line = srcLine;
            file = srcFile ? srcFile : "";
            func = srcFunc ? srcFunc : "";
            name = catName ? catName : "";

            #if __cplusplus >= 201703L
                message.assign(emsg);
            #else
                message = std::move(emsg);
            #endif
        }

        /// @brief  Method to clear the error code information.
        void Clear()
        {
            fail = false;
            code = 0;
            line = 0;
            file = "";
            func = "";
            name = ErrorFamily::None();
            message.clear();
        }

        /** @brief Method tha allow to concatenate error message string with a previous error.
         *
         * @param emsg The message to concatenate.
        */
        #if __cplusplus >= 201703L
            void ConcatenateMessage(std::string_view emsg)
        #else
            void ConcatenateMessage(std::string emsg)
        #endif
        {
            if (!message.empty())
                message.append(". ",2);

            #if __cplusplus >= 201703L
                message.append(emsg);
            #else
                message += std::move(emsg);
            #endif
        }
    };

    /**
     * @class CEEException
     * @brief Class for throw custom exceptions with the custom error code structure.
     *
    */
    class CEEException : public std::exception
    {
    private:

        SEECode m_error;

    public:

        ///< Constructor with movable error code argument.
        explicit CEEException(SEECode &&error) : m_error(std::move(error)) {
        }

        ///< Constuctor with a copyable error code argument.
        explicit CEEException(const SEECode &error) : m_error(error) {
        }

        ///< Constructor with failed, code, and copy family, message
        #if __cplusplus >= 201703L
            CEEException(int32_t code, const char *catName, std::string_view message,
                         const char *srcFile = "", const char *srcFunc = "", uint32_t srcLine = 0)
                : m_error(true, code, catName, message, srcFile, srcFunc, srcLine) {
            }
        #else
            CEEException(int32_t code, const char *catName, std::string message,
                         const char *srcFile = "", const char *srcFunc = "", uint32_t srcLine = 0)
                : m_error(true, code, catName, std::move(message), srcFile, srcFunc, srcLine) {
            }
        #endif

        int32_t             code()     const noexcept { return m_error.code;            }
        uint32_t            line()     const noexcept { return m_error.line;            }
        const char        * category() const noexcept { return m_error.name;            }
        const char        * file()     const noexcept { return m_error.file;            }
        const SEECode     & error()    const noexcept { return m_error;                 }
        const std::string & message()  const noexcept { return m_error.message;         }

        const char * what() const noexcept override { 
            return !m_error.message.empty() ? m_error.message.c_str() 
                                            : "Unknown exception";
        }
    };
}
//---------------------------------------------------------------------------------------

///< Detect what compiler is used to set the best function/method name
#if defined(_MSC_VER)
    #define __EXECERR_FUNCTION_MACRO__ __FUNCSIG__
#elif defined(__GNUC__) || defined(__clang__)
    #define __EXECERR_FUNCTION_MACRO__ __PRETTY_FUNCTION__
#else
    #define __EXECERR_FUNCTION_MACRO__ __func__
#endif

///< Macro to throw CEEException exceptions with __FILE__ AND __LINE__
#define EXECERR_THROW(code, family, message) \
    throw ExecErr::CEEException((code), (family), (message), __FILE__, __EXECERR_FUNCTION_MACRO__, __LINE__)

///< Macro to throw CEEException from an existing SEECode (preserves its original file/line/func context)
#define EXECERR_THROW_CODE(ec) throw ExecErr::CEEException(ec)

///< Macro to Create SEECode with __FILE__ AND __LINE__
#define EXECERR_EECODE(failed, code, family, message) \
    ExecErr::SEECode((failed), (code), (family), (message), __FILE__, __EXECERR_FUNCTION_MACRO__, __LINE__)

///< Macro to move SEECode from an existing SEECode (preserves its original file/line/func context)
#define EXECERR_MOVE_CODE(ec) ExecErr::SEECode(std::move(ec))
//---------------------------------------------------------------------------------------
#endif
