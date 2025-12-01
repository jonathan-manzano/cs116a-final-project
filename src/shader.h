#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glad/gl.h>

class Shader {
public:
    unsigned int ID;

    // Constructor for shaders with tessellation
    Shader(const char* vertexPath, const char* fragmentPath, 
           const char* tessControlPath = nullptr, const char* tessEvalPath = nullptr)
    {
        //reading glsl files into strings
        std::string vCode, fCode;
        std::ifstream vFile(vertexPath), fFile(fragmentPath);

        std::stringstream vStream, fStream;
        vStream << vFile.rdbuf();
        fStream << fFile.rdbuf();

        vCode = vStream.str();
        fCode = fStream.str();

        const char* vShaderCode = vCode.c_str();
        const char* fShaderCode = fCode.c_str();

        // create and compile vertex and fragment shaders
        unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, nullptr);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, nullptr);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        // create program
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);

        // compile and attach tessellation shaders if provided
        unsigned int tessControl = 0, tessEval = 0;
        if (tessControlPath != nullptr && tessEvalPath != nullptr)
        {
            std::string tcCode, teCode;
            std::ifstream tcFile(tessControlPath), teFile(tessEvalPath);
            
            std::stringstream tcStream, teStream;
            tcStream << tcFile.rdbuf();
            teStream << teFile.rdbuf();
            
            tcCode = tcStream.str();
            teCode = teStream.str();
            
            const char* tcShaderCode = tcCode.c_str();
            const char* teShaderCode = teCode.c_str();

            tessControl = glCreateShader(GL_TESS_CONTROL_SHADER);
            glShaderSource(tessControl, 1, &tcShaderCode, nullptr);
            glCompileShader(tessControl);
            checkCompileErrors(tessControl, "TESS_CONTROL");

            tessEval = glCreateShader(GL_TESS_EVALUATION_SHADER);
            glShaderSource(tessEval, 1, &teShaderCode, nullptr);
            glCompileShader(tessEval);
            checkCompileErrors(tessEval, "TESS_EVALUATION");

            glAttachShader(ID, tessControl);
            glAttachShader(ID, tessEval);
        }

        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        // cleanup
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        if (tessControl)  glDeleteShader(tessControl);
        if (tessEval)     glDeleteShader(tessEval);
    }

    void use() const {
        glUseProgram(ID);
    }

    void setMat4(const std::string &name, const float* value) const {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, value);
    }

    void setFloat(const std::string &name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setVec3(const std::string &name, const float* value) const {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, value);
    }

private:
    static void checkCompileErrors(unsigned int shader, const std::string& type)
    {
        int success;
        char infoLog[1024];

        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "SHADER COMPILATION ERROR (" << type << "):\n"
                          << infoLog << "\n";
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "PROGRAM LINK ERROR:\n" << infoLog << "\n";
            }
        }
    }
};