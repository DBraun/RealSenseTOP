/* Shared Use License: This file is owned by Derivative Inc. (Derivative) and
 * can only be used, and/or modified for use, in conjunction with 
 * Derivative's TouchDesigner software, and only if you are a licensee who has
 * accepted Derivative's TouchDesigner license or assignment agreement (which
 * also govern the use of this file).  You may share a modified version of this
 * file with another authorized licensee of Derivative's TouchDesigner software.
 * Otherwise, no redistribution or sharing of this file, with or without
 * modification, is permitted.
 */

#include "CPUMemoryTOP.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <cmath>

#include <iostream> // just for debugging
#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <sstream>


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
	info.executeMode = TOP_ExecuteMode::CPUMemWriteOnly;

	return info;
}

DLLEXPORT
TOP_CPlusPlusBase*
CreateTOPInstance(const OP_NodeInfo* info, TOP_Context* context)
{
	// Return a new instance of your class every time this is called.
	// It will be called once per TOP that is using the .dll
	return new CPUMemoryTOP(info);
}

DLLEXPORT
void
DestroyTOPInstance(TOP_CPlusPlusBase* instance, TOP_Context *context)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the TOP using that instance is deleted, or
	// if the TOP loads a different DLL
	delete (CPUMemoryTOP*)instance;
}

};


CPUMemoryTOP::CPUMemoryTOP(const OP_NodeInfo* info) : myNodeInfo(info)
{
	myExecuteCount = 0;
}

CPUMemoryTOP::~CPUMemoryTOP()
{
	pipe.stop();
}

void
CPUMemoryTOP::getGeneralInfo(TOP_GeneralInfo* ginfo)
{
	// Uncomment this line if you want the TOP to cook every frame even
	// if none of it's inputs/parameters are changing.
	ginfo->cookEveryFrame = true;
	if (image_mode == 0) {
		ginfo->memPixelType = OP_CPUMemPixelType::R32Float;
	}
	else {
		ginfo->memPixelType = OP_CPUMemPixelType::RGBA32Float;
	}
    
	ginfo->clearBuffers = false;
}

void CPUMemoryTOP::setupDevice(const char* sensorID) {
	rs2::context ctx;
	auto list = ctx.query_devices(); // Get a snapshot of currently connected devices
	if (list.size() == 0)
		throw std::runtime_error("No device detected. Is it plugged in?");

	for (rs2::device temp : list) {

		auto new_serial = temp.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
		
		std::stringstream ss;
		ss << "Sensor";
		ss << new_serial;

		if (strcmp(ss.str().c_str(), sensorID) == 0) {
			mySensorID = sensorID;

			// stop the current stream/pipe if it's running
			try {
				// todo: Don't just try to stop it.
				// Only stop it if it was started.
				pipe.stop();
			}
			catch (const std::exception&e) {
				std::cout << "RS2 - Error: " << e.what() << std::endl;
			}

			// todo: refuse to setup the device if it's already in use
			// by a different cplusplus TOP

			//dev.hardware_reset();
			//rs2::device_hub hub(ctx);
			//dev = hub.wait_for_device();

			int width = 848;
			int height = 480;
			int FPS = 60;
			rs2::config config;
			config.enable_device(new_serial);
			config.enable_stream(RS2_STREAM_DEPTH, width, height, RS2_FORMAT_Z16, FPS);

			rs2::pipeline_profile profile = pipe.start(config);
			if (!profile) {
				throw(-1);
			}

			rs2::device dev = profile.get_device();

			for (rs2::sensor& sensor : dev.query_sensors())
			{
				// Check if the sensor is a depth sensor
				if (rs2::depth_sensor dpt = sensor.as<rs2::depth_sensor>())
				{
					depth_scale = dpt.get_depth_scale();
					break;
				}
			}

			break;
		}
	}
}

bool
CPUMemoryTOP::getOutputFormat(TOP_OutputFormat* format)
{
	// In this function we could assign variable values to 'format' to specify
	// the pixel format/resolution etc that we want to output to.
	// If we did that, we'd want to return true to tell the TOP to use the settings we've
	// specified.

	format->width = 848;
	format->height = 480;
	format->bitsPerChannel = 16;
	format->numColorBuffers = 1;
	format->floatPrecision = true;

	bool needOtherChannels = image_mode > 0; // true if point cloud mode

	format->redChannel = true;
	format->blueChannel = needOtherChannels;
	format->greenChannel = needOtherChannels;
	format->alphaChannel = needOtherChannels;

	return true;
}

