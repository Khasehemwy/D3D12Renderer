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
//   float4x4 gViewProj;                // Offset:   64 Size:    64 [unused]
//   float3 gEyePosW;                   // Offset:  128 Size:    12
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
// Patch Constant signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TessFactor            0   x           0 QUADEDGE   float   x   
// SV_TessFactor            1   x           1 QUADEDGE   float   x   
// SV_TessFactor            2   x           2 QUADEDGE   float   x   
// SV_TessFactor            3   x           3 QUADEDGE   float   x   
// SV_InsideTessFactor      0   x           4  QUADINT   float   x   
// SV_InsideTessFactor      1   x           5  QUADINT   float   x   
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
//
// Tessellation Domain   # of control points
// -------------------- --------------------
// Quadrilateral                           4
//
// Tessellation Output Primitive  Partitioning Type 
// ------------------------------ ------------------
// Clockwise Triangles            Integer           
//
hs_5_0
hs_decls 
dcl_input_control_point_count 4
dcl_output_control_point_count 4
dcl_tessellator_domain domain_quad
dcl_tessellator_partitioning partitioning_integer
dcl_tessellator_output_primitive output_triangle_cw
dcl_hs_max_tessfactor l(64.000000)
dcl_globalFlags refactoringAllowed | skipOptimization
dcl_constantbuffer CB0[9], immediateIndexed
hs_control_point_phase 
dcl_input vOutputControlPointID
dcl_input v[4][0].xyz
dcl_output o0.xyz
dcl_temps 1
//
// Initial variable locations:
//   vOutputControlPointID.x <- i; 
//   vPrim.x <- patchId; 
//   o0.x <- <HS return value>.PosL.x; o0.y <- <HS return value>.PosL.y; o0.z <- <HS return value>.PosL.z; 
//   v[0][0].x <- p[0].PosL.x; v[0][0].y <- p[0].PosL.y; v[0][0].z <- p[0].PosL.z; 
//   v[1][0].x <- p[1].PosL.x; v[1][0].y <- p[1].PosL.y; v[1][0].z <- p[1].PosL.z; 
//   v[2][0].x <- p[2].PosL.x; v[2][0].y <- p[2].PosL.y; v[2][0].z <- p[2].PosL.z; 
//   v[3][0].x <- p[3].PosL.x; v[3][0].y <- p[3].PosL.y; v[3][0].z <- p[3].PosL.z
//
#line 88 "E:\Projects\Project1\Project1\Shaders\Tessellation\shader.hlsl"
mov r0.x, vOutputControlPointID
mov r0.xyz, v[r0.x + 0][0].xyzx  // r0.x <- hout.PosL.x; r0.y <- hout.PosL.y; r0.z <- hout.PosL.z

#line 90
mov o0.xyz, r0.xyzx
ret 
hs_join_phase 
dcl_input vicp[4][0].xyz
dcl_output_siv o0.x, finalQuadUeq0EdgeTessFactor
dcl_output_siv o1.x, finalQuadVeq0EdgeTessFactor
dcl_output_siv o2.x, finalQuadUeq1EdgeTessFactor
dcl_output_siv o3.x, finalQuadVeq1EdgeTessFactor
dcl_output_siv o4.x, finalQuadUInsideTessFactor
dcl_output_siv o5.x, finalQuadVInsideTessFactor
dcl_temps 2

#line 45
add r0.xyz, vicp[1][0].xyzx, vicp[0][0].xyzx
add r0.xyz, r0.xyzx, vicp[2][0].xyzx
add r0.xyz, r0.xyzx, vicp[3][0].xyzx
mul r0.xyz, r0.xyzx, l(0.250000, 0.250000, 0.250000, 0.000000)  // r0.x <- centerL.x; r0.y <- centerL.y; r0.z <- centerL.z

#line 46
mov r0.w, l(1.000000)
dp4 r1.x, r0.xyzw, cb0[0].xyzw  // r1.x <- centerW.x
dp4 r1.y, r0.xyzw, cb0[1].xyzw  // r1.y <- centerW.y
dp4 r1.z, r0.xyzw, cb0[2].xyzw  // r1.z <- centerW.z

#line 48
mov r0.xyz, -cb0[8].xyzx
add r0.xyz, r0.xyzx, r1.xyzx
dp3 r0.x, r0.xyzx, r0.xyzx
sqrt r0.x, r0.x  // r0.x <- d

#line 55
mov r0.y, l(100.000000)  // r0.y <- d1

#line 56
mov r0.x, -r0.x
add r0.x, r0.x, r0.y
div r0.x, r0.x, l(80.000000)
max r0.x, r0.x, l(0.000000)
min r0.x, r0.x, l(1.000000)
mul r0.w, r0.x, l(64.000000)  // r0.w <- tess

#line 60
mov r1.x, r0.w  // r1.x <- pt.EdgeTess[0]

#line 61
mov r1.y, r0.w  // r1.y <- pt.EdgeTess[1]

#line 62
mov r0.x, r0.w  // r0.x <- pt.EdgeTess[2]

#line 63
mov r0.y, r0.w  // r0.y <- pt.EdgeTess[3]

#line 65
mov r0.z, r0.w  // r0.z <- pt.InsideTess[0]

#line 66
mov r0.w, r0.w  // r0.w <- pt.InsideTess[1]

#line 68
mov r0.x, r0.x  // r0.x <- <ConstantHS return value>.EdgeTess[2]
mov r0.y, r0.y  // r0.y <- <ConstantHS return value>.EdgeTess[3]
mov r0.z, r0.z  // r0.z <- <ConstantHS return value>.InsideTess[0]
mov r0.w, r0.w  // r0.w <- <ConstantHS return value>.InsideTess[1]
mov r1.x, r1.x  // r1.x <- <ConstantHS return value>.EdgeTess[0]
mov r1.y, r1.y  // r1.y <- <ConstantHS return value>.EdgeTess[1]
min o0.x, r1.x, l(64.000000)
min o1.x, r1.y, l(64.000000)
min r0.xyzw, r0.xyzw, l(64.000000, 64.000000, 64.000000, 64.000000)
mov o2.x, r0.x
mov o3.x, r0.y
mov o4.x, r0.z
mov o5.x, r0.w
ret 
// Approximately 43 instruction slots used
