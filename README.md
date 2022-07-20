# D3D12Renderer

## Introduction

Renderer based on [d3d12book](https://github.com/d3dcoder/d3d12book), using DirectX 12.  
The project implements the various rendering effects.

## APP
### Draw Box  

[App_DrawBox](./Project1/App_DrawBox.cpp)  
  
Simply draw the box. It shows the project structure.  
  
  
  
### Shapes In 3 Frame  
  
[App_shapesIn3Frame](./Project1/App_shapesIn3Frame.cpp)  
  
因CPU多数时候较GPU更空闲，将部分CPU处理的数据划分到3帧。这样即可在GPU渲染时，提前将数据处理完成，GPU不再需要等待CPU。  
  
**注意事项：** DescriptorHeap的构建；资源创建时根据DescriptorHeap需要进行相应偏移。  
  
  
  
### Tessellation  
  
[App_Tessellation](./Project1/App_Tessellation.cpp)  
  
曲面细分实现。  仅为简单入门程度。  
  
**注意事项：** 构建PSO时，PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH ，与传统三角形不同。  
  
<image src="https://user-images.githubusercontent.com/57032017/179745877-04095b94-6209-4651-a1a9-c214e8049f87.gif" width="70%">  
  
  
  
### Instancing  
  
[App_Instancing](./Project1/App_Instancing.cpp)  
  
现代实现方式的实例化。  
  
与OpenGL在绘制命令时传入Transform信息不同，现代的实现方式为将所有Transform信息传入Shader的结构化缓冲区，再根据InstanceID来使用对应Transform Data。  
  
<image src="https://user-images.githubusercontent.com/57032017/179747814-5b177533-28eb-4b41-9288-6199c5a8e196.gif" width="70%">  
  
  
  
### Culling  
  
[App_Culling](./Project1/App_Culling.cpp)  
  
视锥体剔除。  
  
不在视锥体内的物体可以提前剔除，避免绘制开销.  
  
这里是物体级别的剔除，当物体三角形数量较多时，优化才会明显。若物体三角形数量较少（例如样例中的盒子），反而会降低效率，因为包围盒的碰撞检测开销较大。  
  
**思路：** 预计算物体的包围盒，渲染每帧时，将视锥体变换到该物体的模型空间，再检测物体是否在视锥体内，若不在则剔除。  
  
**注意事项：** 在DirectXMath中，需要进行XMMatrixTranspose()以后，再Copy进Buffer给Shader使用才正确。  
  
<image src="https://user-images.githubusercontent.com/57032017/179749927-7ccf4aa0-89e0-464c-8468-b14510705956.gif" width="70%">  
  
  
  
### Blur  
  
[App_Blur](./Project1/App_Blur.cpp)  
  
用ComputeShader实现的模糊效果。  
  
**思路:**   
1. 正常渲染场景，并将BackBuffer给Copy到Texture0（App中为RenderTexture类）。  
2. 将Texture0作为SRV传入CS，并在CS中进行模糊操作，写入到作为UAV的Texture1中。  
3. 将Texture1作为SRV，渲染到整个窗口。  
  
**注意事项：** 
1. 难点主要为CS的编写，dispatchThreadID可以理解为全局的ID，groupThreadID可以理解为局部的ID。
2. 注意在已经设置过PSO的基础上，设置新的PSO时，需要使用SetPipelineState()代替Reset()。
  
<image src="https://user-images.githubusercontent.com/57032017/179885809-55a19e3d-be6d-4b1b-89ba-d7a4a51b447f.png" width="60%">   
  
  
  
### Invert Color  
  
[App_InvertColor](./Project1/App_InvertColor.cpp)  
  
**需求：** 两个相互遮盖的物体，后面的物体呈现反色 （类似透视）。  
  
**思路1：** 用模板测试实现。 分为3个Pass，第一个Pass1绘制前方的物体，并写入模板。第二个Pass2绘制后方的物体的反色，此时开启模板检测，只有Pass1标记的部分绘制。第三个Pass3绘制后方物体剩余部分，开启模板检测，只有未标记的部分绘制。  
  
**思路2：** 该思路为App实现思路，后处理实现。 分为3个Pass。第一个Pass1绘制物体1并Copy到Texture1，z值写入a通道。第二个Pass2绘制物体2并Copy到Texture2，z值也写入a通道。第三个Pass3使用Texture1和Texture2，若两个Texture均采样到非背景色，则根据a通道的z值判断谁在后方并反色。  
  
  
**思路1(模板)和2(后处理)的优劣：**  
1. 模板方式需要先在CPU端判断物体前后，后处理方式不需要。  
2. 后处理方式因为z值存入8bit的a通道，精度不够，会出现深度判断失误的情况。
3. 后处理不能处理物体和背景色相同的情况。  
  
可以结合模板测试、深度测试、后处理，达到最佳效果。  
  
<image src="https://user-images.githubusercontent.com/57032017/179895040-588d1beb-2c17-4eb0-8007-1671bd530f93.gif" width="70%">   
  
  
  
## Shadow  
  
[App_Shadow](./Project1/App_Shadow.cpp)  
  
使用ShadowMap实现的阴影。  
  
**思路：** 分为2个Pass。第一个Pass1从光源视角渲染场景，并存入Texture中作为ShadowMap。第二个Pass2中，将顶点坐标变换到光源空间，再使用ShadowMap采样确定可见性。思路不算复杂，但实现较繁琐。这里添加第三个Pass3将ShadowMap渲染至屏幕右下角。    
  
**注意事项：**  
1. 从光源渲染场景可以先渲染到BackBuffer再Copy到Resource，也可以直接将Resource作为DSV，渲染时绑定该DSV。
2. DSV创建时，Flag和Format必须对应，否则会出错。  
  
<image src="https://user-images.githubusercontent.com/57032017/179924409-85e6d768-7281-40c3-9fc4-c6f206f3d4c3.gif" width="70%">  
