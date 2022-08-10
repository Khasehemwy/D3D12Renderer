# Instancing  

[App_Instancing](./App_Instancing/App_Instancing.cpp)  

现代实现方式的实例化。  

与OpenGL在绘制命令时传入Transform信息不同，现代的实现方式为将所有Transform信息传入Shader的结构化缓冲区，再根据InstanceID来使用对应Transform Data。  

<image src="https://user-images.githubusercontent.com/57032017/179747814-5b177533-28eb-4b41-9288-6199c5a8e196.gif" width="60%">  