#pragma once

// Forward declarations
namespace CodeGenerator
{
class BackendBase;
class CodeStream;
class ModelSpecMerged;
}

//--------------------------------------------------------------------------
// CodeGenerator
//--------------------------------------------------------------------------
namespace CodeGenerator
{
void generateNeuronUpdate(CodeStream &os, const ModelSpecMerged &modelMerged, const BackendBase &backend, bool standaloneModules);
}
