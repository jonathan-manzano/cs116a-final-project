#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glad/gl.h>

class Shader {
public:
    unsigned int ID;

    // Constructor for shaders with optional tessellation
    Shader(const char* vertexPath, const char* fragmentPath, 
           const char* tessControlPath = nullptr, const char* tessEvalPath = nullptr)
    {
        // Read shader files with error checking
        std::string vCode = readShaderFile(vertexPath);
        std::string fCode = readShaderFile(fragmentPath);
        
        if (vCode.empty() || fCode.empty())
        {
            std::cerr << "ERROR: Failed to read shader files\n";
            ID = 0;
            return;
        }

        const char* vShaderCode = vCode.c_str();
        const char* fShaderCode = fCode.c_str();

        // Create and compile vertex and fragment shaders
        unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, nullptr);
        glCompileShader(vertex);
        if (!checkCompileErrors(vertex, "VERTEX"))
        {
            ID = 0;
            return;
        }

        unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, nullptr);
        glCompileShader(fragment);
        if (!checkCompileErrors(fragment, "FRAGMENT"))
        {
            glDeleteShader(vertex);
            ID = 0;
            return;
        }

        // Create program
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);

        // Compile and attach tessellation shaders if provided
        unsigned int tessControl = 0, tessEval = 0;
        if (tessControlPath != nullptr && tessEvalPath != nullptr)
        {
            std::string tcCode = readShaderFile(tessControlPath);
            std::string teCode = readShaderFile(tessEvalPath);
            
            if (tcCode.empty() || teCode.empty())
            {
                std::cerr << "ERROR: Failed to read tessellation shader files\n";
                glDeleteShader(vertex);
                glDeleteShader(fragment);
                ID = 0;
                return;
            }
            
            const char* tcShaderCode = tcCode.c_str();
            const char* teShaderCode = teCode.c_str();

            tessControl = glCreateShader(GL_TESS_CONTROL_SHADER);
            glShaderSource(tessControl, 1, &tcShaderCode, nullptr);
            glCompileShader(tessControl);
            if (!checkCompileErrors(tessControl, "TESS_CONTROL"))
            {
                glDeleteShader(vertex);
                glDeleteShader(fragment);
                ID = 0;
                return;
            }

            tessEval = glCreateShader(GL_TESS_EVALUATION_SHADER);
            glShaderSource(tessEval, 1, &teShaderCode, nullptr);
            glCompileShader(tessEval);
            if (!checkCompileErrors(tessEval, "TESS_EVALUATION"))
            {
                glDeleteShader(vertex);
                glDeleteShader(fragment);
                glDeleteShader(tessControl);
                ID = 0;
                return;
            }

            glAttachShader(ID, tessControl);
            glAttachShader(ID, tessEval);
        }

        glLinkProgram(ID);
        if (!checkCompileErrors(ID, "PROGRAM"))
        {
            ID = 0;
            return;
        }

        // Cleanup
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        if (tessControl)  glDeleteShader(tessControl);
        if (tessEval)     glDeleteShader(tessEval);
    }

    void use() const {
        if (ID != 0)
            glUseProgram(ID);
    }

    void setMat4(const std::string &name, const float* value) const {
        if (ID != 0)
            glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, value);
    }

    void setFloat(const std::string &name, float value) const {
        if (ID != 0)
            glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setVec3(const std::string &name, const float* value) const {
        if (ID != 0)
            glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, value);
    }

    bool isValid() const {
        return ID != 0;
    }

private:
    std::string readShaderFile(const char* path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            std::cerr << "ERROR: Failed to open shader file: " << path << "\n";
            return "";
        }
        
        std::stringstream stream;
        stream << file.rdbuf();
        return stream.str();
    }

    bool checkCompileErrors(unsigned int shader, const std::string& type)
    {
        int success;
        char infoLog[1024];

        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "SHADER COMPILATION ERROR (" << type << "):\n"
                          << infoLog << "\n";
                return false;
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "PROGRAM LINK ERROR:\n" << infoLog << "\n";
                return false;
            }
        }
        return true;
    }
};