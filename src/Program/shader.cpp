#include <Program/shader.h>
#include <glm/gtc/type_ptr.hpp>


using namespace std;

Shader::Shader(const char* vertexSourcePath, const char* fragmentSourcePath) {
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
		vShaderFile.open(vertexSourcePath);
		fShaderFile.open(fragmentSourcePath);
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

}

void Shader::compileAndLink(const char* vertexCode, const char* fragmentCode) {
	unsigned int vertex, fragment;
	int success;
	char infoLog[512];
	//顶点着色器
	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vertexCode, NULL);
	glCompileShader(vertex);
	glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertex, 512, NULL, infoLog);
		cout << "create vertex shader failed:" << infoLog << endl;
	}

	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fragmentCode, NULL);
	glCompileShader(fragment);
	glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragment, 512, NULL, infoLog);
		cout << "create fragment shader failed:" << infoLog << endl;
	}
	ProgramId = glCreateProgram();
	glAttachShader(ProgramId, vertex);
	glAttachShader(ProgramId, fragment);
	glLinkProgram(ProgramId);
	glGetProgramiv(ProgramId, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(ProgramId, 512, NULL, infoLog);
		cout << "link shader error" << infoLog << endl;
	}
	//着色器已经链接至程序中，删除它
	glDeleteShader(vertex);
	glDeleteShader(fragment);

}

void Shader::use() {
	glUseProgram(ProgramId);
}


void Shader::setFloat(string& name, float value) {
	glUniform1f(glGetUniformLocation(ProgramId, name.c_str()), value);
}

void Shader::setInt(string& name, int value) {
	glUniform1i(glGetUniformLocation(ProgramId, name.c_str()), value);
}


void Shader::setBool(string& name, bool value) {
	glUniform1i(glGetUniformLocation(ProgramId, name.c_str()), value);
}

void Shader::setMat4(string& name, glm::mat4& value) {
	glUniformMatrix4fv(glGetUniformLocation(ProgramId, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}


void Shader::setFloatP(const char* name, float value) {
	glUniform1f(glGetUniformLocation(ProgramId, name), value);
}

void Shader::setIntP(const char* name, int value) {
	glUniform1i(glGetUniformLocation(ProgramId, name), value);
}


void Shader::setBoolP(const char* name, bool value) {
	glUniform1i(glGetUniformLocation(ProgramId, name), value);
}

void Shader::setMat4P(const char* name, glm::mat4& value) {
	glUniformMatrix4fv(glGetUniformLocation(ProgramId, name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::release() {
	glDeleteProgram(ProgramId);
}

