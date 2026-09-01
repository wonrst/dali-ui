//@name text-visual-shader.frag

//@version 100

precision highp float;

INPUT highp vec2 vTexCoord;
UNIFORM sampler2D sTexture;
#ifdef IS_REQUIRED_TEXT_GRADIENT_MIXED
UNIFORM sampler2D sTextGradientMask;
#endif
#ifdef IS_REQUIRED_TEXT_GRADIENT
UNIFORM sampler2D sGradientLookup;
#endif
#ifdef IS_REQUIRED_TEXT_GRADIENT_OVERLAY
UNIFORM sampler2D sGradientOverlayLookup;
#endif
#ifdef IS_REQUIRED_STYLE
UNIFORM sampler2D sStyle;
#endif
#ifdef IS_REQUIRED_OVERLAY
UNIFORM sampler2D sOverlayStyle;
#endif

#ifndef IS_REQUIRED_TEXT_GRADIENT
  #ifdef IS_REQUIRED_MULTI_COLOR
  #elif defined(IS_REQUIRED_EMOJI)
  // Single color with emoji.
  UNIFORM sampler2D sMask;
  #endif
#endif

#ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR_PRESERVED
UNIFORM sampler2D sTextRevealFadeBlurPreserved;
#endif
#ifdef IS_REQUIRED_TEXT_REVEAL
UNIFORM sampler2D sTextRevealMetadata;
#endif
#ifdef IS_REQUIRED_TEXT_REVEAL_SEQUENCE_BLUR_PROTOTYPE
UNIFORM sampler2D sTextRevealSequenceBlurPrototype;
#endif

UNIFORM_BLOCK FragBlock
{
  #ifndef IS_REQUIRED_TEXT_GRADIENT
    #ifdef IS_REQUIRED_MULTI_COLOR
    #elif defined(IS_REQUIRED_EMOJI)
    // Single color with emoji.
    UNIFORM lowp float uHasMultipleTextColors;
    #endif
  #endif
  UNIFORM lowp vec4 uTextColorAnimatable;
  UNIFORM lowp vec4 uColor;

  #ifdef IS_REQUIRED_TEXT_GRADIENT
  UNIFORM highp float uTextGradientType;
  UNIFORM highp vec2 uTextGradientStartPosition;
  UNIFORM highp vec2 uTextGradientEndPosition;
  UNIFORM highp vec2 uTextGradientRadialCenter;
  UNIFORM highp vec2 uTextGradientRadialScale;
  UNIFORM highp vec2 uTextGradientConicCenter;
  UNIFORM highp vec2 uTextGradientConicScale;
  UNIFORM highp float uTextGradientConicStartAngle;
  UNIFORM highp float uTextGradientStartOffset;
  UNIFORM highp vec4 uTextGradientBounds;
  #endif

  #ifdef IS_REQUIRED_TEXT_GRADIENT_OVERLAY
  UNIFORM highp float uTextGradientOverlayType;
  UNIFORM highp vec2 uTextGradientOverlayStartPosition;
  UNIFORM highp vec2 uTextGradientOverlayEndPosition;
  UNIFORM highp vec2 uTextGradientOverlayRadialCenter;
  UNIFORM highp vec2 uTextGradientOverlayRadialScale;
  UNIFORM highp vec2 uTextGradientOverlayConicCenter;
  UNIFORM highp vec2 uTextGradientOverlayConicScale;
  UNIFORM highp float uTextGradientOverlayConicStartAngle;
  UNIFORM highp float uTextGradientOverlayStartOffset;
  UNIFORM highp vec4 uTextGradientOverlayBounds;
  UNIFORM highp float uTextGradientOverlayMode;
  #endif

  #ifdef IS_REQUIRED_EMBOSS
  UNIFORM lowp vec2 uEmbossSize;
  UNIFORM lowp vec2 uEmbossDirection;
  UNIFORM lowp float uEmbossStrength;
  UNIFORM lowp vec4 uEmbossLightColor;
  UNIFORM lowp vec4 uEmbossShadowColor;
  #endif

  #ifdef IS_REQUIRED_TEXT_REVEAL
  UNIFORM highp float uTextRevealProgress;
  UNIFORM highp float uTextRevealFadeDuration;
    #ifdef IS_REQUIRED_TEXT_REVEAL_SEQUENCE_BLUR_PROTOTYPE
    UNIFORM highp float uTextRevealSequenceBlurEnabled;
    UNIFORM highp float uTextRevealSequenceBlurDurationRatio;
    #endif
  #endif
};

