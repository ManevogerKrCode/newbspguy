#include "lang.h"
#include "Shader.h"
#include "log.h"

#include <GL/glew.h>

Shader::Shader(const char* sourceCode, int shaderType)
{
	// Create Shader And Program Objects
	ID = glCreateShader(shaderType);

	glShaderSource(ID, 1, &sourceCode, NULL);
	glCompileShader(ID);

	int success;
	glGetShaderiv(ID, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE)
	{
		char* log = new char[512];
		int len;
		glGetShaderInfoLog(ID, 512, &len, log);
		print_log(get_localized_string(LANG_0959),shaderType);
		print_log(log);
		if (len > 512)
			print_log(get_localized_string(LANG_0960));
		delete[] log;
	}
}


Shader::~Shader(void)
{
	glDeleteShader(ID);
}

