#pragma once

// Standard C++ includes
#include <array>
#include <functional>
#include <map>
#include <string>

// CUDA includes
#include <cuda.h>
#include <cuda_runtime.h>

// GeNN includes
#include "backendExport.h"

// GeNN code generator includes
#include "code_generator/backendBase.h"
#include "code_generator/codeStream.h"
#include "code_generator/substitutions.h"

// Forward declarations
namespace filesystem
{
    class path;
}

//--------------------------------------------------------------------------
// CodeGenerator::CUDA::Preferences
//--------------------------------------------------------------------------
namespace CodeGenerator
{
namespace CUDA
{
struct Preferences : public PreferencesBase
{
    //! Should PTX assembler information be displayed for each CUDA kernel during compilation
    bool showPtxInfo = false;

    //! Should block size optimisation be performed?
    bool optimiseBlockSize = true;

    //! Should GPU device be chosen automatically?
    bool autoChooseDevice = true;

    //! NVCC compiler options for all GPU code
    std::string userNvccFlags = "";
};

//--------------------------------------------------------------------------
// CodeGenerator::CUDA::Backend
//--------------------------------------------------------------------------
class BACKEND_EXPORT Backend : public BackendBase
{
public:
    //--------------------------------------------------------------------------
    // Enumerations
    //--------------------------------------------------------------------------
    enum Kernel
    {
        KernelNeuronUpdate,
        KernelPresynapticUpdate,
        KernelPostsynapticUpdate,
        KernelSynapseDynamicsUpdate,
        KernelInitialize,
        KernelInitializeSparse,
        KernelPreNeuronReset,
        KernelPreSynapseReset,
        KernelMax
    };
    
    //--------------------------------------------------------------------------
    // Type definitions
    //--------------------------------------------------------------------------
    using KernelBlockSize = std::array<size_t, KernelMax>;

    Backend(const KernelBlockSize &kernelBlockSizes, const Preferences &preferences, int localHostID, int device);

    //--------------------------------------------------------------------------
    // CodeGenerator::Backends:: virtuals
    //--------------------------------------------------------------------------
    virtual void genNeuronUpdate(CodeStream &os, const ModelSpecInternal &model, NeuronGroupSimHandler simHandler, NeuronGroupHandler wuVarUpdateHandler) const override;

    virtual void genSynapseUpdate(CodeStream &os, const ModelSpecInternal &model,
                                  SynapseGroupHandler wumThreshHandler, SynapseGroupHandler wumSimHandler, SynapseGroupHandler wumEventHandler,
                                  SynapseGroupHandler postLearnHandler, SynapseGroupHandler synapseDynamicsHandler) const override;

    virtual void genInit(CodeStream &os, const ModelSpecInternal &model,
                         NeuronGroupHandler localNGHandler, NeuronGroupHandler remoteNGHandler,
                         SynapseGroupHandler sgDenseInitHandler, SynapseGroupHandler sgSparseConnectHandler, 
                         SynapseGroupHandler sgSparseInitHandler) const override;

    virtual void genDefinitionsPreamble(CodeStream &os) const override;
    virtual void genDefinitionsInternalPreamble(CodeStream &os) const override;
    virtual void genRunnerPreamble(CodeStream &os) const override;
    virtual void genAllocateMemPreamble(CodeStream &os, const ModelSpecInternal &model) const override;
    virtual void genStepTimeFinalisePreamble(CodeStream &os, const ModelSpecInternal &model) const override;

    virtual void genVariableDefinition(CodeStream &definitions, CodeStream &definitionsInternal, const std::string &type, const std::string &name, VarLocation loc) const override;
    virtual void genVariableImplementation(CodeStream &os, const std::string &type, const std::string &name, VarLocation loc) const override;
    virtual void genVariableAllocation(CodeStream &os, const std::string &type, const std::string &name, VarLocation loc, size_t count) const override;
    virtual void genVariableFree(CodeStream &os, const std::string &name, VarLocation loc) const override;

