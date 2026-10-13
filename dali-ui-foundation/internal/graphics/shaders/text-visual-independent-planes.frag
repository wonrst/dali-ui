//@name text-visual-independent-planes.frag

//@version 100

precision highp float;

INPUT highp vec2 vFillUv;
UNIFORM sampler2D sTexture;
#ifdef IS_REQUIRED_STYLE
INPUT highp vec2 vUnderlayUv;
UNIFORM sampler2D sStyle;
#endif
#ifdef IS_REQUIRED_OVERLAY
INPUT highp vec2 vOverlayUv;
UNIFORM sampler2D sOverlayStyle;
#endif
#ifdef IS_REQUIRED_EMOJI
UNIFORM sampler2D sMask;
#endif

UNIFORM_BLOCK FragBlock
{
  UNIFORM lowp vec4 uTextColorAnimatable;
  UNIFORM lowp vec4 uColor;
#ifdef IS_REQUIRED_EMOJI
  UNIFORM lowp float uHasMultipleTextColors;
  UNIFORM mediump vec4 uTextColorBaked;
#endif
};

void main()
{
  // Each texture has a transparent one-texel border. Linear CLAMP_TO_EDGE
  // sampling outside its storage bounds is therefore transparent as well.
#if defined(IS_REQUIRED_MULTI_COLOR) || defined(IS_REQUIRED_EMOJI)
  mediump vec4 color = TEXTURE(sTexture, vFillUv);
#ifdef IS_REQUIRED_EMOJI
  mediump float mask = TEXTURE(sMask, vFillUv).r;
  // Replace only the surviving monochrome write; native RGBA stays baked.
  color = clamp(color + (uTextColorAnimatable - uTextColorBaked) * mask * (1.0 - uHasMultipleTextColors), 0.0, 1.0);
#endif
#else
  mediump vec4 color = uTextColorAnimatable * TEXTURE(sTexture, vFillUv).r;
#endif
#ifdef IS_REQUIRED_STYLE
  color += TEXTURE(sStyle, vUnderlayUv) * (1.0 - color.a);
#endif
#ifdef IS_REQUIRED_OVERLAY
  mediump vec4 overlay = TEXTURE(sOverlayStyle, vOverlayUv);
  color = overlay + color * (1.0 - overlay.a);
#endif
  gl_FragColor = uColor * color;
}