#ifdef IS_REQUIRED_TEXT_GRADIENT
highp float EvaluateTextGradientPosition(highp vec2 textGradientCoord)
{
  const highp float TEXT_GRADIENT_TYPE_RADIAL = 2.0;
  const highp float TEXT_GRADIENT_TYPE_CONIC = 3.0;
  const highp float TEXT_GRADIENT_INV_TWO_PI = 0.15915494309189533576888376337251;
  if(abs(uTextGradientType - TEXT_GRADIENT_TYPE_RADIAL) < 0.5)
  {
    return length((textGradientCoord - uTextGradientRadialCenter) * uTextGradientRadialScale);
  }
  if(abs(uTextGradientType - TEXT_GRADIENT_TYPE_CONIC) < 0.5)
  {
    highp vec2 conicVector = (textGradientCoord - uTextGradientConicCenter) * uTextGradientConicScale;
    highp float angle = atan(conicVector.y, conicVector.x) - uTextGradientConicStartAngle;
    return fract(angle * TEXT_GRADIENT_INV_TWO_PI);
  }

  highp vec2 gradientVector = uTextGradientEndPosition - uTextGradientStartPosition;
  highp float gradientLengthSquared = max(dot(gradientVector, gradientVector), 0.000001);
  return dot(textGradientCoord - uTextGradientStartPosition, gradientVector) / gradientLengthSquared;
}
#endif

#ifdef IS_REQUIRED_TEXT_GRADIENT_OVERLAY
highp float EvaluateTextGradientOverlayPosition(highp vec2 textGradientCoord)
{
  const highp float TEXT_GRADIENT_TYPE_RADIAL = 2.0;
  const highp float TEXT_GRADIENT_TYPE_CONIC = 3.0;
  const highp float TEXT_GRADIENT_INV_TWO_PI = 0.15915494309189533576888376337251;
  if(abs(uTextGradientOverlayType - TEXT_GRADIENT_TYPE_RADIAL) < 0.5)
  {
    return length((textGradientCoord - uTextGradientOverlayRadialCenter) * uTextGradientOverlayRadialScale);
  }
  if(abs(uTextGradientOverlayType - TEXT_GRADIENT_TYPE_CONIC) < 0.5)
  {
    highp vec2 conicVector = (textGradientCoord - uTextGradientOverlayConicCenter) * uTextGradientOverlayConicScale;
    highp float angle = atan(conicVector.y, conicVector.x) - uTextGradientOverlayConicStartAngle;
    return fract(angle * TEXT_GRADIENT_INV_TWO_PI);
  }

  highp vec2 gradientVector = uTextGradientOverlayEndPosition - uTextGradientOverlayStartPosition;
  highp float gradientLengthSquared = max(dot(gradientVector, gradientVector), 0.000001);
  return dot(textGradientCoord - uTextGradientOverlayStartPosition, gradientVector) / gradientLengthSquared;
}

