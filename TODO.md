Fix the fucking stupid hack for mutex exposure in Imgui of Nvrhi, or possibly rewrite the whole backend of ImGui.
Figure out if the fence is neccessary for presentation.
Hopefully not have to hack NVRHI again.
Implement off-screen Rendering logic, need to create frame buffers and renderer.
Add maximum uploads per frame to the resource uploader to avoid stalling the GPU, this is very low priority.
Fucking stop and figure out what the fuck I am doing with the framebuffer and pipeline creation.