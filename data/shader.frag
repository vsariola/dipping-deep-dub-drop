#version 430

// SYNCS - do not touch this line, will be replaced with sync definitions
layout(location=0) uniform float syncs[17*RKT_NUMTRACKS+1]; // location=0 ensures consistent location regardless of driver
uniform sampler2D postSampler; // the scene is first rendered and copied to texture 1 for post-processing

out vec4 outcolor;

const int BDR = RKT_NUMTRACKS+1;
const int SNARE = RKT_NUMTRACKS+2;

// ----------------------------
// CLIP
// ----------------------------        

#define R(a) mat2(cos(a),sin(a),-sin(a),cos(a))
const float Q = 6.;
const float MINDIST = .00001;
const vec2 N=vec2(.04,0);
vec3 LIGHTDIR = normalize(vec3(syncs[LIGHT_X],syncs[LIGHT_Y],-1));
vec2 CENTER = vec2(syncs[CAM_X],syncs[CAM_Y]);


vec2 julia(vec2 z) {
    int i;
    z /= 2.;
    vec2 c = vec2(syncs[JULIA_CX],syncs[JULIA_CY]);
    float ld2 = 1.0;
    float lz2 = dot(z,z);
    for(i=0; i<256; i++ )
	{
        ld2 *= 4.0*lz2;
        z = vec2( z.x*z.x - z.y*z.y, 2.0*z.x*z.y ) + c;
        lz2 = dot(z,z);
		if( lz2>200.0 ) break;
	}
    float d = sqrt(lz2/ld2)*log(lz2);            
    d = atan(d*10.)/10.;
	return vec2(d,float(i)-log2(log(lz2)));    
}

vec2 henge(vec2 z) {
    const int STEPS = 20;
    const float RMUL = 1.3;
    const float RDIV = pow(RMUL,float(STEPS));
    vec2 r = vec2(0);    
    for(int i=0;i<STEPS;i++) {         
        z.x=cos(4.*z.x);        
        r =r*RMUL+z;        
        z =z.yx*R(syncs[HENGE_R]);
    }
    return r/RDIV*vec2(.01,30);
}


vec2 foobar(vec2 z) {
    const int STEPS = 15;
    const float RMUL = 1.2;
    const float RDIV = pow(RMUL,float(STEPS));
    float r = 0.;
    for(int i=0;i<STEPS;i++) {         
        z =z.yx*R(syncs[FOO_R]);
        z.x-=round(z.x);                
        z.x *= 2.;
        r =r*RMUL+z.x*z.x;             
        
    }
    return vec2(r*.1/RDIV,r*20./RDIV);    
}

vec2 frac(vec2 z,float p) {
    if (p>1.)
        return mix(henge(z),foobar(z),p-1.);
    return mix(julia(z),henge(z),p);
}

vec2 map(vec3 p) {
    float logdist = log(length(p));
    float r = -logdist+syncs[ZOOM];
    int ring = int(floor(r));
    vec2 d,d2; 
    for (int i=0;i<2;i++) {
        d2 = d;
        int ring2 = max(ring,0);
        float zoom = exp((float(ring2)-syncs[ZOOM]));
        vec2 u = p.xy*R(float(ring2))*zoom-CENTER;            
        int m = min(max(ring2/30+1,1),3);       
        d = frac(u,syncs[FRACSELECT]);        
        d.x = -d.x/zoom*(syncs[HEIGHT]+syncs[BDR]/9.);
        ring++;
    }
    float t = mod(r,1.);    
    d = mix(d2,d,t);    
    return vec2(-p.z-d.x,d.y);
}



// ----------------------------
// CLAP
// ----------------------------  

vec2 iResolution = vec2(@XRES@,@YRES@);

const float GA =2.399; 
const mat2 rot = mat2(cos(GA),sin(GA),-sin(GA),cos(GA));

// 	simplyfied version of Dave Hoskins blur
vec3 dof(sampler2D tex,vec2 uv,float rad)
{
	vec3 acc=vec3(0);
    vec2 pixel=vec2(.001,.001),angle=vec2(0,rad*syncs[CAM_BLUR]);
    rad=1.;
	for (int j=0;j<80;j++)
    {  
        rad += 1./rad;
	    angle*=rot;
        vec4 col=texture(tex,uv+pixel*(rad-1.)*angle);        
		acc+=tan(col.xyz);                
	}
    return sqrt(atan(acc/80.));	
}

void main() {    
    vec2 u = 2*gl_FragCoord.xy-iResolution;    
    vec3 d = normalize(vec3(u/iResolution.y,1.4)),q=vec3(.5);    

    u/=iResolution;
    if (syncs[ROW]<0) { // negative time indicates we should do post-processing       	
        vec2 uv = gl_FragCoord.xy/iResolution;
	    outcolor=vec4(dof(postSampler,gl_FragCoord.xy/iResolution,texture(postSampler,uv).w),1.);
        return;
    }

    // ----------------------------
    // CLIP
    // ----------------------------       
    
    float td,dist;    
    vec3 p = vec3(0,0,-2);    
    d.xz *= R(syncs[CAM_XZ]);
    p.xz *= R(syncs[CAM_XZ]);
    d.yz *= R(syncs[CAM_YZ]);
    p.yz *= R(syncs[CAM_YZ]);   
    
    for (int i=0;i<200;i++)
       if (td+=dist=map(p).x,p+=d*dist,dist<MINDIST)
            break; 
    
    
    vec2 m = map(p);
    vec3 n = normalize(m.x-vec3(map(p-N.xyy).x,map(p-N.yxy).x,map(p-N.yyx).x)); 
    float fog = exp(-td/2.);
    vec3 col = mix(vec3(0),(.5+.5*sin(m.y*.1+vec3(0,1,1.5)))*max(dot(LIGHTDIR,n),0.)*10.,fog);   
    // ----------------------------
    // CLAP
    // ----------------------------                   
    outcolor = vec4(atan(col),abs(syncs[CAM_DEPTH]-td)/4.);        
}
