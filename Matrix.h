/* Shared Use License: This file is owned by Derivative Inc. (Derivative) and
 * can only be used, and/or modified for use, in conjunction with
 * Derivative's TouchDesigner software, and only if you are a licensee who has
 * accepted Derivative's TouchDesigner license or assignment agreement (which
 * also govern the use of this file).  You may share a modified version of this
 * file with another authorized licensee of Derivative's TouchDesigner software.
 * Otherwise, no redistribution or sharing of this file, with or without
 * modification, is permitted.
 */

#ifndef Matrix_h
#define Matrix_h

#include "TOP_CPlusPlusBase.h"

class Matrix {
public:
    Matrix();
    GLfloat matrix[16];
    GLfloat operator[](int i) const
    {
        return matrix[i];
    };
    GLfloat& operator[](int i)
    {
        return matrix[i];
    };
};

Matrix operator*(const Matrix &a, const Matrix &b);

#endif /* Matrix_h */
