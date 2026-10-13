//@name text-editable-group.vert
//@version 100
precision highp float;
INPUT highp vec2 aPosition;
OUTPUT highp vec2 vTexCoord;
UNIFORM_BLOCK VertBlock
{
  UNIFORM highp mat4 uMvpMatrix;
  UNIFORM highp vec3 uSize;
};
void main()
{
  vTexCoord = aPosition + 0.5;
  gl_Position = uMvpMatrix * vec4(aPosition * uSize.xy, 0.0, 1.0);
}
