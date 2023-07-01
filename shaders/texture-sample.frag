#version 330 core
//纹理
in vec2 outTexcood;
//纹理
uniform sampler2D plane;

//片段
out vec4 FragColor;

void main(){
    //调用方法进行纹理插值，下一步光栅化
    FragColor = texture(plane, outTexcood);
}