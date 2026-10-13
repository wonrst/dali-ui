#ifndef DALI_UI_EDITABLE_TEXT_GROUP_H
#define DALI_UI_EDITABLE_TEXT_GROUP_H

/*
 * Copyright (c) 2026 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

// EXTERNAL INCLUDES
#include <dali/public-api/actors/actor.h>
#include <dali/public-api/actors/camera-actor.h>
#include <dali/public-api/animation/constraint.h>
#include <dali/public-api/render-tasks/render-task-list.h>
#include <dali/public-api/render-tasks/render-task.h>
#include <dali/public-api/signals/connection-tracker.h>

namespace DALI_NAMESPACE::Ui::Text
{
class Renderer;

/** @brief Composes editable text paint before applying inherited color and opacity. */
class EditableTextGroup : public ConnectionTracker
{
public:
  static bool RequiresComposition(const Renderer& renderer, bool background, bool selection);

  explicit EditableTextGroup(Actor viewport);
  ~EditableTextGroup();

  // The viewport clips the text group; caret and handles remain outside it.
  void Update(Actor text, Actor background, Actor replacements, Actor cursor);

private:
  void OnScene(Actor actor);
  void OffScene(Actor actor);
  void UpdateTarget();
  void ReleaseTarget();

  Actor          mViewport;
  Actor          mSource;
  Actor          mOutput;
  Actor          mBackground;
  CameraActor    mCamera;
  RenderTaskList mTasks;
  RenderTask     mTask;
  Constraint     mBackgroundPosition;
  Vector2        mSize;
};
} // namespace DALI_NAMESPACE::Ui::Text

#endif // DALI_UI_EDITABLE_TEXT_GROUP_H
