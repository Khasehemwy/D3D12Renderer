# SSAO  

[App_SSAO](./App_SSAO.cpp)  

经典SSAO实现。  

**思路：**    

SSAO原理不再赘述，这里主要记录与DX12实现相关的事项。  

1. 第一个Pass：生成G-Buffer  
  
   + 生成SSAO Map这里使用观察空间的法线信息和NDC坐标下的深度信息（与观察方向相关即可，最终PS输出z值即在NDC坐标，所以直接记录NDC下z值）。最终混合SSAO效果使用SSAO Map和原场景Texture。所以G-Buffer一共有3个，分别为Normal、Z-Value、Color。z值可以绑定DSV直接得到，Normal和Color使用MRT在一个Pass中得到。  
  
2. 第二个Pass：生成SSAO Map  
  
   + 创建一个RenderTexture作为RandomVectorMap，每个像素代表生成SSAO时，随机采样的向量，数据在CPU端预生成。每帧生成14个Offset数组，作为随机向量的偏移，以保证数据均匀分布（8个立方体角+6个立方体面中心）。  
   
   + 不传入IB、VB，直接使用`mCommandList->DrawInstanced(6, 1, 0, 0);`来绘制屏幕四边形，在Shader中根据SV_VertexID来获取对应顶点。  
   
   + 主要难点为Shader中SSAO遮蔽值计算时各个坐标系的转换，需要理解View、NDC空间之间的关系。坐标经过Perspective Projection以后，再经过透视除法，即变换到NDC空间，因为该Pass只有屏幕信息，w值（z值）为1，所以经过投影矩阵推导即可以得到，对深度`zView = gProj[3][2] / (zNdc - gProj[2][2])`（HLSL列主序）。然后在平截头体中根据相似三角形和z值，可以得到采样像素点对应在观察空间中的点`p = (zView / pIn.posVNear.z) * pin.posVNear`。现在发现需要posVNear，即采样像素点在平截头体中对应在近平面的点。这里将NDC坐标中的点z值设为0，经过逆投影矩阵后，即可得到点在近平面的位置`posVNear = mul(vout.posH, gInvProj).xyz`。
  
3. 第三个Pass：对SSAO Map进行模糊  
  
   + 这里用CS进行模糊，直接处理贴图CS更直观。AO值应该采用双边模糊，保留边缘信息，AO就应该突变而不是渐变。双边模糊需要借助Normal和z值信息，在之前的G-Buffer已经储存了。具体CS编写类似[App_Blur](./Project1/App_Blur.cpp)。注意UAV的创建标识符和SRV有区别。  
  
4. 第四个Pass：使用Color和SSAO Map，构建最终场景  


5. 视口下方4个Debug视图  
  
   + 这里封装了DebugViewer类，自动对相应Resource在新的Heap创建SRV，并在新的Pass绘制。  


**效果：**  

<table><tr>
  <td><image src="https://user-images.githubusercontent.com/57032017/182311718-3f6a3d65-7b1b-41b5-bfd4-f1c1e4cb669c.png" width=60% height=60% border=0>
  <p>（下方从左到右依次为：Normal、SSAO Map、SSAO Map模糊、Color）</p></td>
</tr></table> 
