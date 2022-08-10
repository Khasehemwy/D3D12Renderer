# Blur

[App_Blur](./App_Blur/App_Blur.cpp)  

用ComputeShader实现的模糊效果。  

**思路:**   
1. 正常渲染场景，并将BackBuffer给Copy到Texture0（App中为RenderTexture类）。  
2. 将Texture0作为SRV传入CS，并在CS中进行模糊操作，写入到作为UAV的Texture1中。  
3. 将Texture1作为SRV，渲染到整个窗口。  

**注意事项：** 
1. 难点主要为CS的编写，dispatchThreadID可以理解为全局的ID，groupThreadID可以理解为局部的ID。
2. 注意在已经设置过PSO的基础上，设置新的PSO时，需要使用SetPipelineState()代替Reset()。

<image src="https://user-images.githubusercontent.com/57032017/179885809-55a19e3d-be6d-4b1b-89ba-d7a4a51b447f.png" width="60%">   