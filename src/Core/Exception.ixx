export module Core.Exception;

import <windows.h>;
import <eh.h>;
import std;

namespace
Engine {
    thread_local bool handling_fatal_exception = false;
    
    std::string process_stack_trace(const std::stacktrace &trace) {
        std::string result = "stack trace:\n";

        if (trace.size() == 0) {
            return result + "  <no stack trace available>\n";
        }

        size_t display_index = 0;

        for (size_t start_index = handling_fatal_exception ? 1 : 0;
             start_index < trace.size();
             ++start_index) {
            const auto &frame = trace.at(start_index);
            if (frame.source_line() != 0) {
                result += std::format("  <{}> at {}:{} ({})\n",
                                      display_index++,
                                      frame.source_file().empty() ? "<unknown file>" : frame.source_file(),
                                      frame.source_line(), frame.description());
            }
             }

        handling_fatal_exception = false;

        return result;
    }
    
    export class RuntimeException : public std::runtime_error {
    public:
        RuntimeException(std::string_view message,
                                   const std::stacktrace &trace = std::stacktrace::current())
            : std::runtime_error(
                std::format("{}\n{}",
                            message,
                            process_stack_trace(trace)
                )) {}

    private:
        std::string my_message;
    };

    export class NullPointerException : public RuntimeException {
    public:
        NullPointerException(const std::stacktrace &trace = std::stacktrace::current())
            : RuntimeException("NullPointerException", trace) {}
    };

    export class IllegalAddressException : public RuntimeException {
    public:
        IllegalAddressException(
            const std::stacktrace &trace = std::stacktrace::current())
            : RuntimeException("IllegalAddressException", trace) {}
    };

    export class ArrayIndexOutOfBoundException : public RuntimeException {
    public:
        ArrayIndexOutOfBoundException(
            const std::stacktrace &trace = std::stacktrace::current())
            : RuntimeException("ArrayIndexOutOfBoundException", trace) {}
    };

    export class IntegerDivideByZeroException : public RuntimeException {
    public:
        IntegerDivideByZeroException(
            const std::stacktrace &trace = std::stacktrace::current())
            : RuntimeException("IntegerDivideByZeroException", trace) {}
    };

    export class StackOverflowError : public RuntimeException {
    public:
        StackOverflowError(
            const std::stacktrace &trace = std::stacktrace::current())
            : RuntimeException("StackOverflowError", trace) {}
    };

    export class UnknownSystemFatalException : public RuntimeException {
    public:
        UnknownSystemFatalException(
            const std::stacktrace &trace = std::stacktrace::current())
            : RuntimeException("UnknownSystemFatalException", trace) {}
    };
    
    void veh_fatal_exception_handler(unsigned int code, EXCEPTION_POINTERS *ep) {
        // if debugging, break into the debugger
#if defined(_DEBUG)
        if (IsDebuggerPresent()) {
            __debugbreak();
        }
#endif

        handling_fatal_exception = true;
        switch (code) {
            case EXCEPTION_ACCESS_VIOLATION: {
                if (ep->ExceptionRecord->ExceptionInformation[0] == 0) {
                    throw NullPointerException();
                }
                throw IllegalAddressException();
            }
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
                throw ArrayIndexOutOfBoundException();
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
                throw IntegerDivideByZeroException();
            case EXCEPTION_STACK_OVERFLOW:
                throw StackOverflowError();
            default:
                throw UnknownSystemFatalException();
        }
    }

    export void RegisterSystemFatalExceptionHandler() {
        _set_se_translator(veh_fatal_exception_handler);
    }
}
