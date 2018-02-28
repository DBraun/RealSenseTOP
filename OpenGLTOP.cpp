/* Shared Use License: This file is owned by Derivative Inc. (Derivative) and
 * can only be used, and/or modified for use, in conjunction with 
 * Derivative's TouchDesigner software, and only if you are a licensee who has
 * accepted Derivative's TouchDesigner license or assignment agreement (which
 * also govern the use of this file).  You may share a modified version of this
 * file with another authorized licensee of Derivative's TouchDesigner software.
 * Otherwise, no redistribution or sharing of this file, with or without
 * modification, is permitted.
 */

#include "OpenGLTOP.h"

#include <assert.h>
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#include <string.h>
#endif
#include <cstdio>

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API

static const char *vertexShader = "#version 330\n\
uniform mat4 uModelView; \
in vec3 P; \
void main() { \
    gl_Position = vec4(P, 1) * uModelView; \
}";

static const char *fragmentShader = "#version 330\n\
uniform vec4 uColor; \
out vec4 finalColor; \
void main() { \
    finalColor = uColor; \
}";

static const char *uniformError = "A uniform location could not be found.";

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{
DLLEXPORT
TOP_PluginInfo
GetTOPPluginInfo(void)
{
	TOP_PluginInfo info;
	// This must always be set to this constant
	info.apiVersion = TOPCPlusPlusAPIVersion;

	// Change this to change the executeMode behavior of this plugin.
	info.executeMode = TOP_ExecuteMode::OpenGL_FBO;

	return info;
}

DLLEXPORT
TOP_CPlusPlusBase*
CreateTOPInstance(const OP_NodeInfo* info, TOP_Context *context)
{
	// Return a new instance of your class every time this is called.
	// It will be called once per TOP that is using the .dll

    // Note we can't do any OpenGL work during instantiation

	return new OpenGLTOP(info, context);
}

DLLEXPORT
void
DestroyTOPInstance(TOP_CPlusPlusBase* instance, TOP_Context *context)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the TOP using that instance is deleted, or
	// if the TOP loads a different DLL

    // We do some OpenGL teardown on destruction, so ask the TOP_Context
    // to set up our OpenGL context
    context->beginGLCommands();

	delete (OpenGLTOP*)instance;

    context->endGLCommands();
}

};


OpenGLTOP::OpenGLTOP(const OP_NodeInfo* info, TOP_Context *context)
: myNodeInfo(info), myExecuteCount(0), myRotation(0.0), myError(nullptr),
    myProgram(), myDidSetup(false), myModelViewUniform(-1), myColorUniform(-1)
{

#ifdef WIN32
	// GLEW is global static function pointers, only needs to be inited once,
	// and only on Windows.
	static bool needGLEWInit = true;
	if (needGLEWInit)
	{
		needGLEWInit = false;
		context->beginGLCommands();
		// Setup all our GL extensions using GLEW
		glewInit();
		context->endGLCommands();
	}
#endif


	// If you wanted to do other GL initialization inside this constructor, you could
	// uncomment these lines and do the work between the begin/end
	//
	//context->beginGLCommands();
	// Custom GL initialization here
	//context->endGLCommands();
}

OpenGLTOP::~OpenGLTOP()
{

}

void
OpenGLTOP::getGeneralInfo(TOP_GeneralInfo* ginfo)
{
	// Setting cookEveryFrame to true causes the TOP to cook every frame even
	// if none of its inputs/parameters are changing. Set it to false if it
    // only needs to cook when inputs/parameters change.
	ginfo->cookEveryFrame = true;
}

bool
OpenGLTOP::getOutputFormat(TOP_OutputFormat* format)
{
	// In this function we could assign variable values to 'format' to specify
	// the pixel format/resolution etc that we want to output to.
	// If we did that, we'd want to return true to tell the TOP to use the settings we've
	// specified.
	// In this example we'll return false and use the TOP's settings
	return false;
}
struct float2 { float x, y; };
struct rect
{
	float x, y;
	float w, h;

	// Create new rect within original boundaries with give aspect ration
	rect adjust_ratio(float2 size) const
	{
		auto H = static_cast<float>(h), W = static_cast<float>(h) * size.x / size.y;
		if (W > w)
		{
			auto scale = w / W;
			W *= scale;
			H *= scale;
		}

		return{ x + (w - W) / 2, y + (h - H) / 2, W, H };
	}
};

// COPIED and slightly modified from
// https://github.com/IntelRealSense/librealsense/blob/master/examples/example.hpp
void upload(const rs2::video_frame& frame, GLuint gl_handle)
{
	if (!frame) return;

	if (!gl_handle)
		glGenTextures(1, &gl_handle);
	GLenum err = glGetError();

	auto format = frame.get_profile().format();
	int width = frame.get_width();
	int height = frame.get_height();
	//stream = frame.get_profile().stream_type();

	glBindTexture(GL_TEXTURE_2D, gl_handle);

	switch (format)
	{
	case RS2_FORMAT_RGB8:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, frame.get_data());
		break;
	case RS2_FORMAT_RGBA8:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame.get_data());
		break;
	case RS2_FORMAT_Y8:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, frame.get_data());
		break;
	case RS2_FORMAT_Z16: // add in case for grayscale depth texture
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, frame.get_data());
		break;
	default:
		throw std::runtime_error("The requested format is not supported by this demo!");
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

