# Culling  

[App_Culling](./App_Culling.cpp)  

视锥体剔除。  

不在视锥体内的物体可以提前剔除，避免绘制开销.  

这里是物体级别的剔除，当物体三角形数量较多时，优化才会明显。若物体三角形数量较少（例如样例中的盒子），反而会降低效率，因为包围盒的碰撞检测开销较大。  

**思路：** 预计算物体的包围盒，渲染每帧时，将视锥体变换到该物体的模型空间，再检测物体是否在视锥体内，若不在则剔除。  

**注意事项：** 在DirectXMath中，需要进行XMMatrixTranspose()以后，再Copy进Buffer给Shader使用才正确。  

<image src="https://user-images.githubusercontent.com/57032017/179749927-7ccf4aa0-89e0-464c-8468-b14510705956.gif" width="60%">  