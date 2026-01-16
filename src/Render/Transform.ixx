export module Render.Transform;

import Core.Prelude;
import Core.Utilities;
import "glm/gtx/transform.hpp";

namespace
Engine {
    export class ITransform {
    public:
        virtual ~ITransform() = default;

        virtual void OnFramebufferResized(float newWidth, float newHeight) = 0;

        virtual void DoTransform(glm::mat4 &matrix) = 0;
    };

    // export class RefTransform : public ITransform, public Engine::RefCounted<RefTransform> {
    //
    // };
    // export using RefTransform = RefInterface<ITransform>;
}
