#include "InversionComputeShader.h"


IMPLEMENT_GLOBAL_SHADER(FInversionComputeShader, "/CustomShaders/InversionComputeShader.usf", "MainComputeShader", SF_Compute);