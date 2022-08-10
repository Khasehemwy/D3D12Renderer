# Invert Color  

[App_InvertColor](./App_InvertColor/App_InvertColor.cpp)  

**需求：** 两个相互遮盖的物体，后面的物体呈现反色 （类似透视）。  

**思路1：** 用模板测试实现。 分为3个Pass，第一个Pass1绘制前方的物体，并写入模板。第二个Pass2绘制后方的物体的反色，此时开启模板检测，只有Pass1标记的部分绘制。第三个Pass3绘制后方物体剩余部分，开启模板检测，只有未标记的部分绘制。  


**思路2：** 该思路为App实现思路，后处理实现。 分为3个Pass。第一个Pass1绘制物体1到Texture1（Format为float32的RTV），z值写入a通道。第二个Pass2绘制物体2到Texture2，z值也写入a通道。第三个Pass3使用Texture1和Texture2，若两个Texture均采样到非背景色，则根据a通道的z值判断谁在后方并反色。  

**思路2注意事项：**
1. Texture的a通道应该存posProj.w，因为透视投影后只有w存放z值。
2. Texture必须用float类型的Resource，不然存放的z值不正确。


**思路1(模板)和2(后处理)的优劣：**  
1. 模板方式需要先在CPU端判断物体前后，后处理方式不需要。  
2. 后处理方式z值存入32bit的a通道，需要float32的RTV，内存消耗更多。
3. 后处理不能处理物体和背景色相同的情况。  

可以结合模板测试、深度测试、后处理，达到最佳效果。  

<image src="https://user-images.githubusercontent.com/57032017/180359857-910b86aa-e5d4-4b00-bd2a-880a735ff574.gif" width="60%">   