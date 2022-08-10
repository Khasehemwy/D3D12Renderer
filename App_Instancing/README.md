# Instancing  

[App_Instancing](./App_Instancing.cpp)  

现代实现方式的实例化。  

与OpenGL在绘制命令时传入Transform信息不同，现代的实现方式为将所有Transform信息传入Shader的结构化缓冲区，再根据InstanceID来使用对应Transform Data。  

<image src="https://user-images.githubusercontent.com/57032017/183898419-e87c19a5-67df-4366-9412-0b2df6098f1d.gif" width="60%">  