void
CPUMemoryTOP::execute(const TOP_OutputFormatSpecs* outputFormat,
						OP_Inputs* inputs,
						TOP_Context *context)
{
	myExecuteCount++;

	try
	{

		const char* currentSensor = inputs->getParString("Sensor");

		if (strcmp(mySensorID.c_str(), currentSensor) != 0) {
			setupDevice(currentSensor);
		}

		rs2::frameset frames;
		if (!pipe.poll_for_frames(&frames)) {
			return;
		}
		rs2::frame depth_frame = frames.first(RS2_STREAM_DEPTH);
		auto pixels = (const uint16_t*) depth_frame.get_data();

		int width = outputFormat->width;
		int height = outputFormat->height;

		int textureMemoryLocation = 0;

		float* mem = (float*)outputFormat->cpuPixelData[textureMemoryLocation];

		if (image_mode == 0) {
			// depth
			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					float* pixel = &mem[(y*width + x)]; // or &mem[4*(y*width + x)] if RGBA

					const uint16_t myDepth = pixels[(height - 1 - y)*width + x];

					pixel[0] = depth_scale * myDepth;
				}
			}
		} else {
			// point cloud
			points = pc.calculate(depth_frame);

			auto vertices = points.get_vertices();

			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					float* pixel = &mem[4 * (y*width + x)]; // or &mem[4*(y*width + x)] if RGBA

					const uint16_t myDepth = pixels[(height - 1 - y)*width + x];

					auto vertex = vertices[(height - 1 - y)*width + x];

					pixel[0] = vertex.x;
					pixel[1] = vertex.y;
					pixel[2] = vertex.z;
					pixel[3] = 1.;
				}
			}
		}

		image_mode = inputs->getParInt("Image");

		outputFormat->newCPUPixelDataLocation = textureMemoryLocation;
		textureMemoryLocation = !textureMemoryLocation;
	}
	catch (const std::exception&e)
	{
		std::cout << "RS - Error: " << e.what() << std::endl;
	}

}

int32_t
CPUMemoryTOP::getNumInfoCHOPChans()
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the TOP. In this example we are just going to send one channel.
	return 1;
}

void
CPUMemoryTOP::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name = "executeCount";
		chan->value = (float)myExecuteCount;
	}
}

bool		
CPUMemoryTOP::getInfoDATSize(OP_InfoDATSize* infoSize)
{
	infoSize->rows = 2;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
CPUMemoryTOP::getInfoDATEntries(int32_t index,
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
}

void
CPUMemoryTOP::setupParameters(OP_ParameterManager* manager)
{

	// Mode
	{
		OP_StringParameter	sp;

		sp.name = "Image";
		sp.label = "Image";

		sp.defaultValue = "Depth";

		const char *names[] = { "Depth", "Pointcloud"};
		const char *labels[] = { "Depth", "Point Cloud"};

		OP_ParAppendResult res = manager->appendMenu(sp, 2, names, labels);
		assert(res == OP_ParAppendResult::Success);
	}

	// Sensor
	{
		OP_StringParameter	sp;

		sp.name = "Sensor";
		sp.label = "Sensor";

		sp.defaultValue = "";

		rs2::context ctx;
		auto list = ctx.query_devices(); // Get a snapshot of currently connected devices
		if (list.size() == 0)
			throw std::runtime_error("No device detected. Is it plugged in?");

		size_t numDevices = list.size();
		std::vector<const char*> names;
		std::vector<const char*> labels;

		std::vector<std::string> names_strs;
		std::vector<std::string> labels_strs;

		for (auto dev : list) {
			std::stringstream ss;
			ss << "Sensor";
			ss << dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
			names_strs.push_back(ss.str());
			labels_strs.push_back(ss.str());
		}

		// Convert to a vector of c-style strings
		for (const auto& string : names_strs) names.push_back(string.c_str());
		for (const auto& string : labels_strs) labels.push_back(string.c_str());

		sp.defaultValue = names[0];

		OP_ParAppendResult res = manager->appendMenu(sp, numDevices, names.data(), labels.data());
		assert(res == OP_ParAppendResult::Success);
	}

	// pulse
	/*
	{
		OP_NumericParameter	np;

		np.name = "Reset";
		np.label = "Reset";
		
		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}
	*/

}

void
CPUMemoryTOP::pulsePressed(const char* name)
{
	if (!strcmp(name, "Reset"))
	{

	}
}

