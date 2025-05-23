#version 430

// SYNCS - do not touch this line, will be replaced with sync definitions
layout(location = 0) uniform sampler2D textSampler; // the scene is first rendered and copied to texture 1 for post-processing
layout(location = 1, binding = 1) uniform sampler2D postSampler; // the scene is first rendered and copied to texture 1 for post-processing
layout(location = 2) uniform float syncs[10+RKT_NUMTRACKS+1]; // location=0 ensures consistent location regardless of driver

out vec4 outcolor;

const int SNARE = RKT_NUMTRACKS+1;
const int DUBCHORD = RKT_NUMTRACKS+2;
const int BLLL = RKT_NUMTRACKS+3;
const int SNRFX = RKT_NUMTRACKS+4;

// ----------------------------
// CLIP
// ----------------------------        

#define R(a) mat2(cos(a),sin(a),-sin(a),cos(a))
const float MINDIST = .0001;
const vec2 N=vec2(.04,0);
vec3 LIGHTDIR = normalize(vec3(syncs[LIGHT_X],syncs[LIGHT_Y],-1));
vec2 CENTER = vec2(syncs[CAM_X],syncs[CAM_Y]);


vec2 julia(vec2 z) {
    int i;
    z /= 2.;
    vec2 c = vec2(syncs[JULIA_CX],syncs[JULIA_CY]);
    float ld2 = 1.0;
    float lz2 = dot(z,z);
    for(i=0; i<199; i++ )
	{
        ld2 *= 4.0*lz2;
        z = vec2( z.x*z.x - z.y*z.y, 2.0*z.x*z.y ) + c;
        lz2 = dot(z,z);
		if( lz2>200.0 ) break;
	}
    float d = sqrt(lz2/ld2)*log(lz2);                
	return vec2(atan(d*10.)/10.,float(i)-log2(log(lz2)));    
}

vec2 henge(vec2 z) {
    const int STEPS = 10;
    const float RMUL = 1.3;
    const float RDIV = pow(RMUL,float(STEPS));
    vec2 r = vec2(0);    
    for(int i=0;i<STEPS;i++) {         
        z.x=cos(4.*z.x+syncs[HENGE_SHIFT]);        
        r = r*RMUL+z;        
        z =z.yx*R(syncs[HENGE_ROT]);
    }
    return r/RDIV*vec2(syncs[HENGE_HEIGHT],syncs[HENGE_COLOR]);
}


vec2 kale(vec2 z) {
    const int STEPS = 20;
    for(int i = 0; i < STEPS; i++){
        z = abs(z)-1;        
        z *= syncs[KALE_SCALE]*R(syncs[KALE_ROT]);
    }
    return z*vec2(syncs[KALE_HEIGHT],syncs[KALE_COLOR]);
}

vec2 frac(vec3 p, float ring) {
    ring = clamp(ring,0,107);    
    float zoom = exp((ring-syncs[ZOOM]));
    vec2 u = p.xy*R(ring*ring)*zoom-CENTER;                
        
    vec2 d = syncs[FRACSELECT]>1. ? mix(henge(u),kale(u),syncs[FRACSELECT]-1.) : mix(julia(u),henge(u),syncs[FRACSELECT]);
     
    if (ring >= 107) {
        d.y *= 1-texture(textSampler,(u+CENTER)*vec2(1,1.8)+.5+vec2(-.17,.05)).r;
    }
    d.x = d.x/zoom*(syncs[HEIGHT]+syncs[BLLL]/3)-p.z;
    return d;
}

vec2 map(vec3 p) {           
    float r = -log(length(p))+syncs[ZOOM];    
    float ring = floor(r);    
    return mix(frac(p,ring),frac(p,ring+1),mod(r,1.));        
}

float hash(vec2 p)
{
	return fract(sin(dot(p*1e3, vec2(12.9898, 78.233))) * 43758.5453);   
}


// ----------------------------
// CLAP
// ----------------------------  

void main() {    
    vec2 res = vec2(@XRES@,@YRES@);
    vec2 uv = (2*gl_FragCoord.xy-res)/res.y;
    uv.x += hash(uv.yy)*syncs[DUBCHORD]/3.;
    vec3 d = normalize(vec3(uv,1.4));
    vec3 col = vec3(0);        

    float td = 0,dist;    
        
    if (syncs[ROW]<0) { // negative time indicates we should do post-processing       	                                                
        uv = gl_FragCoord.xy/res;
        vec2 angle=vec2(0,texture(postSampler,uv).w*syncs[DOF_BLUR]*.001);
        float rad=1.;                
	    for (int j=0;j<80;j++)
        {  
            rad += 1./rad;
	        angle*=R(2.4);                
		    col +=texture(postSampler,uv+(rad-1.)*angle).xyz;                
	    }        
        col = pow(col/80.,vec3(.45)); // gamma correction
        
    } else {

    // ----------------------------
    // CLIP
    // ----------------------------       
        
    vec3 p = vec3(0,0,-2);    
    d.xz *= R(syncs[CAM_XZ]);
    p.xz *= R(syncs[CAM_XZ]);
    d.yz *= R(syncs[CAM_YZ]);
    p.yz *= R(syncs[CAM_YZ]);   
    d.xy *= R(syncs[CAM_XY]);
    p.xy *= R(syncs[CAM_XY]);   
   
    for (int i=0;i<99;i++)
       if (td+=dist=map(p).x*.6,p+=d*dist,abs(dist)<MINDIST)
            break; 
    
    vec2 m = map(p);
    vec3 n = normalize(m.x-vec3(map(p-N.xyy).x,map(p-N.yxy).x,map(p-N.yyx).x)); 
    col = .5+.5*sin(m.y*syncs[COLOR_MULT]+vec3(syncs[COLOR_R],syncs[COLOR_G],syncs[COLOR_B]));
    col = atan(  
        mix(col, vec3(0), isnan(col)) // there was some nans from fractals, so avoid white pixels
        * (max(dot(LIGHTDIR,n),0.)+syncs[SNARE]/2.) // lighting
        * exp(-td/syncs[FOG]) // fog
        * syncs[COLOR_FADE] // fade    
    ); // atan tonemapping
    // ----------------------------
    // CLAP
    // ----------------------------                        
    }
    // dithering
    outcolor = vec4(
        col - hash(uv)/256, // some dithering, purposely towards 0 to increase blacks
        abs(syncs[DOF_DEPTH]-td)/4.  // distance from focus saved in alpha for DOF
    );    
}
