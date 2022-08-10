# D3D12Renderer

## Introduction

Renderer based on [d3d12book](https://github.com/d3dcoder/d3d12book), using DirectX 12.  
The project implements the various rendering effects.
  
  
## Dependencies  
  
[D3D12Book - Common](https://github.com/d3dcoder/d3d12book/tree/master/Common)。离线`Common`文件夹已放置于项目中。  
  
[PIX](https://devblogs.microsoft.com/pix/download/)。若在`Debug`下运行，需安装`PIX`或注释相应代码。（PIX为截帧工具，方便Debug）  
  
[Visual Studio 2022](visualstudio.microsoft.com)。VS版本可能会影响模型加载库assimp。

[assimp](https://github.com/assimp/assimp)。模型加载库。
  
  
## APP  
  
**详细介绍在各App文件夹内的README文档。**  

  ***
  
### Draw Box  

[App_DrawBox](./App_DrawBox)  
  
Simply draw the box. It shows the project structure.  
  
  ***
  
### Shapes In 3 Frame  
  
[App_shapesIn3Frame](./App_shapesIn3Frame)  
  
将数据提前处理3帧。
  
  ***
  
### Tessellation  
  
[App_Tessellation](./App_Tessellation)  
  
曲面细分实现。  仅为简单入门程度。  
  
<image src="https://user-images.githubusercontent.com/57032017/179745877-04095b94-6209-4651-a1a9-c214e8049f87.gif" width="50%">  
  
  ***
  
### Instancing  
  
[App_Instancing](./App_Instancing)  
  
现代实现方式的实例化。  
  
<image src="https://user-images.githubusercontent.com/57032017/179747814-5b177533-28eb-4b41-9288-6199c5a8e196.gif" width="50%">  
  
  ***
  
### Culling  
  
[App_Culling](./App_Culling)  
  
视锥体剔除。  
  
<image src="https://user-images.githubusercontent.com/57032017/179749927-7ccf4aa0-89e0-464c-8468-b14510705956.gif" width="50%">  
  
  ***
  
### Blur  
  
[App_Blur](./App_Blur)  
  
用ComputeShader实现的模糊效果。  
  
<image src="https://user-images.githubusercontent.com/57032017/179885809-55a19e3d-be6d-4b1b-89ba-d7a4a51b447f.png" width="50%">   
  
  ***
  
### Invert Color  
  
[App_InvertColor](./App_InvertColor)  
  
两个相互遮盖的物体，后面的物体呈现反色 （类似透视）。  
  
<image src="https://user-images.githubusercontent.com/57032017/180359857-910b86aa-e5d4-4b00-bd2a-880a735ff574.gif" width="50%">   
  
  ***
  
### Shadow  
  
[App_Shadow](./App_Shadow)  
  
使用ShadowMap实现的阴影。  
  
<image src="https://user-images.githubusercontent.com/57032017/179924409-85e6d768-7281-40c3-9fc4-c6f206f3d4c3.gif" width="50%">  
  
  ***
  
### SSAO  
  
[App_SSAO](./App_SSAO)  
  
经典SSAO实现。  
  
<image src="https://user-images.githubusercontent.com/57032017/182311718-3f6a3d65-7b1b-41b5-bfd4-f1c1e4cb669c.png" width=50%>  
  
  ***
  
### ComputeCulling  
  
[App_ComputeCulling](./App_ComputeCulling)  
  
使用CS进行视锥体剔除，依靠ExecuteIndirect()。 
  
<table><tr>
      <td><image src="https://user-images.githubusercontent.com/57032017/183896582-a19585b2-cf6b-419a-853d-1f910e14f117.gif" width=100% border=0>
  <p>CS进行剔除，帧率100</p></td>
      <td><image src="https://user-images.githubusercontent.com/57032017/183896798-a6f54658-d44b-44a8-892e-e876b7898b8f.gif" width=100% border=0>
  <p>CPU进行剔除，帧率37</p></td>
      <td><image src="https://user-images.githubusercontent.com/57032017/183896985-cad4a0ac-51fb-4048-aaf8-b52ddfdab8b3.gif" width=100% border=0>
  <p>无剔除，帧率23</p></td>
</tr></table> 
