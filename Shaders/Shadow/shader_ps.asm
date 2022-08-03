//
// Generated by Microsoft (R) HLSL Shader Compiler 10.1
//
//
// Buffer Definitions: 
//
// cbuffer cbPerPass
// {
//
//   float4x4 gView;                    // Offset:    0 Size:    64 [unused]
//   float4x4 gProj;                    // Offset:   64 Size:    64 [unused]
//   int gLightCount;                   // Offset:  128 Size:     4
//   float3 gEyePos;                    // Offset:  132 Size:    12
//
// }
//
// Resource bind info for gLights
// {
//
//   struct Light
//   {
//       
//       float3 Strength;               // Offset:    0
//       float FalloffStart;            // Offset:   12
//       float3 Direction;              // Offset:   16
//       float FalloffEnd;              // Offset:   28
//       float3 Position;               // Offset:   32
//       float SpotPower;               // Offset:   44
//
//   } $Element;                        // Offset:    0 Size:    48
//
// }
//
//
// Resource Bindings:
//
// Name                                 Type  Format         Dim      ID      HLSL Bind  Count
// ------------------------------ ---------- ------- ----------- ------- -------------- ------
// gLights                           texture  struct         r/o      T0      t0,space1      1 
// cbPerPass                         cbuffer      NA          NA     CB0            cb1      1 
//
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// WORLD_POSITION           0   xyz         1     NONE   float   xyz 
// LIGHT_POSITION           0   xyzw        2     NONE   float       
// COLOR                    0   xyzw        3     NONE   float   xyz 
// NORMAL                   0   xyz         4     NONE   float   xyz 
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
ps_5_1
dcl_globalFlags refactoringAllowed | skipOptimization
dcl_constantbuffer CB0[1:1][9], immediateIndexed, space=0
dcl_resource_structured T0[0:0], 48, space=1
dcl_input_ps linear v1.xyz
dcl_input_ps linear v3.xyz
dcl_input_ps linear v4.xyz
dcl_output o0.xyzw
dcl_temps 9
//
// Initial variable locations:
//   v0.x <- pin.posProj.x; v0.y <- pin.posProj.y; v0.z <- pin.posProj.z; v0.w <- pin.posProj.w; 
//   v1.x <- pin.posWorld.x; v1.y <- pin.posWorld.y; v1.z <- pin.posWorld.z; 
//   v3.x <- pin.color.x; v3.y <- pin.color.y; v3.z <- pin.color.z; v3.w <- pin.color.w; 
//   v4.x <- pin.normal.x; v4.y <- pin.normal.y; v4.z <- pin.normal.z; 
//   v2.x <- pin
// <internal error> could not get find UDT child with correct offset in PDB

//
#line 135 "E:\Projects\Project1\Project1\Shaders\Shadow\shader.hlsl"
itof r0.xyzw, l(0, 0, 0, 1)  // r0.x <- color.x; r0.y <- color.y; r0.z <- color.z; r0.w <- color.w

#line 136
nop 

#line 137
mov r1.x, l(0)  // r1.x <- i
mov r2.xyzw, r0.xyzw  // r2.x <- color.x; r2.y <- color.y; r2.z <- color.z; r2.w <- color.w
mov r1.y, r1.x  // r1.y <- i
loop 
  ilt r1.z, r1.y, CB0[1][8].x
  breakc_z r1.z

#line 140
  mov r3.xyz, v3.xyzx  // r3.x <- v.albedo.x; r3.y <- v.albedo.y; r3.z <- v.albedo.z

#line 141
  mov r4.xyz, v4.xyzx  // r4.x <- v.normal.x; r4.y <- v.normal.y; r4.z <- v.normal.z

#line 142
  mov r5.xyz, v1.xyzx  // r5.x <- v.pos.x; r5.y <- v.pos.y; r5.z <- v.pos.z

