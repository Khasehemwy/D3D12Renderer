# App_ComputeCulling  
  
使用CS进行视锥体剔除，依靠ExecuteIndirect()。  
  
**思路：**   
  
1. 坐标进行透视投影后，透视除法前，其值满足：  
  
   > ${-w \leq x \leq w}$  
   > ${-w \leq y \leq w}$  
   > ${0 \leq z \leq w}$  

   若坐标值在该范围外，即说明在视锥体外，可以剔除。  
  
2. 剔除应当在物体层面，而不是顶点层面，所以传统绘制的管线难以处理，传统剔除方法为CPU端进行。  
   
3. 借助ExecuteIndirect() API，可以在CS进行相关计算后，输出需要的绘制命令，从而避免将不需要的物体绘制命令传入管线。  
   
  
**主要步骤：**  
  
1. 创建一个CommanBuffer，用于储存所有的绘制命令。  
  
   CommandBuffer中储存最终绘制时需要用到的GPU资源地址（`D3D12_GPU_VIRTUAL_ADDRESS`）、绘制命令的相关描述（`D3D12_DRAW_INDEXED_ARGUMENTS`）等信息。  
  
2. 创建一个ProcessedCommandBuffer，用于储存CS处理后，需要进行执行的那些绘制命令。该Buffer在CS中为`AppendStructuredBuffer`，作为UAV。注意该类UAV一般在Buffer最后添加一个UINT的多余空间，用于储存UAV BufferCounter。  

3. 创建CommandSignature。ExecuteIndirect使用CommandBuffer需要CommandSignature（类似RootSignature），如此才能正确使用Buffer中的数据，用于绘制。  
  
4. 剔除物体需要物体包围盒信息，新建一个SRV Buffer用于传入所有物体包围盒信息，寻址Index与Command Buffer相对应，即最终取出某一Index的物体后，判断是否剔除，再将该Index的Command写入UAV。  
  
  
**注意事项：**  
  
1. ConstantBuffer 大小需要为256 Byte的倍数。  
  
2. StructuredBuffer（SRV）虽然没有规范说明必须对齐，但一般仍需要对齐为float4的倍数。实际应用中未对齐导致了错误。  
  
3. `D3D12_DRAW_INDEXED_ARGUMENTS` 实际大小为4个UINT和1个INT，`D3D12_GPU_VIRTUAL_ADDRESS` 为2个UINT，注意Shader中对齐。  
  
4. UAV Counter必须和4K对齐。项目中AlignForUavCounter()进行了Buffer正确大小的计算。  
  
  
**效果：**  
  
  
<table><tr>
      <td><image src="https://user-images.githubusercontent.com/57032017/183896582-a19585b2-cf6b-419a-853d-1f910e14f117.gif" width=100% border=0>
  <p>CS进行剔除，帧率100</p></td>
      <td><image src="https://user-images.githubusercontent.com/57032017/183896798-a6f54658-d44b-44a8-892e-e876b7898b8f.gif" width=100% border=0>
  <p>CPU进行剔除，帧率37</p></td>
      <td><image src="https://user-images.githubusercontent.com/57032017/183896985-cad4a0ac-51fb-4048-aaf8-b52ddfdab8b3.gif" width=100% border=0>
  <p>无剔除，帧率23</p></td>
</tr></table> 