// COPIED and slightly modified from
// https://github.com/IntelRealSense/librealsense/blob/master/examples/example.hpp
void show(const rect& r, GLuint gl_handle)
{
	if (!gl_handle)
		return;

	glBindTexture(GL_TEXTURE_2D, gl_handle);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUAD_STRIP);
	glTexCoord2f(0.f, 1.f); glVertex2f(r.x, r.y + r.h);
	glTexCoord2f(0.f, 0.f); glVertex2f(r.x, r.y);
	glTexCoord2f(1.f, 1.f); glVertex2f(r.x + r.w, r.y + r.h);
	glTexCoord2f(1.f, 0.f); glVertex2f(r.x + r.w, r.y);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	//draw_text((int)r.x + 15, (int)r.y + 20, rs2_stream_to_string(stream));
}

void
OpenGLTOP::execute(const TOP_OutputFormatSpecs* outputFormat ,
							OP_Inputs* inputs,
							TOP_Context* context)
{
	myExecuteCount++;

	if (!hasStarted) {
		hasStarted = true;
		rs2::pipeline_profile profile = pipe.start();
		if (!profile) {
			throw(-1);
		}
	}

	// These functions must be called before
	// beginGLCommands()/endGLCommands() block
	double speed = inputs->getParDouble("Speed");

	double color1[3];
	double color2[3];

	inputs->getParDouble3("Color1", color1[0], color1[1], color1[2]);
	inputs->getParDouble3("Color2", color2[0], color2[1], color2[2]);

    myRotation += speed;

    int width = outputFormat->width;
    int height = outputFormat->height;

    float ratio = static_cast<float>(height) / static_cast<float>(width);

    Matrix view;
    view[0] = ratio;

    context->beginGLCommands();
    
    setupGL();

	if (!myError)
	{
		glViewport(0, 0, width, height);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(myProgram.getName());

		// Draw the square

		glUniform4f(myColorUniform, static_cast<GLfloat>(color1[0]), static_cast<GLfloat>(color1[1]), static_cast<GLfloat>(color1[2]), 1.0f);

		mySquare.setTranslate(0.5f, 0.5f);
		mySquare.setRotation(static_cast<GLfloat>(myRotation));

		Matrix model = mySquare.getMatrix();
		glUniformMatrix4fv(myModelViewUniform, 1, GL_FALSE, (model * view).matrix);

		mySquare.bindVAO();

		glDrawArrays(GL_TRIANGLES, 0, mySquare.getElementCount() / 3);

		// Draw the chevron

		glUniform4f(myColorUniform, static_cast<GLfloat>(color2[0]), static_cast<GLfloat>(color2[1]), static_cast<GLfloat>(color2[2]), 1.0f);

		myChevron.setScale(0.8f, 0.8f);
		myChevron.setTranslate(-0.5, -0.5);
		myChevron.setRotation(static_cast<GLfloat>(myRotation));

		model = myChevron.getMatrix();
		glUniformMatrix4fv(myModelViewUniform, 1, GL_FALSE, (model * view).matrix);

		myChevron.bindVAO();

		glDrawArrays(GL_TRIANGLES, 0, myChevron.getElementCount() / 3);

		// Tidy up

		glBindVertexArray(0);

		// start realsense render
		if (hasStarted && myExecuteCount > 100) {
			/*
			rs2::frameset data = pipe.wait_for_frames();
			rs2::depth_frame depth = data.get_depth_frame();
			int width = depth.get_width();
			int height = depth.get_height();

			upload(depth, gl_handle);
			rect r;
			show(r.adjust_ratio({ float(width), float(height) }), gl_handle);
			*/
			rs2::frameset frames;
			if (pipe.poll_for_frames(&frames)) {
				// todo: this section is never reached?
				rs2::depth_frame depth = frames.first(RS2_STREAM_DEPTH);

				int width = depth.get_width();
				int height = depth.get_height();

				upload(depth, gl_handle);
				rect r;
				show(r.adjust_ratio({ float(width), float(height) }), gl_handle);
			}
		}
		// end realsense render

		glUseProgram(0);
		
	}

	context->endGLCommands();
}

int32_t
OpenGLTOP::getNumInfoCHOPChans()
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the TOP. In this example we are just going to send one channel.
	return 2;
}

void
OpenGLTOP::getInfoCHOPChan(int32_t index,
										OP_InfoCHOPChan* chan)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name = "executeCount";
		chan->value = (float)myExecuteCount;
	}

	if (index == 1)
	{
		chan->name = "rotation";
		chan->value = (float)myRotation;
	}
}

