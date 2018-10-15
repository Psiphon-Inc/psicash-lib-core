#ifndef PSICASHLIB_ERROR_H
#define PSICASHLIB_ERROR_H

#include <string>
#include <vector>
#include "nonstd/expected.hpp"
#include "utils.h"


namespace error {

    class Error {
    public:
        Error();

        Error(const Error &src);

        Error(const std::string& message, const std::string& filename, const std::string& function, int line);

        // Wrapping a non-error results in a non-error (i.e., is a no-op). This allows it to be done
        // unconditionally without introducing an error where there isn't one.
        // Returns *this.
        Error &Wrap(const std::string& message, const std::string& filename, const std::string& function, int line);

        Error &Wrap(const std::string& filename, const std::string& function, int line);

        operator bool() const;

        std::string ToString() const;

    private:
        // Indicates that this error is actually set. (There must be a more elegant way to do this...)
        bool is_error_;

        struct StackFrame {
            std::string message;
            std::string filename;
            std::string function;
            int line;
        };
        std::vector<StackFrame> stack_;
    };

    const Error nullerr;

#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __func__
#endif
#define MakeError(message)         (error::Error((message), __FILE__, __PRETTY_FUNCTION__, __LINE__))
#define WrapError(err, message)    (err.Wrap((message), __FILE__, __PRETTY_FUNCTION__, __LINE__))
#define PassError(err)             (err.Wrap(__FILE__, __PRETTY_FUNCTION__, __LINE__))

    template<typename T>
    class Result : public nonstd::expected<T, Error> {
    public:
        Result() = delete;

        Result(const T &val) : nonstd::expected<T, Error>(val) {}

        Result(const Error &err) : nonstd::expected<T, Error>(
                (nonstd::unexpected_type<Error>) err) {}
    };

}

#endif //PSICASHLIB_ERROR_H
