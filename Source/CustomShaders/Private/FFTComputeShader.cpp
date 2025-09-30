#include "FFTComputeShader.h"


IMPLEMENT_GLOBAL_SHADER(FFFTComputeShader, "/CustomShaders/FFTComputeShader.usf", "MainComputeShader", SF_Compute);