bool		
OpenGLTOP::getInfoDATSize(OP_InfoDATSize* infoSize)
{
	infoSize->rows = 2;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
OpenGLTOP::getInfoDATEntries(int32_t index,
										int32_t nEntries,
										OP_InfoDATEntries* entries)
{
	// It's safe to use static buffers here because Touch will make it's own
	// copies of the strings immediately after this call returns
	// (so the buffers can be reuse for each column/row)
	static char tempBuffer1[4096];
	static char tempBuffer2[4096];

	if (index == 0)
	{
		// Set the value for the first column
#ifdef WIN32
		strcpy_s(tempBuffer1, "executeCount");
#else // macOS
        strlcpy(tempBuffer1, "executeCount", sizeof(tempBuffer1));
#endif
		entries->values[0] = tempBuffer1;

		// Set the value for the second column
#ifdef WIN32
		sprintf_s(tempBuffer2, "%d", myExecuteCount);
#else // macOS
        snprintf(tempBuffer2, sizeof(tempBuffer2), "%d", myExecuteCount);
#endif
		entries->values[1] = tempBuffer2;
	}

	if (index == 1)
	{
		// Set the value for the first column
#ifdef WIN32
		strcpy_s(tempBuffer1, "rotation");
#else // macOS
		strlcpy(tempBuffer1, "rotation", sizeof(tempBuffer1));
#endif
		entries->values[0] = tempBuffer1;

		// Set the value for the second column
#ifdef WIN32
		sprintf_s(tempBuffer2, "%g", myRotation);
#else // macOS
		snprintf(tempBuffer2, sizeof(tempBuffer2), "%g", myRotation);
#endif
		entries->values[1] = tempBuffer2;
	}
}

const char *
OpenGLTOP::getErrorString()
{
    return myError;
}

void
OpenGLTOP::setupParameters(OP_ParameterManager* manager)
{
	// color 1
	{
		OP_NumericParameter	np;

		np.name = "Color1";
		np.label = "Color 1";

        np.defaultValues[0] = 1.0;
        np.defaultValues[1] = 0.5;
        np.defaultValues[2] = 0.8;

		for (int i=0; i<3; i++)
		{
			np.minValues[i] = 0.0;
			np.maxValues[i] = 1.0;
			np.minSliders[i] = 0.0;
			np.maxSliders[i] = 1.0;
			np.clampMins[i] = true;
			np.clampMaxes[i] = true;
		}
		
		OP_ParAppendResult res = manager->appendRGB(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// color 2
	{
		OP_NumericParameter	np;

		np.name = "Color2";
		np.label = "Color 2";

        np.defaultValues[0] = 1.0;
        np.defaultValues[1] = 1.0;
        np.defaultValues[2] = 0.25;

		for (int i=0; i<3; i++)
		{
			np.minValues[i] = 0.0;
			np.maxValues[i] = 1.0;
			np.minSliders[i] = 0.0;
			np.maxSliders[i] = 1.0;
			np.clampMins[i] = true;
			np.clampMaxes[i] = true;
		}
		
		OP_ParAppendResult res = manager->appendRGB(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// speed
	{
		OP_NumericParameter	np;

		np.name = "Speed";
		np.label = "Speed";
		np.defaultValues[0] = 1.0;
		np.minSliders[0] = -10.0;
		np.maxSliders[0] =  10.0;
		
		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// pulse
	{
		OP_NumericParameter	np;

		np.name = "Reset";
		np.label = "Reset";
		
		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}

}

void
OpenGLTOP::pulsePressed(const char* name)
{
	if (!strcmp(name, "Reset"))
	{
		myRotation = 0.0;
	}
}

void OpenGLTOP::setupGL()
{
    if (myDidSetup == false)
    {
        myError = myProgram.build(vertexShader, fragmentShader);

        // If an error occurred creating myProgram, we can't proceed
        if (myError == nullptr)
        {
            GLint vertAttribLocation = glGetAttribLocation(myProgram.getName(), "P");
            myModelViewUniform = glGetUniformLocation(myProgram.getName(), "uModelView");
            myColorUniform = glGetUniformLocation(myProgram.getName(), "uColor");

            if (vertAttribLocation == -1 || myModelViewUniform == -1 || myColorUniform == -1)
            {
                myError = uniformError;
            }

            // Set up our two shapes
            GLfloat square[] = {
                -0.5, -0.5, 1.0,
                0.5, -0.5, 1.0,
                -0.5,  0.5, 1.0,

                0.5, -0.5, 1.0,
                0.5,  0.5, 1.0,
                -0.5,  0.5, 1.0
            };

            mySquare.setVertices(square, 2 * 9);
            mySquare.setup(vertAttribLocation);

            GLfloat chevron[] = {
                -1.0, -1.0,  1.0,
                -0.5,  0.0,  1.0,
                0.0, -1.0,  1.0,

                -0.5,  0.0,  1.0,
                0.5,  0.0,  1.0,
                0.0, -1.0,  1.0,

                0.0,  1.0,  1.0,
                0.5,  0.0,  1.0,
                -0.5,  0.0,  1.0,

                -1.0,  1.0,  1.0,
                0.0,  1.0,  1.0,
                -0.5,  0.0,  1.0
            };

            myChevron.setVertices(chevron, 4 * 9);
            myChevron.setup(vertAttribLocation);
        }

        myDidSetup = true;
    }
}
