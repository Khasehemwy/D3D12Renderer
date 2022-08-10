# Shadow  

[App_Shadow](./App_Shadow/App_Shadow.cpp)  

使用ShadowMap实现的阴影。  

**思路：** 分为2个Pass。第一个Pass1从光源视角渲染场景，并存入Texture中作为ShadowMap。第二个Pass2中，将顶点坐标变换到光源空间，再使用ShadowMap采样确定可见性。思路不算复杂，但实现较繁琐。这里添加第三个Pass3将ShadowMap渲染至屏幕右下角。    

**注意事项：**  
1. 从光源渲染场景可以先渲染到BackBuffer再Copy到Resource，也可以直接将Resource作为DSV，渲染时绑定该DSV。
2. DSV创建时，Flag和Format必须对应，否则会出错。  

<image src="https://user-images.githubusercontent.com/57032017/179924409-85e6d768-7281-40c3-9fc4-c6f206f3d4c3.gif" width="60%">  