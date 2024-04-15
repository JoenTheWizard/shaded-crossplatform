///<music id="1A2tMtBiBRo" loop="true">
//#version 330 core
uniform vec2 iResolution;
uniform float iTime;

#define PI 3.14159265359

const int MAX_MARCHING_STEPS = 255;
const float MIN_DIST = 0.0;
const float MAX_DIST = 100.0;
const float PRECISION = 0.001;

struct Surface {
    float sd;
    vec3 col;
};

mat2 rot(float t) {
    return mat2(cos(t), sin(t), -sin(t), cos(t));
}

Surface minWithColor(Surface obj1, Surface obj2) {
  if (obj2.sd < obj1.sd) return obj2;
  return obj1;
}

Surface ears(vec3 p, vec2 c, float h, vec3 col){
  float q = length(p.xz);
  return Surface(max(dot(c.xy,vec2(q,p.y)),-h-p.y),col);
}

Surface blink(vec3 p,vec3 a,vec3 b,float r,vec3 col){
  vec3 pa = p - a, ba = b - a;
  float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
  return Surface(length( pa - ba*h )-r,col);
}

Surface head(vec3 p, vec3 r, vec3 col){
	float k0 = length(p/r);
	float k1 = length(p/(r*r));
	return Surface(k0*(k0-1.0)/k1, col);
}

float smoothUnion( float d1, float d2, float k ) {
    float h = clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) - k*h*(1.0-h);
}

Surface newRem(Surface s1, Surface s2, float strength) {
	float newD = smoothUnion(s1.sd,s2.sd, strength);
	return Surface(newD, s1.col);
}

float opSmoothSubtraction( float d1, float d2, float k ) {
    float h = clamp( 0.5 - 0.5*(d2+d1)/k, 0.0, 1.0 );
    return mix( d2, -d1, h ) + k*h*(1.0-h); }


Surface newRem1(Surface s1, Surface s2, float strength) {
	float newD = opSmoothSubtraction(s1.sd,s2.sd, strength);
	return Surface(newD, s1.col);
}

Surface sdScene(vec3 ty) {
  vec3 p = ty;
  p.y += 0.25*cos(1.5*iTime);
  vec3 cat_col = vec3(1., .4, .0);
  Surface sphereLeft =
  	head(p-vec3(0,0,-0.5), vec3(1.,.9,.73), cat_col);
	Surface leftEar = ears(p-vec3(.45,1.35,-.5),vec2(2.2,0.7),0.78,vec3(0.));
	Surface rightEar = ears(p-vec3(-.45,1.35,-.5),vec2(2.2,0.7),0.78,vec3(0.));
	Surface s1 = newRem(newRem(sphereLeft,leftEar,0.4), rightEar,0.4);
	
	Surface bl1 = blink(p-vec3(-.2,.2,-.24), vec3(-.25,.25,.5), vec3(0.,0.,0.46), .01, vec3(0.));
	Surface bl2 = blink(p-vec3(-.2,.2,-.24), vec3(-.25,-.25,0.5), vec3(0.,0.,0.46), .01, vec3(0.));
	
	Surface eye0 = head(p-vec3(.25,.1,-.05),
		vec3(.22,0.37,0.29), vec3(1.));
	Surface eye1 = head(p-vec3(0.25,.08,-.045),
		vec3(.22,0.37,0.29), vec3(0,1,1));
	
	Surface whisker0 = blink(p-vec3(-.75,-.2,-.24), vec3(-.25,.25,.5), vec3(.5,0.,.46), .01, vec3(0.));
	Surface whisker1 = blink(p-vec3(-.75,-.2,-.24), vec3(-.25,-.25,.5), vec3(.5,0.,.46), .01, vec3(0.));
	
	Surface whisker2 = blink(p-vec3(.75,-.2,-.24), vec3(.25,.25,.5), vec3(-.5,0.,.46), .01, vec3(0.));
	Surface whisker3 = blink(p-vec3(.75,-.2,-.24), vec3(.25,-.25,.5), vec3(-.5,0.,.46), .01, vec3(0.));
	
	Surface mouth_0 = head(p-vec3(0.,-.01,.08), vec3(.39,.3,.2), vec3(.94,.37,.52));
	Surface mouth_1 = head(p-vec3(0.,-.23,.1), vec3(.23,.45,.2), vec3(0.));
	Surface mL = newRem1(mouth_0,mouth_1,0.05);
	
  Surface c = minWithColor(s1, mL);
  c = minWithColor(minWithColor(c, bl1),bl2);
  c = minWithColor(minWithColor(minWithColor(c, eye0),eye1),whisker0);
  c = minWithColor(minWithColor(minWithColor(c, whisker1),whisker2),whisker3);
  return c;
}

