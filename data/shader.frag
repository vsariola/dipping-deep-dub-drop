#version 430

// SYNCS - do not touch this line, will be replaced with sync definitions
layout(location=0) uniform float syncs[20]; // location=0 ensures consistent location regardless of driver

out vec3 outcolor;

// ----------------------------
// CLIP
// ----------------------------        

#define R(a) mat2(cos(a),sin(a),-sin(a),cos(a))
const float Q = 6.;
const float MINDIST = .00001;
const vec2 N=vec2(.04,0);
const vec2 CENTER = vec2(.53101,.558);
const vec2 JULIAC = vec2(.25,-.6);
const vec3 LIGHTDIR = normalize(vec3(2,-1,-2));


vec2 julia2(vec2 z,vec2 c) {
    int i;
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
	return vec2(d,float(i)-log2(log(lz2)));    
}

vec2 map(vec3 p) {
    float zoom=exp(-syncs[ZOOM]+3.);
    vec2 d=julia2(p.xy*zoom-CENTER,JULIAC);
    return vec2(-p.z-exp(-d.x/zoom)*syncs[HEIGHT],d.y);
}


// ----------------------------
// CLAP
// ----------------------------  

void main() {    
    vec3 d = normalize(vec3((2*gl_FragCoord.xy-vec2(@XRES@,@YRES@))/@YRES@,1.4));         
    // ----------------------------
    // CLIP
    // ----------------------------       
    
    float td,dist;
    float a = syncs[CAM_YZ];
    d.yz *= R(a);
    vec3 p = vec3(0,0,-2);
    p.yz *= R(a);
    
    
    for (int i=0;i<300;i++)
       if (td+=dist=map(p).x*.3,p+=d*dist,dist<MINDIST)
            break; 
    
    
    vec2 m = map(p);
    vec3 n = normalize(m.x-vec3(map(p-N.xyy).x,map(p-N.yxy).x,map(p-N.yyx).x)); 
    float fog = exp(-td/2.);
    vec3 col = mix(vec3(0),(.5+.5*sin(m.y*.1+vec3(0,1,2)))*max(dot(LIGHTDIR,n),0.)*10.,fog);                    
        
    // ----------------------------
    // CLAP
    // ----------------------------                   
    outcolor = pow(atan(col),vec3(.45));        
}
