#ifndef BGFX_SHADER_H
#define BGFX_SHADER_H
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <bx/math.h>
#include <bgfx/bgfx.h>


/// <summary>
/// 着色器类
/// </summary>
class Shader {
public:
	bgfx::ProgramHandle programHandle_;
	/// <summary>
	/// </summary>
	/// <param name="vertexSource">顶点着色器源代码</param>
	/// <param name="fragmentSource">片段着色器源代码</param>
	Shader(const char* vertexSource, const char* fragmentSource);
	//使用
	void submit(bgfx::ViewId viewId);


	void setInt(const char* name, int value);
	void setFloat(const char* name, float value);
	void setMat3(const char* name, float* mat3);
	void setMat4(const char* name, float* mat4);

	~Shader();


private:

	std::unordered_map<const char *, bgfx::UniformHandle> uniformLocationCache;


	void compileAndLink(const char* vertexCode, const char* fragmentCode);

	bgfx::UniformHandle getUniformLocation(const char* name, bgfx::UniformType::Enum type);


};

#endif