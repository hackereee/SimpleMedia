#include <Program/shader.h>

using namespace std;

Shader::Shader(char* vertexSourcePath, char* fragmentSourcePath){
    string vertexCode;
    string fragmentCode;
    ifstream vShaderFile;
    ifstream fShaderFile;
    vShaderFile.exceptions(ifstream::failbit | ifstream::badbit);
    fShaderFile.exceptions(ifstream::failbit | ifstream::badbit);

    try {

    }
    catch (ifstream::failure e) {
        cout << "init shader source failed: " << endl;
    }
}

void Shader::use(){

}

