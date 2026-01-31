export module Render.RenderAPI;

import Core.Exception;
import Core.Prelude;

namespace Engine {
    export enum class RHIAPI {
        Unknown = 0,
        NVRHI,
        Vuk
    };

    export class RHIIncompatibleException : public RuntimeException {
    public:
        RHIIncompatibleException(std::string_view message, std::stacktrace trace = std::stacktrace::current())
            : RuntimeException(std::format("RHIIncompatibleException: {}", message), trace) {}
    };
}