mediump vec4 ApplyTextGradientOverlay(mediump vec4 baseFill, highp vec2 texCoord)
{
  mediump float glyphAlpha = baseFill.a;
  if(glyphAlpha <= 0.000001)
  {
    return baseFill;
  }

  highp vec2 textGradientOverlayCoord =
    (texCoord - uTextGradientOverlayBounds.xy) / max(uTextGradientOverlayBounds.zw, vec2(0.000001));
  highp float gradientPosition = EvaluateTextGradientOverlayPosition(textGradientOverlayCoord);
  mediump vec4 overlayColor =
    TEXTURE(sGradientOverlayLookup, vec2(gradientPosition + uTextGradientOverlayStartOffset, 0.5));
  mediump vec3 baseRgb = baseFill.rgb / max(glyphAlpha, 0.000001);
  mediump vec3 blendedRgb;

  const highp float TEXT_GRADIENT_OVERLAY_MODE_SCREEN = 1.0;
  if(abs(uTextGradientOverlayMode - TEXT_GRADIENT_OVERLAY_MODE_SCREEN) < 0.5)
  {
    mediump vec3 screen = vec3(1.0) - (vec3(1.0) - baseRgb) * (vec3(1.0) - overlayColor.rgb);
    blendedRgb = mix(baseRgb, screen, overlayColor.a);
  }
  else
  {
    blendedRgb = overlayColor.rgb * overlayColor.a + baseRgb * (1.0 - overlayColor.a);
  }
  return vec4(blendedRgb * glyphAlpha, glyphAlpha);
}
#endif

#ifdef IS_REQUIRED_TEXT_REVEAL
highp float ResolveTextRevealOpacity(highp vec2 texCoord)
{
  mediump vec4 revealMetadata = TEXTURE(sTextRevealMetadata, texCoord);
  highp float encodedStart =
    floor(revealMetadata.r * 255.0 + 0.5) * 256.0 + floor(revealMetadata.g * 255.0 + 0.5);
  highp float normalizedStart = encodedStart / 65535.0;
  highp float revealProgress = clamp(uTextRevealProgress, 0.0, 1.0);
  highp float localOpacity;
  if(uTextRevealFadeDuration <= 0.000001)
  {
    // The second term preserves the public progress==0 fully-hidden endpoint,
    // including the first unit whose scheduled start is zero.
    localOpacity = step(normalizedStart, revealProgress) * step(0.000001, revealProgress);
  }
  else
  {
    localOpacity = clamp((revealProgress - normalizedStart) / uTextRevealFadeDuration, 0.0, 1.0);
  }
  localOpacity = mix(1.0, localOpacity, step(0.5, revealMetadata.b));
  // Defensive global endpoint: malformed/missing metadata must not expose a
  // reveal-target foreground pixel at progress zero. FinalRevealPlan is still
  // responsible for assigning valid metadata to every rendered glyph.
  localOpacity *= sign(max(revealProgress, 0.0));
  // Fixed-point start metadata can round slightly above the theoretical
  // schedule. Preserve the public fully-revealed endpoint independently of
  // that quantization.
  return mix(localOpacity, 1.0, step(1.0, revealProgress));
}
#endif

#ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR
highp float ResolveTextRevealFadeBlurProgress(highp float normalizedStart)
{
  highp float revealProgress = clamp(uTextRevealProgress, 0.0, 1.0);
  highp float localProgress;
  if(uTextRevealFadeDuration <= 0.000001)
  {
    localProgress = step(normalizedStart, revealProgress) * step(0.000001, revealProgress);
  }
  else
  {
    localProgress = clamp((revealProgress - normalizedStart) / uTextRevealFadeDuration, 0.0, 1.0);
  }
  localProgress *= sign(max(revealProgress, 0.0));
  return mix(localProgress, 1.0, step(1.0, revealProgress));
}

#ifdef IS_REQUIRED_TEXT_REVEAL_SEQUENCE_BLUR_PROTOTYPE
highp float ResolveTextRevealSequenceSharpProgress(highp float sequenceStart, highp float sequenceEnd)
{
  highp float duration = max((sequenceEnd - sequenceStart) *
                              clamp(uTextRevealSequenceBlurDurationRatio, 0.0, 1.0),
                            0.000001);
  return clamp((clamp(uTextRevealProgress, 0.0, 1.0) - sequenceStart) / duration, 0.0, 1.0);
}
#endif
#endif

