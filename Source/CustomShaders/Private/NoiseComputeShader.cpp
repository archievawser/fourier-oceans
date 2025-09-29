#include "NoiseComputeShader.h"


IMPLEMENT_GLOBAL_SHADER(FNoiseComputeShader, "/CustomShaders/NoiseComputeShader.usf", "MainComputeShader", SF_Compute);