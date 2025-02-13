#ifndef SHADER_H
#define SHADER_H
#include <glad/glad.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <glm/glm.hpp>

/// <summary>
/// 着色器类
/// </summary>
class Shader {
public:
	unsigned int ProgramId;
	/// <summary>
	/// </summary>
	/// <param name="vertexSource">顶点着色器源代码</param>
	/// <param name="fragmentSource">片段着色器源代码</param>
	Shader(const char* vertexSource, const char* fragmentSource);
	//使用
	void use();

	void setInt(std::string& name, int value);
	void setBool(std::string& name, bool value);
	void setFloat(std::string& name, float value);
	void setMat4(std::string& name, glm::mat4& value);

	void setIntP(const char* name, int value);
	void setBoolP(const char* name, bool value);
	void setFloatP(const char* name, float value);
	void setMat4P(const char* name, glm::mat4& value);

	void release();

private:


	void compileAndLink(const char* vertexCode, const char* fragmentCode);


};

#endif