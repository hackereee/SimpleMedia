#include <bgfx/bgfx.h>
#include <bgfx/shader.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
using namespace std;
Shader::Shader(const char* vertexSource, const char* fragmentSource) {
    string vertexCode;
    string fragmentCode;
    ifstream vShaderFile;
    ifstream fShaderFile;
    vShaderFile.exceptions(ifstream::failbit | ifstream::badbit);
    fShaderFile.exceptions(ifstream::failbit | ifstream::badbit);

    try {
        if(!vShaderFile.good() || !fShaderFile.good()){
            cout << "the vertexSourcePath or fragmentSourcePath is not exists" << endl;
            return;
        }
        vShaderFile.open(vertexSource);
        fShaderFile.open(fragmentSource);
        stringstream vShaderStream, fShaderStream;
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        vShaderFile.close();
        fShaderFile.close();
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (ifstream::failure e) {
        std::cout << "init shader source failed: " << endl;
    }

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();
    compileAndLink(vShaderCode, fShaderCode);
    delete vShaderCode;
    delete fShaderCode;

};

void Shader::compileAndLink(const char* vertexCode, const char* fragmentCode){
    if(!vertexCode || !fragmentCode){
        cout << "vertexCode or fragmentCode is null" << endl;
        return;
    }
    const bgfx::Memory* vsMem = bgfx::copy(vertexCode, strlen(vertexCode) + 1);
    const bgfx::Memory* fsMem = bgfx::copy(fragmentCode, strlen(fragmentCode) + 1);
    bgfx::ShaderHandle vsh = bgfx::createShader(vsMem);
    bgfx::ShaderHandle fsh = bgfx::createShader(fsMem);
    programHandle_ = bgfx::createProgram(vsh, fsh, true);
    
}

Shader::~Shader(){
    bgfx::destroy(programHandle_);
    //遍历删除uniform
    for(auto it = uniformLocationCache.begin(); it != uniformLocationCache.end(); it++){
        bgfx::destroy(it->second);
    }
    std::clog << "Shader destroyed" << std::endl;
}

void Shader::submit(bgfx::ViewId viewId){
   bgfx::submit(viewId, programHandle_);
}

void Shader::setInt(const char* name, int value){
    bgfx::setUniform(getUniformLocation(name, bgfx::UniformType::Count), &value);
}


void Shader::setFloat(const char* name, float value){
    bgfx::setUniform(getUniformLocation(name, bgfx::UniformType::Count), &value);
}


void Shader::setMat3(const char* name, float* mat3){
    bgfx::setUniform(getUniformLocation(name, bgfx::UniformType::Mat3), mat3);
}

void Shader::setMat4(const char* name, float* mat4){
    bgfx::setUniform(getUniformLocation(name, bgfx::UniformType::Mat4), mat4);
}


bgfx::UniformHandle Shader::getUniformLocation(const char* name, bgfx::UniformType::Enum type){
    bgfx::UniformHandle handle;
    if(uniformLocationCache.count(name)){
        handle =   uniformLocationCache[name];
   }else{
       handle = bgfx::createUniform(name, type);
       uniformLocationCache[name] = handle;
   }
   return handle;
}

