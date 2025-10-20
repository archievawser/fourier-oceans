#include "FoamComputeShader.h"


IMPLEMENT_GLOBAL_SHADER(FFoamComputeShader, "/CustomShaders/FoamComputeShader.usf", "MainComputeShader", SF_Compute);