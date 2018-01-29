/* Shared Use License: This file is owned by Derivative Inc. (Derivative) and
 * can only be used, and/or modified for use, in conjunction with
 * Derivative's TouchDesigner software, and only if you are a licensee who has
 * accepted Derivative's TouchDesigner license or assignment agreement (which
 * also govern the use of this file).  You may share a modified version of this
 * file with another authorized licensee of Derivative's TouchDesigner software.
 * Otherwise, no redistribution or sharing of this file, with or without
 * modification, is permitted.
 */

#ifndef Shape_h
#define Shape_h

#include "TOP_CPlusPlusBase.h"
#include "Matrix.h"

class Shape
{
    /*
     A very naive 2D shape
     */
public:
    Shape();
    ~Shape();
    Shape(const Shape&) = delete;
    Shape& operator=(const Shape&) = delete;
    void setVertices(GLfloat *vertices, int elements);
    void setRotation(GLfloat degrees);
    void setTranslate(GLfloat x, GLfloat y);
    void setScale(GLfloat x, GLfloat y);
    void setup(GLuint attrib);
    void bindVAO() const;
    Matrix getMatrix() const;
    GLint getElementCount() const;
private:
    GLuint myVAO;
    GLuint myVBO;
    GLuint myElementCount;
    GLfloat myTranslateX;
    GLfloat myTranslateY;
    GLfloat myScaleX;
    GLfloat myScaleY;
    GLfloat myRotation;
};

#endif /* Shape_h */