#line 145
  nop 
  mov r3.xyz, r3.xyzx
  mov r4.xyz, r4.xyzx
  mov r5.xyz, r5.xyzx
  ld_structured r6.x, r1.y, l(0), T0[0].xxxx
  ld_structured r6.y, r1.y, l(4), T0[0].xxxx
  ld_structured r6.z, r1.y, l(8), T0[0].xxxx
  ld_structured r7.x, r1.y, l(16), T0[0].xxxx
  ld_structured r7.y, r1.y, l(20), T0[0].xxxx
  ld_structured r7.z, r1.y, l(24), T0[0].xxxx

#line 109
  nop 

#line 111
  mul r8.xyz, r3.xyzx, l(0.300000, 0.300000, 0.300000, 0.000000)  // r8.x <- ambient.x; r8.y <- ambient.y; r8.z <- ambient.z

#line 119
  dp3 r1.z, r4.xyzx, r4.xyzx
  rsq r1.z, r1.z
  mul r4.xyz, r1.zzzz, r4.xyzx  // r4.x <- normal.x; r4.y <- normal.y; r4.z <- normal.z

#line 120
  mov r7.xyz, -r7.xyzx
  dp3 r1.z, r7.xyzx, r7.xyzx
  rsq r1.z, r1.z
  mul r7.xyz, r1.zzzz, r7.xyzx  // r7.x <- lightDir.x; r7.y <- lightDir.y; r7.z <- lightDir.z

#line 121
  dp3 r1.z, r4.xyzx, r7.xyzx
  max r1.z, r1.z, l(0.000000)  // r1.z <- diff

#line 122
  mul r6.xyz, r1.zzzz, r6.xyzx
  mul r6.xyz, r3.xyzx, r6.xyzx  // r6.x <- diffuse.x; r6.y <- diffuse.y; r6.z <- diffuse.z

#line 124
  mov r7.xyz, -r7.xyzx
  dp3 r1.z, r7.xyzx, r4.xyzx
  add r1.z, r1.z, r1.z
  mov r1.z, -r1.z
  mul r4.xyz, r1.zzzz, r4.xyzx
  add r4.xyz, r7.xyzx, r4.xyzx
  dp3 r1.z, r4.xyzx, r4.xyzx
  rsq r1.z, r1.z
  mul r4.xyz, r1.zzzz, r4.xyzx  // r4.x <- reflectDir.x; r4.y <- reflectDir.y; r4.z <- reflectDir.z

#line 125
  mov r5.xyz, -r5.xyzx
  add r5.xyz, r5.xyzx, CB0[1][8].yzwy
  dp3 r1.z, r5.xyzx, r5.xyzx
  rsq r1.z, r1.z
  mul r5.xyz, r1.zzzz, r5.xyzx  // r5.x <- viewDir.x; r5.y <- viewDir.y; r5.z <- viewDir.z

#line 126
  mov r1.z, l(32.000000)  // r1.z <- shininess

#line 127
  dp3 r1.w, r5.xyzx, r4.xyzx
  max r1.w, r1.w, l(0.000000)
  log r1.w, r1.w
  mul r1.z, r1.w, r1.z
  exp r1.z, r1.z  // r1.z <- spec

#line 128
  itof r4.xyz, l(1, 1, 1, 0)
  mul r4.xyz, r1.zzzz, r4.xyzx
  mul r3.xyz, r3.xyzx, r4.xyzx  // r3.x <- specular.x; r3.y <- specular.y; r3.z <- specular.z

#line 130
  add r4.xyz, r6.xyzx, r8.xyzx
  add r3.xyz, r3.xyzx, r4.xyzx  // r3.x <- <CalculateLight return value>.x; r3.y <- <CalculateLight return value>.y; r3.z <- <CalculateLight return value>.z
  mov r3.w, l(1.000000)  // r3.w <- <CalculateLight return value>.w

#line 145
  add r2.xyzw, r2.xyzw, r3.xyzw

#line 146
  iadd r1.y, r1.y, l(1)
endloop 

#line 147
mov o0.xyzw, r2.xyzw
ret 
// Approximately 65 instruction slots used