# Shapes In 3 Frame  

[App_shapesIn3Frame](./App_shapesIn3Frame/App_shapesIn3Frame.cpp)  

因CPU多数时候较GPU更空闲，将部分CPU处理的数据划分到3帧。这样即可在GPU渲染时，提前将数据处理完成，GPU不再需要等待CPU。  

**注意事项：** DescriptorHeap的构建；资源创建时根据DescriptorHeap需要进行相应偏移。  