    virtual void genExtraGlobalParamDefinition(CodeStream &definitions, const std::string &type, const std::string &name, VarLocation loc) const override;
    virtual void genExtraGlobalParamImplementation(CodeStream &os, const std::string &type, const std::string &name, VarLocation loc) const override;
    virtual void genExtraGlobalParamAllocation(CodeStream &os, const std::string &type, const std::string &name, VarLocation loc) const override;
     virtual void genExtraGlobalParamPush(CodeStream &os, const std::string &type, const std::string &name) const override;
    virtual void genExtraGlobalParamPull(CodeStream &os, const std::string &type, const std::string &name) const override;

    virtual void genPopVariableInit(CodeStream &os, VarLocation loc, const Substitutions &kernelSubs, Handler handler) const override;
    virtual void genVariableInit(CodeStream &os, VarLocation loc, size_t count, const std::string &indexVarName,
                                 const Substitutions &kernelSubs, Handler handler) const override;
    virtual void genSynapseVariableRowInit(CodeStream &os, VarLocation loc, const SynapseGroupInternal &sg,
                                           const Substitutions &kernelSubs, Handler handler) const override;

    virtual void genVariablePush(CodeStream &os, const std::string &type, const std::string &name, bool autoInitialized, size_t count) const override;
    virtual void genVariablePull(CodeStream &os, const std::string &type, const std::string &name, size_t count) const override;
    virtual void genCurrentTrueSpikePush(CodeStream &os, const NeuronGroupInternal &ng) const override
    {
        genCurrentSpikePush(os, ng, false);
    }
    virtual void genCurrentTrueSpikePull(CodeStream &os, const NeuronGroupInternal &ng) const override
    {
        genCurrentSpikePull(os, ng, false);
    }
    virtual void genCurrentSpikeLikeEventPush(CodeStream &os, const NeuronGroupInternal &ng) const override
    {
        genCurrentSpikePush(os, ng, true);
    }
    virtual void genCurrentSpikeLikeEventPull(CodeStream &os, const NeuronGroupInternal &ng) const override
    {
        genCurrentSpikePull(os, ng, true);
    }

    virtual void genGlobalRNG(CodeStream &definitions, CodeStream &definitionsInternal, CodeStream &runner, CodeStream &allocations, CodeStream &free, const ModelSpecInternal &model) const override;
    virtual void genPopulationRNG(CodeStream &definitions, CodeStream &definitionsInternal, CodeStream &runner, CodeStream &allocations, CodeStream &free,
                                  const std::string &name, size_t count) const override;
    virtual void genTimer(CodeStream &definitions, CodeStream &definitionsInternal, CodeStream &runner, CodeStream &allocations, CodeStream &free,
                          CodeStream &stepTimeFinalise, const std::string &name, bool updateInStepTime) const override;

    virtual void genMakefilePreamble(std::ostream &os) const override;
    virtual void genMakefileLinkRule(std::ostream &os) const override;
    virtual void genMakefileCompileRule(std::ostream &os) const override;

    virtual void genMSBuildConfigProperties(std::ostream &os) const override;
    virtual void genMSBuildImportProps(std::ostream &os) const override;
    virtual void genMSBuildItemDefinitions(std::ostream &os) const override;
    virtual void genMSBuildCompileModule(const std::string &moduleName, std::ostream &os) const override;
    virtual void genMSBuildImportTarget(std::ostream &os) const override;

    virtual std::string getVarPrefix() const override{ return "dd_"; }

    virtual bool isGlobalRNGRequired(const ModelSpecInternal &model) const override;
    virtual bool isSynRemapRequired() const override{ return true; }
    virtual bool isPostsynapticRemapRequired() const override{ return true; }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------
    const cudaDeviceProp &getChosenCUDADevice() const{ return m_ChosenDevice; }
    int getChosenDeviceID() const{ return m_ChosenDeviceID; }
    std::string getNVCCFlags() const;

    //--------------------------------------------------------------------------
    // Static API
    //--------------------------------------------------------------------------
    static size_t getNumPresynapticUpdateThreads(const SynapseGroupInternal &sg);
    static size_t getNumPostsynapticUpdateThreads(const SynapseGroupInternal &sg);
    static size_t getNumSynapseDynamicsThreads(const SynapseGroupInternal &sg);

