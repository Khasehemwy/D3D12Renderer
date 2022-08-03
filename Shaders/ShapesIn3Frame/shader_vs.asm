//
// Generated by Microsoft (R) HLSL Shader Compiler 10.1
//
//
// Buffer Definitions: 
//
// cbuffer cbPerObject
// {
//
//   float4x4 gWorld;                   // Offset:    0 Size:    64
//
// }
//
// cbuffer cbPerPass
// {
//
//   float4x4 gView;                    // Offset:    0 Size:    64
//   float4x4 gProj;                    // Offset:   64 Size:    64
//
// }
//
//
// Resource Bindings:
//
// Name                                 Type  Format         Dim      HLSL Bind  Count
// ------------------------------ ---------- ------- ----------- -------------- ------
// cbPerObject                       cbuffer      NA          NA            cb0      1 
// cbPerPass                         cbuffer      NA          NA            cb1      1 
//
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// COLOR                    0   xyzw        1     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// COLOR                    0   xyzw        1     NONE   float   xyzw
//
vs_5_0
dcl_globalFlags refactoringAllowed | skipOptimization
dcl_constantbuffer CB0[4], immediateIndexed
dcl_constantbuffer CB1[8], immediateIndexed
dcl_input v0.xyz
dcl_input v1.xyzw
dcl_output_siv o0.xyzw, position
dcl_output o1.xyzw
dcl_temps 2
//
// Initial variable locations:
//   v0.x <- vin.posLocal.x; v0.y <- vin.posLocal.y; v0.z <- vin.posLocal.z; 
//   v1.x <- vin.color.x; v1.y <- vin.color.y; v1.z <- vin.color.z; v1.w <- vin.color.w; 
//   o1.x <- <VS return value>.color.x; o1.y <- <VS return value>.color.y; o1.z <- <VS return value>.color.z; o1.w <- <VS return value>.color.w; 
//   o0.x <- <VS return value>.posProj.x; o0.y <- <VS return value>.posProj.y; o0.z <- <VS return value>.posProj.z; o0.w <- <VS return value>.posProj.w
//
#line 24 "C:\Users\shuangyiguo\source\repos\Project1\Project1\Shaders\ShapesIn3Frame\shader.hlsl"
mov r0.xyz, v0.xyzx
mov r0.w, l(1.000000)
dp4 r1.x, r0.xyzw, cb0[0].xyzw  // r1.x <- vout.posProj.x
dp4 r1.y, r0.xyzw, cb0[1].xyzw  // r1.y <- vout.posProj.y
dp4 r1.z, r0.xyzw, cb0[2].xyzw  // r1.z <- vout.posProj.z
dp4 r1.w, r0.xyzw, cb0[3].xyzw  // r1.w <- vout.posProj.w

#line 25
dp4 r0.x, r1.xyzw, cb1[0].xyzw  // r0.x <- vout.posProj.x
dp4 r0.y, r1.xyzw, cb1[1].xyzw  // r0.y <- vout.posProj.y
dp4 r0.z, r1.xyzw, cb1[2].xyzw  // r0.z <- vout.posProj.z
dp4 r0.w, r1.xyzw, cb1[3].xyzw  // r0.w <- vout.posProj.w

#line 26
dp4 r1.x, r0.xyzw, cb1[4].xyzw  // r1.x <- vout.posProj.x
dp4 r1.y, r0.xyzw, cb1[5].xyzw  // r1.y <- vout.posProj.y
dp4 r1.z, r0.xyzw, cb1[6].xyzw  // r1.z <- vout.posProj.z
dp4 r1.w, r0.xyzw, cb1[7].xyzw  // r1.w <- vout.posProj.w

#line 28
mov r0.xyzw, v1.xyzw  // r0.x <- vout.color.x; r0.y <- vout.color.y; r0.z <- vout.color.z; r0.w <- vout.color.w

#line 30
mov o0.xyzw, r1.xyzw
mov o1.xyzw, r0.xyzw
ret 
// Approximately 18 instruction slots used