export module Core.Utilities:Tags;

namespace
Engine {
    namespace ResourceOwnership {
        export inline namespace Tags {
            struct Static {};

            struct AutoManaged {};

            struct Transferred {};

            struct Shared {};
        }
    }
}