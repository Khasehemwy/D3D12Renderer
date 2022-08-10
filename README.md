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
### Draw Box  

[App_DrawBox](./App_DrawBox)  
  
Simply draw the box. It shows the project structure.  
  
  
  
### Shapes In 3 Frame  
  
[App_shapesIn3Frame](./App_shapesIn3Frame)  
  
将数据提前处理3帧。
  
  
  
### Tessellation  
  
[App_Tessellation](./App_Tessellation)  
  
曲面细分实现。  仅为简单入门程度。  
  
  
  
### Instancing  
  
[App_Instancing](./App_Instancing)  
  
现代实现方式的实例化。  
  
  
  
### Culling  
  
[App_Culling](./App_Culling)  
  
视锥体剔除。  
  
  
  
### Blur  
  
[App_Blur](./App_Blur)  
  
用ComputeShader实现的模糊效果。  
  
  
  
### Invert Color  
  
[App_InvertColor](./App_InvertColor)  
  
两个相互遮盖的物体，后面的物体呈现反色 （类似透视）。  
  
  
  
## Shadow  
  
[App_Shadow](./App_Shadow)  
  
使用ShadowMap实现的阴影。  
  
  
  
## SSAO  
  
[App_SSAO](./App_SSAO)  
  
经典SSAO实现。  
