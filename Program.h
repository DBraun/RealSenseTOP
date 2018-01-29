/* Shared Use License: This file is owned by Derivative Inc. (Derivative) and
 * can only be used, and/or modified for use, in conjunction with
 * Derivative's TouchDesigner software, and only if you are a licensee who has
 * accepted Derivative's TouchDesigner license or assignment agreement (which
 * also govern the use of this file).  You may share a modified version of this
 * file with another authorized licensee of Derivative's TouchDesigner software.
 * Otherwise, no redistribution or sharing of this file, with or without
 * modification, is permitted.
 */

#ifndef Program_h
#define Program_h

#include "TOP_CPlusPlusBase.h"

class Program 
{
public:
    Program();
    ~Program();
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;
	const char *build(const char *vertex, const char *fragment);
    GLuint getName() const;
private:
    static GLuint compileShader(const char *source, GLenum type, const char **error);
    GLuint myProgram;
};

#endif /* Program_h */
