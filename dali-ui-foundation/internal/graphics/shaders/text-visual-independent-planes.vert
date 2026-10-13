//@name text-visual-independent-planes.vert

//@version 100

precision highp float;

INPUT highp vec2 aPosition;

OUTPUT highp vec2 vFillUv;
#if defined(IS_REQUIRED_TEXT_GRADIENT) || defined(IS_REQUIRED_TEXT_GRADIENT_OVERLAY) || defined(IS_REQUIRED_EMBOSS)
OUTPUT highp vec2 vTexCoord;
#endif
#ifdef IS_REQUIRED_STYLE
OUTPUT highp vec2 vUnderlayUv;
#endif
#ifdef IS_REQUIRED_OVERLAY
OUTPUT highp vec2 vOverlayUv;
#endif

UNIFORM_BLOCK PlaneBlock
{
  UNIFORM highp vec4 uFillUvTransform;
#if defined(IS_REQUIRED_TEXT_GRADIENT) || defined(IS_REQUIRED_TEXT_GRADIENT_OVERLAY) || defined(IS_REQUIRED_EMBOSS)
  UNIFORM highp vec4 uTextCoordTransform;
#endif
#ifdef IS_REQUIRED_STYLE
  UNIFORM highp vec4 uUnderlayUvTransform;
#endif
#ifdef IS_REQUIRED_OVERLAY
  UNIFORM highp vec4 uOverlayUvTransform;
#endif
};

UNIFORM_BLOCK VertBlock
{
  UNIFORM highp mat4 uMvpMatrix;
  UNIFORM highp vec3 uSize;
  UNIFORM highp float viewEffectiveScale;
  UNIFORM highp vec3 uScale;
  UNIFORM highp float pixelSnapFactor;
  UNIFORM lowp  float visualTransformUseEffectiveScale;
};

UNIFORM_BLOCK VisualVertBlock
{
  //Visual size and offset
  UNIFORM highp vec2 offset;
  UNIFORM highp vec2 size;
  UNIFORM highp vec2 extraSize;
  UNIFORM mediump vec4 offsetSizeMode;
  UNIFORM mediump vec2 origin;
  UNIFORM mediump vec2 pivot;
};

vec4 ComputeVertexPosition()
{
  highp float effectiveScale = mix(1.0, viewEffectiveScale, visualTransformUseEffectiveScale);
  vec2 visualSize = mix(size * uSize.xy, size * effectiveScale, offsetSizeMode.zw ) + extraSize * effectiveScale;
  vec2 visualOffset = mix(offset * uSize.xy, offset * effectiveScale, offsetSizeMode.xy);
  vec4 result = vec4( (aPosition + pivot) * visualSize + visualOffset + origin * uSize.xy, 0.0, 1.0 );

  vec2 snappedPosition = result.xy;
  snappedPosition.x = floor(snappedPosition.x * uScale.x + 0.5) / uScale.x;
  snappedPosition.y = floor(snappedPosition.y * uScale.y + 0.5) / uScale.y;

  snappedPosition.x = snappedPosition.x + (1.0 - abs(mod(uSize.x, 2.0) - 1.0)) * 0.5;
  snappedPosition.y = snappedPosition.y + (1.0 - abs(mod(uSize.y, 2.0) - 1.0)) * 0.5;

  result.xy = mix(result.xy, snappedPosition, pixelSnapFactor);

  return result;
}

void main()
{
  // q maps composition edges to [0,1]; texel centers follow by interpolation.
  highp vec2 q = aPosition + vec2(0.5);
  vFillUv = q * uFillUvTransform.xy + uFillUvTransform.zw;
#if defined(IS_REQUIRED_TEXT_GRADIENT) || defined(IS_REQUIRED_TEXT_GRADIENT_OVERLAY) || defined(IS_REQUIRED_EMBOSS)
  vTexCoord = q * uTextCoordTransform.xy + uTextCoordTransform.zw;
#endif
#ifdef IS_REQUIRED_STYLE
  vUnderlayUv = q * uUnderlayUvTransform.xy + uUnderlayUvTransform.zw;
#endif
#ifdef IS_REQUIRED_OVERLAY
  vOverlayUv = q * uOverlayUvTransform.xy + uOverlayUvTransform.zw;
#endif
  gl_Position = uMvpMatrix * ComputeVertexPosition();
}
