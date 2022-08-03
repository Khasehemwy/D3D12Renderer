float4x4 inverse(float4x4 m)
{
    float4x4 ans;
    float4 u = float4(m[0][0], m[0][1], m[0][2], m[0][3]);
    float4 v = float4(m[1][0], m[1][1], m[1][2], m[1][3]);
    float4 w = float4(m[2][0], m[2][1], m[2][2], m[2][3]);
    float4 t = float4(m[3][0], m[3][1], m[3][2], m[3][3]);
    ans[0][0] = u.x;
    ans[1][0] = u.y;
    ans[2][0] = u.z;
    ans[3][0] = -dot(u, t);

    ans[0][1] = v.x;
    ans[1][1] = v.y;
    ans[2][1] = v.z;
    ans[3][1] = -dot(v, t);

    ans[0][2] = w.x;
    ans[1][2] = w.y;
    ans[2][2] = w.z;
    ans[3][2] = -dot(w, t);

    ans[0][3] = u.w;
    ans[1][3] = v.w;
    ans[2][3] = w.w;
    ans[3][3] = 1;

    return ans;
}