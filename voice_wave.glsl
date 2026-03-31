void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord/iResolution.xy * 2. - 1.;
    uv.x *= iResolution.x / iResolution.y;
    
    uv.x += sin(cos(uv.x*sin(uv.x/2.)-iTime)+iTime)*5.;
    
    vec3 uvRGB = vec3(uv.y,uv.y,uv.y);
    
    uvRGB.x += (cos(uv.x)*sin(iTime)+cos(uv.x+iTime*2.))*0.3;
    uvRGB.y += (cos(uv.x+2.)*sin(iTime-2.)+cos(uv.x+iTime*2.))*0.3;
    uvRGB.z += (cos(uv.x+4.)*sin(iTime-4.)+cos(uv.x+iTime*2.))*0.3;
    
    float a = smoothstep(0., abs(uvRGB.x), 0.08);
    float b = smoothstep(0., abs(uvRGB.y), 0.08);
    float c = smoothstep(0., abs(uvRGB.z), 0.08);
    
    vec3 aRGB = vec3(a) * vec3(0.973,0.188,0.384);
    vec3 bRGB = vec3(b) * vec3(0.110,0.949,0.475);
    vec3 cRGB = vec3(c) * vec3(0.067,0.475,0.910);
    
    fragColor = vec4(aRGB + bRGB + cRGB,1.0);
}