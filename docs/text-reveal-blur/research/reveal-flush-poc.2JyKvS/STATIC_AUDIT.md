# Before implementation: bounded ordinary command-chain flush

The complete preceding source audit is [here](../reveal-backend-predictability.z457tX/BACKEND_AUDIT.md).
This turn re-read Begin/EndRenderPass, controller Flush/ProcessCommandBuffer/Queues,
texture dependencies, sync pool, resource/surface switching and external completion paths.

GL API flush sites in the current Adaptor (not controller queue drains):

| Site | Purpose/context | PoC |
|---|---|---|
| GLES Context::EndRenderPass | submit FBO write + forward fence, resource context | eligible ordinary chain only |
| Controller::ResolvePresentRenderTarget | submit new backward/native fence after surface present | unchanged |
| OffscreenRenderSurfaceEgl::PostRender (3 sites) | async export failure/success or no callback, offscreen surface context | unchanged |
| X11 PixmapRenderSurfaceX::PostRender; alternate ubuntu-x11 PixmapRenderSurfaceEcoreX::PostRender | submit before pixmap handoff | unchanged (both platform implementations) |
| GlImplementation::Flush | actual glFlush forwarding method | unchanged; actual GL calls counted by private interposer |

Memory2/3::Flush uploads/maps buffers; Controller::Flush drains CPU queues. Neither is
the API glFlush targeted here. XFlush is X11 protocol and unrelated. ReadPixels calls
glFinish; native sync client waits have their own flags. These are unchanged.

Candidate ownership: controller state applies only to its unique resource Context.
No global pending flag. Pending work cannot survive ProcessCommandQueues return.

Preflight each top-level command buffer, including secondary buffers, before executing it.
Only whitelisted ordinary FBO commands qualify. Flush pending before any ineligible buffer;
then use the original immediate behavior for that buffer. Consecutive eligible buffers can
share a final drain flush. No command queue copy, no resource changes, no command reordering.

Exclude: surface/present commands, explicit FLUSH command, readback, END with RenderTracker
sync, native draw/callback, native/external attachments/textures, unsupported resource
context, more than one surface, unknown commands. A defensive native-texture check in
Context also ends eligibility before native preparation, including inherited bindings.
This last check preserves native release behavior without assuming binds are self-contained.

Mandatory boundaries and disposition:

- Resource→surface/other context: ineligible buffer forces submission before its execution.
- Native/external/own callback: ineligible buffer; defensive native guard before texture prepare.
- Readback/REFRESH_ONCE tracker/explicit completion/explicit FLUSH: original path for whole buffer.
- End of outer command drain: always submits pending, including FBO-only/no window.
- Multiple windows: optimization disabled if >1 surface; original R→W1→R→W2 behavior.
- Unsupported resource context or unfamiliar non-FBO target: unchanged fallback.
- Resource destruction/disconnect/shutdown: pending cannot escape synchronous drain; no
  callback or context destruction is allowed in an eligible buffer. Discard queues run later.
- Recursive ProcessCommandBuffer: preflight understands secondaries; no premature flush at
  every secondary return. Bounded recursion guard falls back, never drops a command.

Fences, AddTextures, forward/backward maps, SyncPool allocation/wait/free, native release,
RenderTracker creation, pass/state cleanup remain unchanged. This is not fence coalescing.

Correctness rationale: ordered ordinary framebuffer writes/reads in one context do not
require a flush between each draw. Producer commands must be submitted before a different
context can wait on their fence; consumer flushing does not submit the producer.
References: [ES 3.2](https://registry.khronos.org/OpenGL/specs/es/3.2/es_spec_3.2.pdf),
[EGL fence sync](https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_fence_sync.txt).

Static verdict: a conservative bounded candidate is feasible. Performance remains unknown:
preflight has a cost; later submission may harm GPU overlap. Stop if PC benefit/tail gate fails.
