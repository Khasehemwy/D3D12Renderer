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
//   float4x4 gViewProj;                // Offset:   64 Size:    64
//
// }
//
//
// Resource Bindings:
//
// Name                                 Type  Format         Dim      HLSL Bind  Count
// ------------------------------ ---------- ------- ----------- -------------- ------
// cbPerObject                       cbuffer      NA          NA            cb0      1 
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
dcl_constantbuffer CB0[8], immediateIndexed
dcl_input v0.xyz
dcl_input v1.xyzw
dcl_output_siv o0.xyzw, position
dcl_output o1.xyzw
dcl_temps 2
//
// Initial variable locations:
//   v0.x <- vin.pos.x; v0.y <- vin.pos.y; v0.z <- vin.pos.z; 
//   v1.x <- vin.color.x; v1.y <- vin.color.y; v1.z <- vin.color.z; v1.w <- vin.color.w; 
//   o1.x <- <VS return value>.color.x; o1.y <- <VS return value>.color.y; o1.z <- <VS return value>.color.z; o1.w <- <VS return value>.color.w; 
//   o0.x <- <VS return value>.pos.x; o0.y <- <VS return value>.pos.y; o0.z <- <VS return value>.pos.z; o0.w <- <VS return value>.pos.w
//
#line 23 "C:\Users\shuangyiguo\source\repos\Project1\Project1\Shaders\DrawBox\shader.hlsl"
mov r0.xyz, v0.xyzx
mov r0.w, l(1.000000)
dp4 r1.x, r0.xyzw, cb0[0].xyzw
dp4 r1.y, r0.xyzw, cb0[1].xyzw
dp4 r1.z, r0.xyzw, cb0[2].xyzw
dp4 r1.w, r0.xyzw, cb0[3].xyzw
dp4 r0.x, r1.xyzw, cb0[4].xyzw  // r0.x <- vout.pos.x
dp4 r0.y, r1.xyzw, cb0[5].xyzw  // r0.y <- vout.pos.y
dp4 r0.z, r1.xyzw, cb0[6].xyzw  // r0.z <- vout.pos.z
dp4 r0.w, r1.xyzw, cb0[7].xyzw  // r0.w <- vout.pos.w

#line 24
mov r1.xyzw, v1.xyzw  // r1.x <- vout.color.x; r1.y <- vout.color.y; r1.z <- vout.color.z; r1.w <- vout.color.w

#line 26
mov o0.xyzw, r0.xyzw
mov o1.xyzw, r1.xyzw
ret 
// Approximately 14 instruction slots used
