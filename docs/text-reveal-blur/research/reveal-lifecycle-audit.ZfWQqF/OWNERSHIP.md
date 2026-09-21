# Runtime Reveal Blur ownership audit (before code changes)

Legend: `=>` owning handle/attachment, `~>` weak or observer reference.

```text
Scene => Label/View => registered TextVisual => unique TextVisualRevealData
         |                                      => runtimeBlur companion
         => companion (scene child)                 ~> owner Label (WeakHandle)
         |                                          => original foreground handle
         |                                          => sequence TextureSets
         |                                          => Source/H/V/output actors
         |                                               => attached CameraActors
         |                                               => Renderer(s)
         |                                                  => Geometry => VertexBuffer
         |                                                  => TextureSet => Texture(s)
         |                                                  => applied Constraints
         |                                                       ~> source/target objects
         |                                          => Pass FrameBuffers => color Textures
         |                                          => Pass RenderTasks / RenderTaskList
         |                                               ~> source/camera actors (observers)
         |                                               => FrameBuffer
         |                                          => optional decoration composition
         |                                               => planes, actors, FBO, task
         |                                          => optional ImageCapture
         |                                               => hidden retention Actor
         |                                               => original image Renderer handles
         |                                               => capture proxy Renderer handles
         => InlineReplacementData attachment
              => host ~> owner Label (WeakHandle)
              => manager => Entry ImageVisual / Reveal Constraint
                            => BlurCaptureState => borrowed Renderer handles
                                                ~> companion client (WeakHandle)
                                                ~> retention Actor (WeakHandle)
```

## Teardown authority

- TextVisual::DoSetOffScene cancels a running async render, calls RemoveRenderer,
  and clears its weak control. RemoveRenderer first removes runtime blur.
- RemoveRuntimeRevealBlur moves the authoritative companion handle out first.
  ReleaseForeground marks it released, ends ImageSpan capture, and restores only
  borrowed foreground state. mForegroundBorrowed makes restoration one-shot.
  Unparent invokes OnSceneDisconnection: every pass/decoration task is removed
  from the scene list, its handle reset, then mTaskList reset. Companion destruction
  drops its actors, bindings, FBOs, sequence textures and foreground handles.
- RuntimeBlurActor has member/scene ownership, not an owning reference to its
  Label. RenderTask source/camera links are ActorObservers, not a reverse strong
  reference to the companion. A surviving task is still dangerous resource retention,
  hence explicit task-list removal is required and tested.
- Applied constraints are owned by the target Object; ConstraintBase observes
  source/target Objects and clears links on ObjectDestroyed. Blur functors capture
  scalar timing/geometry values, not strong Label/companion handles. Batched line
  properties/constraints therefore expire with their renderer, not a global registry.

## ImageSpan transfer

The original ImageVisual stays registered with Label/ViewDataImpl. Its renderer
is removed from the owner's draw list and attached to the invisible retention
actor, preserving connected constraint updates. Source renders a proxy sharing
its geometry/shader/textures and mirrored properties; it is not a second direct
draw of the original renderer.

CaptureUpdateScope releases the borrow before visual/timing mutation and prevents
recapture while nested updates are active. CaptureBlurRenderer validates client,
source revision, occurrence identity and current renderer. Repeated capture is
idempotent. ReleaseBlurCapture moves its state out first and restores only a
still-current entry/renderer to a live owner. ReleaseEntryVisual removes matching
retained sources before unregister/discard.

LabelImpl destruction disconnects resource-ready, calls PrepareOwnerDestruction,
removes attachment identity, clears the async interface, then discards TextVisual.
PrepareOwnerDestruction removes retained images/constraints without Self() or
visual unregister during owner destruction. ViewDataImpl retires registered visuals.
RemoveInlineReplacementData removes the old attachment identity before destruction,
so a reentrant new attachment cannot be erased by the old removal.

## Cancellation and caches

TextVisual publication compares reveal/render revisions, source/layout identity,
sync/async mode, scene/owner geometry and foreground identity around observable
allocation/property boundaries. Candidate ownership is published before activation
and withdrawn before rollback. Manager UpdateGuard owns only cancellation state,
not resources or the manager; generation/alive detect reentrant replacement/removal.

Shader caches, the scalar cached Geometry, Gaussian shared sample blocks and
framework caches intentionally outlive a Label. Warm them before weak/count tracking;
do not classify allocator RSS/high-water marks as per-Label leaks.

CreateBlurOutput callers prove !quarterSource && !batched implies alphaOnly:
the scalar call is selected only by alphaOnly || quarterBlur; scalar HIGH RGBA
uses CreatePlainOutput. No extra defensive guard or cache generalization is needed.