Surface rayMarch(vec3 ro, vec3 rd, float start, float end) {
  float depth = start;
  Surface co; // closest object

  for (int i = 0; i < MAX_MARCHING_STEPS; i++) {
    vec3 p = ro + depth * rd;
    co = sdScene(p);
    depth += co.sd;
    if (co.sd < PRECISION || depth > end) break;
  }
  
  co.sd = depth;
  
  return co;
}

vec3 calcNormal(in vec3 p) {
    vec2 e = vec2(1.0, -1.0) * 0.0005; // epsilon
    return normalize(
      e.xyy * sdScene(p + e.xyy).sd +
      e.yyx * sdScene(p + e.yyx).sd +
      e.yxy * sdScene(p + e.yxy).sd +
      e.xxx * sdScene(p + e.xxx).sd);
}

//Background
float heart(vec2 p) {
    p.y -= 1.6;
    float t = atan(p.y, p.x);
    float s = sin(t);
    float r = 2.-2.*s+s*(sqrt(abs(cos(t)))/(s+1.4));
    return r+2.;
}

vec3 hue(in float c) {
    return cos(2.0*PI*c + 2.0*PI/3.0*vec3(3,2,1))*0.5+0.5;
}

float map(vec3 p) {
    p.y = -p.y;
    p.y += p.z*p.z*.05;
    float r = heart(p.xy);
    return length(p.xy)-r;
}

vec3 fullBackground(vec2 uv){
	 vec3 dir = normalize(vec3(uv, 0.7));
    dir.yz *= rot(-0.4);
    
    float tot = 0.0;
    for (int i = 0; i < 100 ; i++) {
        vec3 p = tot*dir;
        tot += map(p)*0.9;
    }
	return hue(tot*0.3-iTime*2.7);
}
//End of background

void main()
{
  vec2 uv = (gl_FragCoord.xy-.5*iResolution.xy)/iResolution.y;
  vec3 backgroundColor = fullBackground(uv);

  vec3 col = vec3(0);
  vec3 ro = vec3(0, 0, cos(15.5*iTime)+4.);
  vec3 rd = normalize(vec3(uv, -1)); // ray direction

  Surface co = rayMarch(ro, rd, MIN_DIST, MAX_DIST);

  if (co.sd > MAX_DIST) {
    col = backgroundColor;
  } else {
    vec3 p = ro + rd * co.sd;
    vec3 normal = calcNormal(p);
    vec3 lightPosition = vec3(2, 1, 7);
    vec3 lightDirection = normalize(lightPosition - p);

    float dif = clamp(dot(normal, lightDirection), 0.3, 1.);
    
    col = dif * co.col + vec3(1.3)*.25;
  }

  // Output to screen
  gl_FragColor = vec4(col, 1.0);
  
  float brightness = (gl_FragColor.r + gl_FragColor.g + gl_FragColor.g) / 3.; 
  float shade = floor(brightness * 3.);
  float brighnessOfShade = shade / 3.;
  float factor = brightness / brighnessOfShade;
  if (!(co.sd > MAX_DIST)) 
  	gl_FragColor.rgb /= vec3(factor);
  
}

