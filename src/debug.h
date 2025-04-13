// This header contains some useful functions for debugging OpenGL.
// Remember to disable them when building your final releases.
#ifdef DEBUG_INTRO

#include <windows.h>
#include <GL/gl.h>
#include "glext.h"
#include <stdio.h>

#define STRINGIFY2(x) #x // Thanks sooda!
#define STRINGIFY(x) STRINGIFY2(x)
#define CHECK_ERRORS() assertGlError(STRINGIFY(__LINE__))
#define CHECK_RESULT(command,expected) checkResult(__LINE__, STRINGIFY(command), (int)(command), (int)(expected))

static GLchar* getErrorString(GLenum errorCode)
{
	if (errorCode == GL_NO_ERROR) {
		return (GLchar*) "No error";
	}
	else if (errorCode == GL_INVALID_VALUE) {
		return (GLchar*) "Invalid value";
	}
	else if (errorCode == GL_INVALID_ENUM) {
		return (GLchar*) "Invalid enum";
	}
	else if (errorCode == GL_INVALID_OPERATION) {
		return (GLchar*) "Invalid operation";
	}
	else if (errorCode == GL_STACK_OVERFLOW) {
		return (GLchar*) "Stack overflow";
	}
	else if (errorCode == GL_STACK_UNDERFLOW) {
		return (GLchar*) "Stack underflow";
	}
	else if (errorCode == GL_OUT_OF_MEMORY) {
		return (GLchar*) "Out of memory";
	}
	return (GLchar*) "Unknown";
}

static void assertGlError(const char* message)
{
	const GLenum ErrorValue = glGetError();
	if (ErrorValue == GL_NO_ERROR) return;

	MessageBox(NULL, message, getErrorString(ErrorValue), 0x00000000L);
	ExitProcess(0);
}


static void checkResult(int line, const char* code, int got, int expected)
{	
	if (got == expected)
		return;

	char message[1024];
	sprintf_s(message, 1024, "On line %d, \"%s\" returned %d, expected %d", line, code, got, expected);

	MessageBox(NULL, message, NULL, 0x00000000L);
	ExitProcess(0);
}

#else

#define CHECK_ERRORS()
#define CHECK_RESULT(command,expectedResult) command

#endif