    //--------------------------------------------------------------------------
    // Constants
    //--------------------------------------------------------------------------
    static const char *KernelNames[KernelMax];

private:
    //--------------------------------------------------------------------------
    // Type definitions
    //--------------------------------------------------------------------------
    template<typename T>
    using GetPaddedGroupSizeFunc = std::function<size_t(const T&)>;

    template<typename T>
    using FilterGroupFunc = std::function<bool(const T&)>;

    //--------------------------------------------------------------------------
    // Private methods
    //--------------------------------------------------------------------------
    template<typename T>
    void genParallelGroup(CodeStream &os, const Substitutions &kernelSubs, const std::map<std::string, T> &groups, size_t &idStart,
                          GetPaddedGroupSizeFunc<T> getPaddedSizeFunc,
                          FilterGroupFunc<T> filter, 
                          GroupHandler<T> handler) const
    {
        // Populate neuron update groups
        for (const auto &g : groups) {
            // If this synapse group should be processed
            Substitutions popSubs(&kernelSubs);
            if(filter(g.second)) {
                const size_t paddedSize = getPaddedSizeFunc(g.second);

                os << "// " << g.first << std::endl;

                // If this is the first  group
                if (idStart == 0) {
                    os << "if(id < " << paddedSize << ")" << CodeStream::OB(1);
                    popSubs.addVarSubstitution("id", "id");
                }
                else {
                    os << "if(id >= " << idStart << " && id < " << idStart + paddedSize << ")" << CodeStream::OB(1);
                    os << "const unsigned int lid = id - " << idStart << ";" << std::endl;
                    popSubs.addVarSubstitution("id", "lid");
                }

                handler(os, g.second, popSubs);

                idStart += paddedSize;
                os << CodeStream::CB(1) << std::endl;
            }
        }
    }

    template<typename T>
    void genParallelGroup(CodeStream &os, const Substitutions &kernelSubs, const std::map<std::string, T> &groups, size_t &idStart,
                          GetPaddedGroupSizeFunc<T> getPaddedSizeFunc,
                          GroupHandler<T> handler) const
    {
        genParallelGroup<T>(os, kernelSubs, groups, idStart, getPaddedSizeFunc,
                            [](const T&){ return true; }, handler);
    }

    void genEmitSpike(CodeStream &os, const Substitutions &subs, const std::string &suffix) const;

    void genCurrentSpikePush(CodeStream &os, const NeuronGroupInternal &ng, bool spikeEvent) const;
    void genCurrentSpikePull(CodeStream &os, const NeuronGroupInternal &ng, bool spikeEvent) const;

    void genPresynapticUpdatePreSpan(CodeStream &os, const ModelSpecInternal &model, const SynapseGroupInternal &sg, const Substitutions &popSubs, bool trueSpike,
                                     SynapseGroupHandler wumThreshHandler, SynapseGroupHandler wumSimHandler) const;
    void genPresynapticUpdatePostSpan(CodeStream &os, const ModelSpecInternal &model, const SynapseGroupInternal &sg, const Substitutions &popSubs, bool trueSpike,
                                      SynapseGroupHandler wumThreshHandler, SynapseGroupHandler wumSimHandler) const;

    void genKernelDimensions(CodeStream &os, Kernel kernel, size_t numThreads) const;

    bool shouldAccumulateInLinSyn(const SynapseGroupInternal &sg) const;

    bool shouldAccumulateInSharedMemory(const SynapseGroupInternal &sg) const;

    std::string getFloatAtomicAdd(const std::string &ftype) const;

    //--------------------------------------------------------------------------
    // Members
    //--------------------------------------------------------------------------
    const KernelBlockSize m_KernelBlockSizes;
    const Preferences m_Preferences;
    const int m_LocalHostID;
    
    const int m_ChosenDeviceID;
    cudaDeviceProp m_ChosenDevice;

    int m_RuntimeVersion;
};
}   // CUDA
}   // CodeGenerator
