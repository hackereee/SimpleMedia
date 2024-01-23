#include <Part1.h>
#include <common/TextureSample.h>
#include <iostream>
#include <map>


std::map<int, void(*)()> functionMap;

void init(){
    functionMap[1] = &beginPart1;
    functionMap[2] = &renderTexture;
    functionMap[3] = &renderTexture3D;
}



int main(){
    init();
    std::cout << "opengl 练习实现,请输入序号以开始：" << std::endl;
    std::cout << "1.绘制三角形" << std::endl;
    std::cout << "2.纹理练习" << std::endl;
    std::cout << "3. 3D纹理，加lookAt 矩阵" << std::endl;
    int input = 0;
    std::cin >> input;
    functionMap[input]();
    return 0;
}