void main()
{
#ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR
  mediump vec4 textRevealFadeBlurMetadata = TEXTURE(sTextRevealMetadata, vTexCoord);
  highp float sharpEncodedStart =
    floor(textRevealFadeBlurMetadata.r * 255.0 + 0.5) * 256.0 +
    floor(textRevealFadeBlurMetadata.g * 255.0 + 0.5);
  highp float sharpRevealProgress = ResolveTextRevealFadeBlurProgress(sharpEncodedStart / 65535.0);
  highp float blurRevealProgress = ResolveTextRevealFadeBlurProgress(textRevealFadeBlurMetadata.b);
#ifdef IS_REQUIRED_TEXT_REVEAL_SEQUENCE_BLUR_PROTOTYPE
  mediump vec4 sequenceBlurTiming = TEXTURE(sTextRevealSequenceBlurPrototype, vTexCoord);
  highp float sharpSequenceProgress = ResolveTextRevealSequenceSharpProgress(sequenceBlurTiming.r,
                                                                              sequenceBlurTiming.g);
  highp float blurSequenceProgress = ResolveTextRevealSequenceSharpProgress(sequenceBlurTiming.b,
                                                                             sequenceBlurTiming.a);
#endif
  mediump vec4 fadeBlurColor;
#endif

#ifdef IS_REQUIRED_STYLE
  mediump vec4 styleTexture = TEXTURE( sStyle, vTexCoord );
#endif

#ifdef IS_REQUIRED_OVERLAY
  mediump vec4 overlayStyleTexture = TEXTURE( sOverlayStyle, vTexCoord );
#endif

  mediump vec4 textColor;
#ifdef IS_REQUIRED_TEXT_GRADIENT_MIXED
  mediump vec4 preservedColor = TEXTURE(sTexture, vTexCoord);
  mediump float textTexture = TEXTURE(sTextGradientMask, vTexCoord).r;
  highp vec2 textGradientCoord =
    (vTexCoord - uTextGradientBounds.xy) / max(uTextGradientBounds.zw, vec2(0.000001));
  highp float gradientPosition = EvaluateTextGradientPosition(textGradientCoord);
  mediump vec4 gradientColor = TEXTURE(sGradientLookup, vec2(gradientPosition + uTextGradientStartOffset, 0.5));
  mediump vec4 gradientFill = vec4(gradientColor.rgb * textTexture,
                                   gradientColor.a * textTexture * uTextColorAnimatable.a);
  textColor = gradientFill + preservedColor * (1.0 - gradientFill.a);
#ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR
  mediump float fadeBlurCoverage = textRevealFadeBlurMetadata.a;
  mediump vec4 fadeBlurGradientFill = vec4(gradientColor.rgb * fadeBlurCoverage,
                                           gradientColor.a * fadeBlurCoverage * uTextColorAnimatable.a);
  #ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR_PRESERVED
  mediump vec4 fadeBlurPreserved = TEXTURE(sTextRevealFadeBlurPreserved, vTexCoord);
  fadeBlurColor = fadeBlurGradientFill + fadeBlurPreserved * (1.0 - fadeBlurGradientFill.a);
  #else
  fadeBlurColor = fadeBlurGradientFill;
  #endif
#endif
#elif defined(IS_REQUIRED_TEXT_GRADIENT)
  mediump float textTexture = TEXTURE(sTexture, vTexCoord).r;
  highp vec2 textGradientCoord =
    (vTexCoord - uTextGradientBounds.xy) / max(uTextGradientBounds.zw, vec2(0.000001));
  highp float gradientPosition = EvaluateTextGradientPosition(textGradientCoord);
  mediump vec4 gradientColor = TEXTURE(sGradientLookup, vec2(gradientPosition + uTextGradientStartOffset, 0.5));
  textColor = vec4(gradientColor.rgb * textTexture, gradientColor.a * textTexture * uTextColorAnimatable.a);
#ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR
  mediump float fadeBlurCoverage = textRevealFadeBlurMetadata.a;
  fadeBlurColor = vec4(gradientColor.rgb * fadeBlurCoverage,
                       gradientColor.a * fadeBlurCoverage * uTextColorAnimatable.a);
#endif
#elif defined(IS_REQUIRED_MULTI_COLOR) || defined(IS_REQUIRED_EMOJI)
  // Multiple color or use emoji.
  textColor = TEXTURE(sTexture, vTexCoord);
#ifdef IS_REQUIRED_EMBOSS
  // Multiple color or use emoji, with emboss
  mediump float textAlpha = textColor.a;
  mediump vec2 offset = normalize(uEmbossDirection) * uEmbossSize * uEmbossStrength;
  float light = TEXTURE(sTexture, clamp(vTexCoord - offset, 0.0, 1.0)).a;
  float shadow = TEXTURE(sTexture, clamp(vTexCoord + offset, 0.0, 1.0)).a;

#ifdef IS_REQUIRED_TEXT_REVEAL
  // Bevel extends the foreground with neighboring texture samples. Apply the
  // timing owned by each source sample so its halo cannot precede the glyph.
  textAlpha *= ResolveTextRevealOpacity(vTexCoord);
  light *= ResolveTextRevealOpacity(clamp(vTexCoord - offset, 0.0, 1.0));
  shadow *= ResolveTextRevealOpacity(clamp(vTexCoord + offset, 0.0, 1.0));
#define TEXT_REVEAL_RESOLVED_IN_EMBOSS
#endif

  vec4 lightColor = vec4(uEmbossLightColor.rgb * light, light);
  vec4 shadowColor = vec4(uEmbossShadowColor.rgb * shadow, shadow);
  vec4 embossColor = lightColor + shadowColor;

  vec4 baseColor = vec4(uTextColorAnimatable.rgb * textAlpha, textAlpha);
  textColor = embossColor * (1.0 - textAlpha) + baseColor;
  textColor.a = max(textAlpha, embossColor.a);
#endif
#endif

#ifdef IS_REQUIRED_TEXT_GRADIENT
#elif defined(IS_REQUIRED_MULTI_COLOR)
#ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR
  mediump vec4 fadeBlurDefaultColor = uTextColorAnimatable * textRevealFadeBlurMetadata.a;
  #ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR_PRESERVED
  mediump vec4 fadeBlurPreserved = TEXTURE(sTextRevealFadeBlurPreserved, vTexCoord);
  fadeBlurColor = fadeBlurDefaultColor + fadeBlurPreserved * (1.0 - fadeBlurDefaultColor.a);
  #else
  fadeBlurColor = fadeBlurDefaultColor;
  #endif
#endif
#elif defined(IS_REQUIRED_EMOJI)
  // Single color with emoji.
  mediump float maskTexture = TEXTURE(sMask, vTexCoord).r;

  // Set the color of non-transparent pixel in text to what it is animated to.
  // Markup text with multiple text colors are not animated (but can be supported later on if required).
  // Emoji color are not animated.
  mediump float vstep = step( 0.0001, textColor.a );
  textColor.rgb = mix(textColor.rgb, uTextColorAnimatable.rgb, vstep * maskTexture * (1.0 - uHasMultipleTextColors));
#ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR
  mediump vec4 fadeBlurDefaultColor = uTextColorAnimatable * textRevealFadeBlurMetadata.a;
  #ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR_PRESERVED
  mediump vec4 fadeBlurPreserved = TEXTURE(sTextRevealFadeBlurPreserved, vTexCoord);
  fadeBlurColor = fadeBlurDefaultColor + fadeBlurPreserved * (1.0 - fadeBlurDefaultColor.a);
  #else
  fadeBlurColor = fadeBlurDefaultColor;
  #endif
#endif
#elif defined(IS_REQUIRED_EMBOSS)
// Single color with emboss, without emoji.
  mediump float textAlpha = TEXTURE(sTexture, vTexCoord).r;
  mediump vec2 offset = normalize(uEmbossDirection) * uEmbossSize * uEmbossStrength;
  float light = TEXTURE(sTexture, clamp(vTexCoord - offset, 0.0, 1.0)).r;
  float shadow = TEXTURE(sTexture, clamp(vTexCoord + offset, 0.0, 1.0)).r;

#ifdef IS_REQUIRED_TEXT_REVEAL
  // Bevel extends the foreground with neighboring texture samples. Apply the
  // timing owned by each source sample so its halo cannot precede the glyph.
  textAlpha *= ResolveTextRevealOpacity(vTexCoord);
  light *= ResolveTextRevealOpacity(clamp(vTexCoord - offset, 0.0, 1.0));
  shadow *= ResolveTextRevealOpacity(clamp(vTexCoord + offset, 0.0, 1.0));
#define TEXT_REVEAL_RESOLVED_IN_EMBOSS
#endif

  vec4 lightColor = vec4(uEmbossLightColor.rgb * light, light);
  vec4 shadowColor = vec4(uEmbossShadowColor.rgb * shadow, shadow);
  vec4 embossColor = lightColor + shadowColor;

  vec4 baseColor = vec4(uTextColorAnimatable.rgb * textAlpha, textAlpha);
  textColor = embossColor * (1.0 - textAlpha) + baseColor;
  textColor.a = max(textAlpha, embossColor.a);

#else
  // Single color without emoji.
  mediump float textTexture = TEXTURE(sTexture, vTexCoord).r;
  textColor = uTextColorAnimatable * textTexture;
#ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR
  fadeBlurColor = uTextColorAnimatable * textRevealFadeBlurMetadata.a;
#endif
#endif

#ifdef IS_REQUIRED_TEXT_GRADIENT_OVERLAY
  textColor = ApplyTextGradientOverlay(textColor, vTexCoord);
#ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR
  fadeBlurColor = ApplyTextGradientOverlay(fadeBlurColor, vTexCoord);
#endif
#endif

#ifdef IS_REQUIRED_TEXT_REVEAL
#ifdef IS_REQUIRED_TEXT_REVEAL_FADE_BLUR
  // p^2 sharp + p(1-p) preblur preserves opacity ~= p while reducing blur to zero.
#ifdef IS_REQUIRED_TEXT_REVEAL_SEQUENCE_BLUR_PROTOTYPE
  mediump vec4 unitLocalBlurColor =
    textColor * (sharpRevealProgress * sharpRevealProgress) +
    fadeBlurColor * (blurRevealProgress * (1.0 - blurRevealProgress));
  mediump vec4 sequenceBlurColor =
    textColor * sharpRevealProgress * sharpSequenceProgress +
    fadeBlurColor * blurRevealProgress * (1.0 - blurSequenceProgress);
  textColor = mix(unitLocalBlurColor,
                  sequenceBlurColor,
                  step(0.5, uTextRevealSequenceBlurEnabled));
#else
  textColor = textColor * (sharpRevealProgress * sharpRevealProgress) +
              fadeBlurColor * (blurRevealProgress * (1.0 - blurRevealProgress));
#endif
#else
#ifndef TEXT_REVEAL_RESOLVED_IN_EMBOSS
  // textColor is premultiplied, so scale rgb and alpha together.
  textColor *= ResolveTextRevealOpacity(vTexCoord);
#endif
#endif
#endif

  // Draw the text as overlay above the style
  gl_FragColor = uColor * (
#ifdef IS_REQUIRED_OVERLAY
                   (
#endif
                     textColor
#ifdef IS_REQUIRED_STYLE
                     + styleTexture * (1.0 - textColor.a)
#endif
#ifdef IS_REQUIRED_OVERLAY
                   ) * (1.0 - overlayStyleTexture.a) + overlayStyleTexture
#endif
                 );
}
