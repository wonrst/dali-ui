#pragma once

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
#include <dali-ui-foundation/public-api/dali-ui-common.h>
#include <dali-ui-foundation/public-api/view.h>

namespace Dali
{

namespace Ui
{

// Forward declarations
namespace Integration
{
class TextVisualizerImpl;
}

/**
 * @brief TextVisualizer is a read-only View intended for dynamic text layout.
 *
 * This commit only introduces the public handle / internal implementation skeleton.
 */
class DALI_UI_API TextVisualizer : public View
{
public: // Creation & Destruction
  /**
   * @brief Creates an uninitialized TextVisualizer handle.
   *
   * Only derived versions can be instantiated. Calling member
   * functions with an uninitialized Dali::Object is not allowed.
   */
  TextVisualizer();

  /**
   * @brief Creates an initialized TextVisualizer.
   *
   * @return A handle to a newly allocated Dali resource
   */
  static TextVisualizer New();

  /**
   * @brief Copy constructor.
   *
   * Creates another handle that points to the same real object.
   * @param[in] textVisualizer Handle to copy
   */
  TextVisualizer(const TextVisualizer& textVisualizer);

  /**
   * @brief Move constructor.
   *
   * @param[in] rhs Handle to move
   */
  TextVisualizer(TextVisualizer&& rhs) noexcept;

  /**
   * @brief Virtual destructor.
   *
   * This is non-virtual since derived Handle types must not contain data or virtual methods.
   */
  ~TextVisualizer();

public: // Operators
  /**
   * @brief Copy assignment operator.
   *
   * Changes this handle to point to another real object.
   * @param[in] handle Object to assign this to
   * @return Reference to this
   */
  TextVisualizer& operator=(const TextVisualizer& handle);

  /**
   * @brief Move assignment operator.
   *
   * @param[in] rhs Object to assign this to
   * @return Reference to this
   */
  TextVisualizer& operator=(TextVisualizer&& rhs) noexcept;

public: // Static Methods
  /**
   * @brief Downcasts a handle to TextVisualizer handle.
   *
   * If handle points to a TextVisualizer, the downcast produces valid handle.
   * If not, the returned handle is left uninitialized.
   *
   * @param[in] handle Handle to an object
   * @return A handle to a TextVisualizer or an uninitialized handle
   */
  static TextVisualizer DownCast(BaseHandle handle);

public:
  explicit DALI_UI_API TextVisualizer(Integration::TextVisualizerImpl& implementation);
  explicit DALI_UI_API TextVisualizer(Dali::Internal::CustomActor* internal);
};

} // namespace Ui

} // namespace Dali
