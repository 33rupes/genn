#include "code_generator/generateRunner.h"

// Standard C++ includes
#include <random>
#include <sstream>
#include <string>

// GeNN includes
#include "gennUtils.h"

// GeNN code generator
#include "code_generator/codeGenUtils.h"
#include "code_generator/codeStream.h"
#include "code_generator/groupMerged.h"
#include "code_generator/substitutions.h"
#include "code_generator/teeStream.h"
#include "code_generator/backendBase.h"
#include "code_generator/mergedStructGenerator.h"
#include "code_generator/modelSpecMerged.h"

using namespace CodeGenerator;

//--------------------------------------------------------------------------
// Anonymous namespace
//--------------------------------------------------------------------------
namespace
{
void genTypeRange(CodeStream &os, const std::string &precision, const std::string &prefix)
{
    os << "#define " << prefix << "_MIN ";
    if (precision == "float") {
        Utils::writePreciseString(os, std::numeric_limits<float>::min());
        os << "f" << std::endl;
    }
    else {
        Utils::writePreciseString(os, std::numeric_limits<double>::min());
        os << std::endl;
    }

    os << "#define " << prefix << "_MAX ";
    if (precision == "float") {
        Utils::writePreciseString(os, std::numeric_limits<float>::max());
        os << "f" << std::endl;
    }
    else {
        Utils::writePreciseString(os, std::numeric_limits<double>::max());
        os << std::endl;
    }
    os << std::endl;
}
//-------------------------------------------------------------------------
void genSpikeMacros(CodeStream &os, const NeuronGroupInternal &ng, bool trueSpike)
{
    const bool delayRequired = trueSpike
        ? (ng.isDelayRequired() && ng.isTrueSpikeRequired())
        : ng.isDelayRequired();
    const std::string eventSuffix = trueSpike ? "" : "Evnt";
    const std::string eventMacroSuffix = trueSpike ? "" : "Event";

    // convenience macros for accessing spike count
    os << "#define spike" << eventMacroSuffix << "Count_" << ng.getName() << " glbSpkCnt" << eventSuffix << ng.getName();
    if (delayRequired) {
        os << "[spkQuePtr" << ng.getName() << "]";
    }
    else {
        os << "[0]";
    }
    os << std::endl;

    // convenience macro for accessing spikes
    os << "#define spike" << eventMacroSuffix << "_" << ng.getName();
    if (delayRequired) {
        os << " (glbSpk" << eventSuffix << ng.getName() << " + (spkQuePtr" << ng.getName() << " * " << ng.getNumNeurons() << "))";
    }
    else {
        os << " glbSpk" << eventSuffix << ng.getName();
    }
    os << std::endl;

    // convenience macro for accessing delay offset
    // **NOTE** we only require one copy of this so only ever write one for true spikes
    if(trueSpike) {
        os << "#define glbSpkShift" << ng.getName() << " ";
        if (delayRequired) {
            os << "spkQuePtr" << ng.getName() << "*" << ng.getNumNeurons();
        }
        else {
            os << "0";
        }
    }

    os << std::endl << std::endl;
}
//--------------------------------------------------------------------------
bool canPushPullVar(VarLocation loc)
{
    // A variable can be pushed and pulled if it is located on both host and device
    return ((loc & VarLocation::HOST) &&
            (loc & VarLocation::DEVICE));
}
//-------------------------------------------------------------------------
bool genVarPushPullScope(CodeStream &definitionsFunc, CodeStream &runnerPushFunc, CodeStream &runnerPullFunc,
                         VarLocation loc, bool automaticCopyEnabled, const std::string &description, std::function<void()> handler)
{
    // If this variable has a location that allows pushing and pulling and automatic copying isn't enabled
    if(canPushPullVar(loc) && !automaticCopyEnabled) {
        definitionsFunc << "EXPORT_FUNC void push" << description << "ToDevice(bool uninitialisedOnly = false);" << std::endl;
        definitionsFunc << "EXPORT_FUNC void pull" << description << "FromDevice();" << std::endl;

        runnerPushFunc << "void push" << description << "ToDevice(bool uninitialisedOnly)";
        runnerPullFunc << "void pull" << description << "FromDevice()";
        {
            CodeStream::Scope a(runnerPushFunc);
            CodeStream::Scope b(runnerPullFunc);

            handler();
        }
        runnerPushFunc << std::endl;
        runnerPullFunc << std::endl;

        return true;
    }
    else {
        return false;
    }
}
//-------------------------------------------------------------------------
void genVarPushPullScope(CodeStream &definitionsFunc, CodeStream &runnerPushFunc, CodeStream &runnerPullFunc,
                         VarLocation loc, bool automaticCopyEnabled, const std::string &description, std::vector<std::string> &statePushPullFunction,
                         std::function<void()> handler)
{
    // Add function to vector if push pull function was actually required
    if(genVarPushPullScope(definitionsFunc, runnerPushFunc, runnerPullFunc, loc, automaticCopyEnabled, description, handler)) {
        statePushPullFunction.push_back(description);
    }
}
//-------------------------------------------------------------------------
void genVarGetterScope(CodeStream &definitionsFunc, CodeStream &runnerGetterFunc,
                       VarLocation loc, const std::string &description, const std::string &type, std::function<void()> handler)
{
    // If this variable has a location that allows pushing and pulling and hence getting a host pointer
    if(canPushPullVar(loc)) {
        // Export getter
        definitionsFunc << "EXPORT_FUNC " << type << " get" << description << "();" << std::endl;

        // Define getter
        runnerGetterFunc << type << " get" << description << "()";
        {
            CodeStream::Scope a(runnerGetterFunc);
            handler();
        }
        runnerGetterFunc << std::endl;
    }
}
//-------------------------------------------------------------------------
void genSpikeGetters(CodeStream &definitionsFunc, CodeStream &runnerGetterFunc,
                     const NeuronGroupInternal &ng, bool trueSpike)
{
    const std::string eventSuffix = trueSpike ? "" : "Evnt";
    const bool delayRequired = trueSpike
        ? (ng.isDelayRequired() && ng.isTrueSpikeRequired())
        : ng.isDelayRequired();
    const VarLocation loc = trueSpike ? ng.getSpikeLocation() : ng.getSpikeEventLocation();

    // Generate getter for current spike counts
    genVarGetterScope(definitionsFunc, runnerGetterFunc,
                      loc, ng.getName() +  (trueSpike ? "CurrentSpikes" : "CurrentSpikeEvents"), "unsigned int*",
                      [&]()
                      {
                          runnerGetterFunc << "return ";
                          if (delayRequired) {
                              runnerGetterFunc << " (glbSpk" << eventSuffix << ng.getName() << " + (spkQuePtr" << ng.getName() << " * " << ng.getNumNeurons() << "));";
                          }
                          else {
                              runnerGetterFunc << " glbSpk" << eventSuffix << ng.getName() << ";";
                          }
                          runnerGetterFunc << std::endl;
                      });

    // Generate getter for current spikes
    genVarGetterScope(definitionsFunc, runnerGetterFunc,
                      loc, ng.getName() + (trueSpike ? "CurrentSpikeCount" : "CurrentSpikeEventCount"), "unsigned int&",
                      [&]()
                      {
                          runnerGetterFunc << "return glbSpkCnt" << eventSuffix << ng.getName();
                          if (delayRequired) {
                              runnerGetterFunc << "[spkQuePtr" << ng.getName() << "];";
                          }
                          else {
                              runnerGetterFunc << "[0];";
                          }
                          runnerGetterFunc << std::endl;
                      });


}
//-------------------------------------------------------------------------
void genStatePushPull(CodeStream &definitionsFunc, CodeStream &runnerPushFunc, CodeStream &runnerPullFunc,
                      const std::string &name, bool generateEmptyStatePushPull, 
                      const std::vector<std::string> &groupPushPullFunction, std::vector<std::string> &modelPushPullFunctions)
{
    // If we should either generate emtpy state push pull functions or this one won't be empty!
    if(generateEmptyStatePushPull || !groupPushPullFunction.empty()) {
        definitionsFunc << "EXPORT_FUNC void push" << name << "StateToDevice(bool uninitialisedOnly = false);" << std::endl;
        definitionsFunc << "EXPORT_FUNC void pull" << name << "StateFromDevice();" << std::endl;

        runnerPushFunc << "void push" << name << "StateToDevice(bool uninitialisedOnly)";
        runnerPullFunc << "void pull" << name << "StateFromDevice()";
        {
            CodeStream::Scope a(runnerPushFunc);
            CodeStream::Scope b(runnerPullFunc);

            for(const auto &func : groupPushPullFunction) {
                runnerPushFunc << "push" << func << "ToDevice(uninitialisedOnly);" << std::endl;
                runnerPullFunc << "pull" << func << "FromDevice();" << std::endl;
            }
        }
        runnerPushFunc << std::endl;
        runnerPullFunc << std::endl;

        // Add function to list
        modelPushPullFunctions.push_back(name);
    }
}
//-------------------------------------------------------------------------
MemAlloc genVariable(const BackendBase &backend, CodeStream &definitionsVar, CodeStream &definitionsFunc,
                     CodeStream &definitionsInternal, CodeStream &runner, CodeStream &allocations, CodeStream &free,
                     CodeStream &push, CodeStream &pull, const std::string &type, const std::string &name,
                     VarLocation loc, bool autoInitialized, size_t count, std::vector<std::string> &statePushPullFunction)
{
    // Generate push and pull functions
    genVarPushPullScope(definitionsFunc, push, pull, loc, backend.isAutomaticCopyEnabled(), name, statePushPullFunction,
        [&]()
        {
            backend.genVariablePushPull(push, pull, type, name, loc, autoInitialized, count);
        });

    // Generate variables
    return backend.genArray(definitionsVar, definitionsInternal, runner, allocations, free,
                            type, name, loc, count);
}
//-------------------------------------------------------------------------
void genExtraGlobalParam(const BackendBase &backend, CodeStream &definitionsVar,
                         CodeStream &definitionsFunc, CodeStream &runner,
                         CodeStream &extraGlobalParam, const MergedStructData &mergedStructData,
                         const std::string &type, const std::string &name, bool apiRequired, VarLocation loc)
{
    // Generate variables
    backend.genExtraGlobalParamDefinition(definitionsVar, type, name, loc);
    backend.genExtraGlobalParamImplementation(runner, type, name, loc);

    // If type is a pointer and API is required
    if(Utils::isTypePointer(type) && apiRequired) {
        // Write definitions for functions to allocate and free extra global param
        definitionsFunc << "EXPORT_FUNC void allocate" << name << "(unsigned int count);" << std::endl;
        definitionsFunc << "EXPORT_FUNC void free" << name << "();" << std::endl;

        // Write allocation function
        extraGlobalParam << "void allocate" << name << "(unsigned int count)";
        {
            CodeStream::Scope a(extraGlobalParam);
            backend.genExtraGlobalParamAllocation(extraGlobalParam, type, name, loc);

            // Get destinations in merged structures, this EGP 
            // needs to be copied to and call push function
            const auto &mergedDestinations = mergedStructData.getMergedEGPs().at(backend.getArrayPrefix() + name);
            for(const auto &v : mergedDestinations) {
                extraGlobalParam << "pushMerged" << v.first << v.second.mergedGroupIndex << v.second.fieldName << "ToDevice(";
                extraGlobalParam << v.second.groupIndex << ", " << name << ");" << std::endl;
            }
        }

        // Write free function
        extraGlobalParam << "void free" << name << "()";
        {
            CodeStream::Scope a(extraGlobalParam);
            backend.genVariableFree(extraGlobalParam, name, loc);
        }

        // If variable can be pushed and pulled
        if(!backend.isAutomaticCopyEnabled() && canPushPullVar(loc)) {
            // Write definitions for push and pull functions
            definitionsFunc << "EXPORT_FUNC void push" << name << "ToDevice(unsigned int count);" << std::endl;

            // Write push function
            extraGlobalParam << "void push" << name << "ToDevice(unsigned int count)";
            {
                CodeStream::Scope a(extraGlobalParam);
                backend.genExtraGlobalParamPush(extraGlobalParam, type, name, loc);
            }

            if(backend.shouldGenerateExtraGlobalParamPull()) {
                // Write definitions for pull functions
                definitionsFunc << "EXPORT_FUNC void pull" << name << "FromDevice(unsigned int count);" << std::endl;

                // Write pull function
                extraGlobalParam << "void pull" << name << "FromDevice(unsigned int count)";
                {
                    CodeGenerator::CodeStream::Scope a(extraGlobalParam);
                    backend.genExtraGlobalParamPull(extraGlobalParam, type, name, loc);
                }
            }
        }

    }
}
//-------------------------------------------------------------------------
MemAlloc genGlobalHostRNG(CodeStream &definitionsVar, CodeStream &runnerVarDecl,
                          CodeStream &runnerVarAlloc, unsigned int seed)
{
    definitionsVar << "EXPORT_VAR " << "std::mt19937 rng;" << std::endl;
    runnerVarDecl << "std::mt19937 rng;" << std::endl;

    // If no seed is specified, use system randomness to generate seed sequence
    CodeStream::Scope b(runnerVarAlloc);
    if(seed == 0) {
        runnerVarAlloc << "uint32_t seedData[std::mt19937::state_size];" << std::endl;
        runnerVarAlloc << "std::random_device seedSource;" << std::endl;
        {
            CodeStream::Scope b(runnerVarAlloc);
            runnerVarAlloc << "for(int i = 0; i < std::mt19937::state_size; i++)";
            {
                CodeStream::Scope b(runnerVarAlloc);
                runnerVarAlloc << "seedData[i] = seedSource();" << std::endl;
            }
        }
        runnerVarAlloc << "std::seed_seq seeds(std::begin(seedData), std::end(seedData));" << std::endl;
    }
    // Otherwise, create a seed sequence from model seed
    // **NOTE** this is a terrible idea see http://www.pcg-random.org/posts/cpp-seeding-surprises.html
    else {
        runnerVarAlloc << "std::seed_seq seeds{" << seed << "};" << std::endl;
    }

    // Seed RNG from seed sequence
    runnerVarAlloc << "rng.seed(seeds);" << std::endl;

    // Add size of Mersenne Twister to memory tracker
    return MemAlloc::host(sizeof(std::mt19937));
}
//-------------------------------------------------------------------------
void genSynapseConnectivityHostInit(const BackendBase &backend, CodeStream &os, 
                                    const SynapseConnectivityHostInitGroupMerged &sg, const std::string &precision)
{
    CodeStream::Scope b(os);
    os << "// merged synapse connectivity host init group " << sg.getIndex() << std::endl;
    os << "for(unsigned int g = 0; g < " << sg.getGroups().size() << "; g++)";
    {
        CodeStream::Scope b(os);

        // Get reference to group
        os << "const auto &group = mergedSynapseConnectivityHostInitGroup" << sg.getIndex() << "[g]; " << std::endl;

        const auto &connectInit = sg.getArchetype().getConnectivityInitialiser();

        // If matrix type is procedural then initialized connectivity init snippet will potentially be used with multiple threads per spike. 
        // Otherwise it will only ever be used for initialization which uses one thread per row
        const size_t numThreads = (sg.getArchetype().getMatrixType() & SynapseMatrixConnectivity::PROCEDURAL) ? sg.getArchetype().getNumThreadsPerSpike() : 1;

        // Create substitutions
        Substitutions subs;
        subs.addVarSubstitution("rng", "rng");
        subs.addVarSubstitution("num_pre", "group.numSrcNeurons");
        subs.addVarSubstitution("num_post", "group.numTrgNeurons");
        subs.addVarSubstitution("num_threads", std::to_string(numThreads));
        subs.addVarNameSubstitution(connectInit.getSnippet()->getExtraGlobalParams(), "", "*group.");
        subs.addParamValueSubstitution(connectInit.getSnippet()->getParamNames(), connectInit.getParams(),
                                       [&sg](size_t p) { return sg.isConnectivityHostInitParamHeterogeneous(p); },
                                       "", "group.");
        subs.addVarValueSubstitution(connectInit.getSnippet()->getDerivedParams(), connectInit.getDerivedParams(),
                                     [&sg](size_t p) { return sg.isConnectivityHostInitDerivedParamHeterogeneous(p); },
                                     "", "group.");

        // Loop through EGPs
        const auto egps = connectInit.getSnippet()->getExtraGlobalParams();
        for(size_t i = 0; i < egps.size(); i++) {
            const auto loc = sg.getArchetype().getSparseConnectivityExtraGlobalParamLocation(i);
            // If EGP is a pointer and located on the host
            if(Utils::isTypePointer(egps[i].type) && (loc & VarLocation::HOST)) {
                // Generate code to allocate this EGP with count specified by $(0)
                std::stringstream allocStream;
                CodeGenerator::CodeStream alloc(allocStream);
                backend.genExtraGlobalParamAllocation(alloc, egps[i].type + "*", egps[i].name,
                                                      loc, "$(0)", "group.");

                // Add substitution
                subs.addFuncSubstitution("allocate" + egps[i].name, 1, allocStream.str());

                // Generate code to push this EGP with count specified by $(0)
                std::stringstream pushStream;
                CodeStream push(pushStream);
                backend.genExtraGlobalParamPush(push, egps[i].type + "*", egps[i].name,
                                                loc, "$(0)", "group.");


                // Add substitution
                subs.addFuncSubstitution("push" + egps[i].name, 1, pushStream.str());
            }
        }
        std::string code = connectInit.getSnippet()->getHostInitCode();
        subs.applyCheckUnreplaced(code, "hostInitSparseConnectivity : merged" + std::to_string(sg.getIndex()));
        code = ensureFtype(code, precision);

        // Write out code
        os << code << std::endl;

    }
}
}   // Anonymous namespace

//--------------------------------------------------------------------------
// CodeGenerator
//--------------------------------------------------------------------------
MemAlloc CodeGenerator::generateRunner(CodeStream &definitions, CodeStream &definitionsInternal, CodeStream &runner,
                                       MergedStructData &mergedStructData, const ModelSpecMerged &modelMerged,
                                       const BackendBase &backend)
{
    // Track memory allocations, initially starting from zero
    auto mem = MemAlloc::zero();

    // Write definitions preamble
    definitions << "#pragma once" << std::endl;

#ifdef _WIN32
    definitions << "#ifdef BUILDING_GENERATED_CODE" << std::endl;
    definitions << "#define EXPORT_VAR __declspec(dllexport) extern" << std::endl;
    definitions << "#define EXPORT_FUNC __declspec(dllexport)" << std::endl;
    definitions << "#else" << std::endl;
    definitions << "#define EXPORT_VAR __declspec(dllimport) extern" << std::endl;
    definitions << "#define EXPORT_FUNC __declspec(dllimport)" << std::endl;
    definitions << "#endif" << std::endl;
#else
    definitions << "#define EXPORT_VAR extern" << std::endl;
    definitions << "#define EXPORT_FUNC" << std::endl;
#endif
    backend.genDefinitionsPreamble(definitions, modelMerged);

    // Write definitions internal preamble
    definitionsInternal << "#pragma once" << std::endl;
    definitionsInternal << "#include \"definitions.h\"" << std::endl << std::endl;
    backend.genDefinitionsInternalPreamble(definitionsInternal, modelMerged);
    
    // write DT macro
    const ModelSpecInternal &model = modelMerged.getModel();
    if (model.getTimePrecision() == "float") {
        definitions << "#define DT " << std::to_string(model.getDT()) << "f" << std::endl;
    } else {
        definitions << "#define DT " << std::to_string(model.getDT()) << std::endl;
    }

    // Typedefine scalar type
    definitions << "typedef " << model.getPrecision() << " scalar;" << std::endl;

    // Write ranges of scalar and time types
    genTypeRange(definitions, model.getPrecision(), "SCALAR");
    genTypeRange(definitions, model.getTimePrecision(), "TIME");

    definitions << "// ------------------------------------------------------------------------" << std::endl;
    definitions << "// bit tool macros" << std::endl;
    definitions << "#define B(x,i) ((x) & (0x80000000 >> (i))) //!< Extract the bit at the specified position i from x" << std::endl;
    definitions << "#define setB(x,i) x= ((x) | (0x80000000 >> (i))) //!< Set the bit at the specified position i in x to 1" << std::endl;
    definitions << "#define delB(x,i) x= ((x) & (~(0x80000000 >> (i)))) //!< Set the bit at the specified position i in x to 0" << std::endl;
    definitions << std::endl;

    // Write runner preamble
    runner << "#include \"definitionsInternal.h\"" << std::endl << std::endl;
    backend.genRunnerPreamble(runner, modelMerged);

    // Create codestreams to generate different sections of runner and definitions
    std::stringstream runnerVarDeclStream;
    std::stringstream runnerVarAllocStream;
    std::stringstream runnerMergedStructAllocStream;
    std::stringstream runnerVarFreeStream;
    std::stringstream runnerExtraGlobalParamFuncStream;
    std::stringstream runnerPushFuncStream;
    std::stringstream runnerPullFuncStream;
    std::stringstream runnerGetterFuncStream;
    std::stringstream runnerStepTimeFinaliseStream;
    std::stringstream definitionsVarStream;
    std::stringstream definitionsFuncStream;
    std::stringstream definitionsInternalVarStream;
    std::stringstream definitionsInternalFuncStream;
    CodeStream runnerVarDecl(runnerVarDeclStream);
    CodeStream runnerVarAlloc(runnerVarAllocStream);
    CodeStream runnerMergedStructAlloc(runnerMergedStructAllocStream);
    CodeStream runnerVarFree(runnerVarFreeStream);
    CodeStream runnerExtraGlobalParamFunc(runnerExtraGlobalParamFuncStream);
    CodeStream runnerPushFunc(runnerPushFuncStream);
    CodeStream runnerPullFunc(runnerPullFuncStream);
    CodeStream runnerGetterFunc(runnerGetterFuncStream);
    CodeStream runnerStepTimeFinalise(runnerStepTimeFinaliseStream);
    CodeStream definitionsVar(definitionsVarStream);
    CodeStream definitionsFunc(definitionsFuncStream);
    CodeStream definitionsInternalVar(definitionsInternalVarStream);
    CodeStream definitionsInternalFunc(definitionsInternalFuncStream);

    // Create a teestream to allow simultaneous writing to all streams
    TeeStream allVarStreams(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree);

    // Begin extern C block around variable declarations
    runnerVarDecl << "extern \"C\" {" << std::endl;
    definitionsVar << "extern \"C\" {" << std::endl;
    definitionsInternalVar << "extern \"C\" {" << std::endl;

    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    allVarStreams << "// global variables" << std::endl;
    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;

    // Define and declare time variables
    definitionsVar << "EXPORT_VAR unsigned long long iT;" << std::endl;
    definitionsVar << "EXPORT_VAR " << model.getTimePrecision() << " t;" << std::endl;
    runnerVarDecl << "unsigned long long iT;" << std::endl;
    runnerVarDecl << model.getTimePrecision() << " t;" << std::endl;

    // If backend requires a global device RNG to simulate (or initialize) this model
    if(backend.isGlobalDeviceRNGRequired(modelMerged)) {
        mem += backend.genGlobalDeviceRNG(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree);
    }
    // If backend required a global host RNG to simulate (or initialize) this model, generate a standard Mersenne Twister
    if(backend.isGlobalHostRNGRequired(modelMerged)) {
        mem += genGlobalHostRNG(definitionsVar, runnerVarDecl, runnerVarAlloc, model.getSeed());
    }
    allVarStreams << std::endl;

    // Generate preamble for the final stage of time step
    // **NOTE** this is done now as there can be timing logic here
    backend.genStepTimeFinalisePreamble(runnerStepTimeFinalise, modelMerged);

    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    allVarStreams << "// timers" << std::endl;
    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;

    // Generate scalars to store total elapsed time
    // **NOTE** we ALWAYS generate these so usercode doesn't require #ifdefs around timing code
    backend.genScalar(definitionsVar, definitionsInternalVar, runnerVarDecl, "double", "neuronUpdateTime", VarLocation::HOST);
    backend.genScalar(definitionsVar, definitionsInternalVar, runnerVarDecl, "double", "initTime", VarLocation::HOST);
    backend.genScalar(definitionsVar, definitionsInternalVar, runnerVarDecl, "double", "presynapticUpdateTime", VarLocation::HOST);
    backend.genScalar(definitionsVar, definitionsInternalVar, runnerVarDecl, "double", "postsynapticUpdateTime", VarLocation::HOST);
    backend.genScalar(definitionsVar, definitionsInternalVar, runnerVarDecl, "double", "synapseDynamicsTime", VarLocation::HOST);
    backend.genScalar(definitionsVar, definitionsInternalVar, runnerVarDecl, "double", "initSparseTime", VarLocation::HOST);

    // If timing is actually enabled
    if(model.isTimingEnabled()) {
        // Create neuron timer
        backend.genTimer(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                         runnerStepTimeFinalise, "neuronUpdate", true);

        // Add presynaptic update timer
        if(!modelMerged.getMergedPresynapticUpdateGroups().empty()) {
            backend.genTimer(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                             runnerStepTimeFinalise, "presynapticUpdate", true);
        }

        // Add postsynaptic update timer if required
        if(!modelMerged.getMergedPostsynapticUpdateGroups().empty()) {
            backend.genTimer(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                             runnerStepTimeFinalise, "postsynapticUpdate", true);
        }

        // Add synapse dynamics update timer if required
        if(!modelMerged.getMergedSynapseDynamicsGroups().empty()) {
            backend.genTimer(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                             runnerStepTimeFinalise, "synapseDynamics", true);
        }

        // Create init timer
        backend.genTimer(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                         runnerStepTimeFinalise, "init", false);

        // Add sparse initialisation timer
        if(!modelMerged.getMergedSynapseSparseInitGroups().empty()) {
            backend.genTimer(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                             runnerStepTimeFinalise, "initSparse", false);
        }

        allVarStreams << std::endl;
    }

    runnerVarDecl << "// ------------------------------------------------------------------------" << std::endl;
    runnerVarDecl << "// merged group arrays" << std::endl;
    runnerVarDecl << "// ------------------------------------------------------------------------" << std::endl;

    definitionsInternal << "// ------------------------------------------------------------------------" << std::endl;
    definitionsInternal << "// merged group structures" << std::endl;
    definitionsInternal << "// ------------------------------------------------------------------------" << std::endl;

    definitionsInternalVar << "// ------------------------------------------------------------------------" << std::endl;
    definitionsInternalVar << "// merged group arrays for host initialisation" << std::endl;
    definitionsInternalVar << "// ------------------------------------------------------------------------" << std::endl;

    definitionsInternalFunc << "// ------------------------------------------------------------------------" << std::endl;
    definitionsInternalFunc << "// copying merged group structures to device" << std::endl;
    definitionsInternalFunc << "// ------------------------------------------------------------------------" << std::endl;

    // Loop through merged synapse connectivity host initialisation groups
    for(const auto &m : modelMerged.getMergedSynapseConnectivityHostInitGroups()) {
        m.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar,
                   runnerVarDecl, runnerMergedStructAlloc, mergedStructData, model.getPrecision());
    }

    // Loop through merged synapse connectivity host init groups and generate host init code
    // **NOTE** this is done here so valid pointers get copied straight into subsequent structures and merged EGP system isn't required
    for(const auto &sg : modelMerged.getMergedSynapseConnectivityHostInitGroups()) {
        genSynapseConnectivityHostInit(backend, runnerMergedStructAlloc, sg, model.getPrecision());
    }

    // Generate merged neuron initialisation groups
    for(const auto &m : modelMerged.getMergedNeuronInitGroups()) {
        m.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar,
                   runnerVarDecl, runnerMergedStructAlloc, mergedStructData, 
                   model.getPrecision(), model.getTimePrecision());
    }

    // Loop through merged dense synapse init groups
    for(const auto &m : modelMerged.getMergedSynapseDenseInitGroups()) {
         m.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar,
                    runnerVarDecl, runnerMergedStructAlloc, mergedStructData, 
                    model.getPrecision(), model.getTimePrecision());
    }

    // Loop through merged synapse connectivity initialisation groups
    for(const auto &m : modelMerged.getMergedSynapseConnectivityInitGroups()) {
        m.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar,
                   runnerVarDecl, runnerMergedStructAlloc, mergedStructData, model.getPrecision());
    }

    // Loop through merged sparse synapse init groups
    for(const auto &m : modelMerged.getMergedSynapseSparseInitGroups()) {
        m.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar,
                   runnerVarDecl, runnerMergedStructAlloc, mergedStructData, 
                   model.getPrecision(), model.getTimePrecision());
    }

    // Loop through merged neuron update groups
    for(const auto &m : modelMerged.getMergedNeuronUpdateGroups()) {
        m.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar,
                   runnerVarDecl, runnerMergedStructAlloc, mergedStructData, 
                   model.getPrecision(), model.getTimePrecision());
    }

    // Loop through merged presynaptic update groups
    for(const auto &m : modelMerged.getMergedPresynapticUpdateGroups()) {
        m.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar,
                   runnerVarDecl, runnerMergedStructAlloc, mergedStructData, 
                   model.getPrecision(), model.getTimePrecision());
    }

    // Loop through merged postsynaptic update groups
    for(const auto &m : modelMerged.getMergedPostsynapticUpdateGroups()) {
        m.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar,
                   runnerVarDecl, runnerMergedStructAlloc, mergedStructData, 
                   model.getPrecision(), model.getTimePrecision());
    }

    // Loop through synapse dynamics groups
    for(const auto &m : modelMerged.getMergedSynapseDynamicsGroups()) {
        m.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar,
                   runnerVarDecl, runnerMergedStructAlloc, mergedStructData, 
                   model.getPrecision(), model.getTimePrecision());
    }

    // Loop through neuron groups whose spike queues need resetting
    for(const auto &m : modelMerged.getMergedNeuronSpikeQueueUpdateGroups()) {
        m.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar, 
                   runnerVarDecl, runnerMergedStructAlloc,
                   mergedStructData, model.getPrecision());
    }

    // Loop through synapse groups whose dendritic delay pointers need updating
    for(const auto &m : modelMerged.getMergedSynapseDendriticDelayUpdateGroups()) {
       m.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar, 
                  runnerVarDecl, runnerMergedStructAlloc,
                  mergedStructData, model.getPrecision());
    }

    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    allVarStreams << "// local neuron groups" << std::endl;
    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    std::vector<std::string> currentSpikePullFunctions;
    std::vector<std::string> currentSpikeEventPullFunctions;
    std::vector<std::string> statePushPullFunctions;
    for(const auto &n : model.getNeuronGroups()) {
        // Write convenience macros to access spikes
        genSpikeMacros(definitionsVar, n.second, true);

        // True spike variables
        const size_t numNeuronDelaySlots = n.second.getNumNeurons() * n.second.getNumDelaySlots();
        const size_t numSpikeCounts = n.second.isTrueSpikeRequired() ? n.second.getNumDelaySlots() : 1;
        const size_t numSpikes = n.second.isTrueSpikeRequired() ? numNeuronDelaySlots : n.second.getNumNeurons();
        mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                "unsigned int", "glbSpkCnt" + n.first, n.second.getSpikeLocation(), numSpikeCounts);
        mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                "unsigned int", "glbSpk" + n.first, n.second.getSpikeLocation(), numSpikes);

        // True spike push and pull functions
        genVarPushPullScope(definitionsFunc, runnerPushFunc, runnerPullFunc, n.second.getSpikeLocation(),
                            backend.isAutomaticCopyEnabled(), n.first + "Spikes",
            [&]()
            {
                backend.genVariablePushPull(runnerPushFunc, runnerPullFunc,
                                            "unsigned int", "glbSpkCnt" + n.first, n.second.getSpikeLocation(), true, numSpikeCounts);
                backend.genVariablePushPull(runnerPushFunc, runnerPullFunc,
                                            "unsigned int", "glbSpk" + n.first, n.second.getSpikeLocation(), true, numSpikes);
            });

        // Current true spike push and pull functions
        genVarPushPullScope(definitionsFunc, runnerPushFunc, runnerPullFunc, n.second.getSpikeLocation(),
                            backend.isAutomaticCopyEnabled(), n.first + "CurrentSpikes", currentSpikePullFunctions,
            [&]()
            {
                backend.genCurrentTrueSpikePush(runnerPushFunc, n.second);
                backend.genCurrentTrueSpikePull(runnerPullFunc, n.second);
            });

        // Current true spike getter functions
        genSpikeGetters(definitionsFunc, runnerGetterFunc, n.second, true);

        // If neuron ngroup eeds to emit spike-like events
        if (n.second.isSpikeEventRequired()) {
            // Write convenience macros to access spike-like events
            genSpikeMacros(definitionsVar, n.second, false);

            // Spike-like event variables
            mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                    "unsigned int", "glbSpkCntEvnt" + n.first, n.second.getSpikeEventLocation(),
                                    n.second.getNumDelaySlots());
            mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                    "unsigned int", "glbSpkEvnt" + n.first, n.second.getSpikeEventLocation(),
                                    numNeuronDelaySlots);

            // Spike-like event push and pull functions
            genVarPushPullScope(definitionsFunc, runnerPushFunc, runnerPullFunc, n.second.getSpikeEventLocation(),
                                backend.isAutomaticCopyEnabled(), n.first + "SpikeEvents",
                [&]()
                {
                    backend.genVariablePushPull(runnerPushFunc, runnerPullFunc,
                                                "unsigned int", "glbSpkCntEvnt" + n.first, n.second.getSpikeLocation(), true, n.second.getNumDelaySlots());
                    backend.genVariablePushPull(runnerPushFunc, runnerPullFunc,
                                                "unsigned int", "glbSpkEvnt" + n.first, n.second.getSpikeLocation(), true, numNeuronDelaySlots);
                });

            // Current spike-like event push and pull functions
            genVarPushPullScope(definitionsFunc, runnerPushFunc, runnerPullFunc, n.second.getSpikeEventLocation(),
                                backend.isAutomaticCopyEnabled(), n.first + "CurrentSpikeEvents", currentSpikeEventPullFunctions,
                [&]()
                {
                    backend.genCurrentSpikeLikeEventPush(runnerPushFunc, n.second);
                    backend.genCurrentSpikeLikeEventPull(runnerPullFunc, n.second);
                });

            // Current true spike getter functions
            genSpikeGetters(definitionsFunc, runnerGetterFunc, n.second, false);
        }

        // If neuron group has axonal delays
        if (n.second.isDelayRequired()) {
            backend.genScalar(definitionsVar, definitionsInternalVar, runnerVarDecl, "unsigned int", "spkQuePtr" + n.first, VarLocation::HOST_DEVICE);
        }

        // If neuron group needs to record its spike times
        if (n.second.isSpikeTimeRequired()) {
            mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                    model.getTimePrecision(), "sT" + n.first, n.second.getSpikeTimeLocation(),
                                    numNeuronDelaySlots);

            // Generate push and pull functions
            genVarPushPullScope(definitionsFunc, runnerPushFunc, runnerPullFunc, n.second.getSpikeTimeLocation(),
                                backend.isAutomaticCopyEnabled(), n.first + "SpikeTimes",
                [&]()
                {
                    backend.genVariablePushPull(runnerPushFunc, runnerPullFunc, model.getTimePrecision(),
                                                "sT" + n.first, n.second.getSpikeTimeLocation(), true, 
                                                numNeuronDelaySlots);
                });
        }

        // If neuron group needs per-neuron RNGs
        if(n.second.isSimRNGRequired()) {
            mem += backend.genPopulationRNG(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree, "rng" + n.first, n.second.getNumNeurons());
        }

        // Neuron state variables
        const auto neuronModel = n.second.getNeuronModel();
        const auto vars = neuronModel->getVars();
        std::vector<std::string> neuronStatePushPullFunctions;
        for(size_t i = 0; i < vars.size(); i++) {
            const size_t count = n.second.isVarQueueRequired(i) ? n.second.getNumNeurons() * n.second.getNumDelaySlots() : n.second.getNumNeurons();
            const bool autoInitialized = !n.second.getVarInitialisers()[i].getSnippet()->getCode().empty();
            mem += genVariable(backend, definitionsVar, definitionsFunc, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                               runnerPushFunc, runnerPullFunc, vars[i].type, vars[i].name + n.first,
                               n.second.getVarLocation(i), autoInitialized, count, neuronStatePushPullFunctions);

            // Current variable push and pull functions
            genVarPushPullScope(definitionsFunc, runnerPushFunc, runnerPullFunc, n.second.getVarLocation(i),
                                backend.isAutomaticCopyEnabled(), "Current" + vars[i].name + n.first,
                [&]()
                {
                    backend.genCurrentVariablePushPull(runnerPushFunc, runnerPullFunc, n.second, vars[i].type,
                                                    vars[i].name, n.second.getVarLocation(i));
                });

            // Write getter to get access to correct pointer
            const bool delayRequired = (n.second.isVarQueueRequired(i) &&  n.second.isDelayRequired());
            genVarGetterScope(definitionsFunc, runnerGetterFunc, n.second.getVarLocation(i),
                              "Current" + vars[i].name + n.first, vars[i].type + "*",
                [&]()
                {
                    if(delayRequired) {
                        runnerGetterFunc << "return " << vars[i].name << n.first << " + (spkQuePtr" << n.first << " * " << n.second.getNumNeurons() << ");" << std::endl;
                    }
                    else {
                        runnerGetterFunc << "return " << vars[i].name << n.first << ";" << std::endl;
                    }
                });
        }

        // Add helper function to push and pull entire neuron state
        if(!backend.isAutomaticCopyEnabled()) {
            genStatePushPull(definitionsFunc, runnerPushFunc, runnerPullFunc, 
                             n.first, backend.shouldGenerateEmptyStatePushPull(), 
                             neuronStatePushPullFunctions, statePushPullFunctions);
        }

        const auto extraGlobalParams = neuronModel->getExtraGlobalParams();
        for(size_t i = 0; i < extraGlobalParams.size(); i++) {
            genExtraGlobalParam(backend, definitionsVar, definitionsFunc, runnerVarDecl, runnerExtraGlobalParamFunc,
                                mergedStructData, extraGlobalParams[i].type, extraGlobalParams[i].name + n.first,
                                true, n.second.getExtraGlobalParamLocation(i));
        }

        if(!n.second.getCurrentSources().empty()) {
            allVarStreams << "// current source variables" << std::endl;
        }
        for (auto const *cs : n.second.getCurrentSources()) {
            const auto csModel = cs->getCurrentSourceModel();
            const auto csVars = csModel->getVars();

            std::vector<std::string> currentSourceStatePushPullFunctions;
            for(size_t i = 0; i < csVars.size(); i++) {
                const bool autoInitialized = !cs->getVarInitialisers()[i].getSnippet()->getCode().empty();
                mem += genVariable(backend, definitionsVar, definitionsFunc, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                   runnerPushFunc, runnerPullFunc, csVars[i].type, csVars[i].name + cs->getName(),
                                   cs->getVarLocation(i), autoInitialized, n.second.getNumNeurons(), currentSourceStatePushPullFunctions);
            }

            // Add helper function to push and pull entire current source state
            if(!backend.isAutomaticCopyEnabled()) {
                genStatePushPull(definitionsFunc, runnerPushFunc, runnerPullFunc, 
                                 cs->getName(), backend.shouldGenerateEmptyStatePushPull(), 
                                 currentSourceStatePushPullFunctions, statePushPullFunctions);
            }

            const auto csExtraGlobalParams = csModel->getExtraGlobalParams();
            for(size_t i = 0; i < csExtraGlobalParams.size(); i++) {
                genExtraGlobalParam(backend, definitionsVar, definitionsFunc, runnerVarDecl, runnerExtraGlobalParamFunc,
                                    mergedStructData, csExtraGlobalParams[i].type, csExtraGlobalParams[i].name + cs->getName(),
                                    true, cs->getExtraGlobalParamLocation(i));
            }
        }
    }
    allVarStreams << std::endl;

    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    allVarStreams << "// postsynaptic variables" << std::endl;
    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    for(const auto &n : model.getNeuronGroups()) {
        // Loop through merged incoming synaptic populations
        // **NOTE** because of merging we need to loop through postsynaptic models in this
        for(const auto &m : n.second.getMergedInSyn()) {
            const auto *sg = m.first;

            mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                    model.getPrecision(), "inSyn" + sg->getPSModelTargetName(), sg->getInSynLocation(),
                                    sg->getTrgNeuronGroup()->getNumNeurons());

            if (sg->isDendriticDelayRequired()) {
                mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                        model.getPrecision(), "denDelay" + sg->getPSModelTargetName(), sg->getDendriticDelayLocation(),
                                        sg->getMaxDendriticDelayTimesteps() * sg->getTrgNeuronGroup()->getNumNeurons());
                backend.genScalar(definitionsVar, definitionsInternalVar, runnerVarDecl, "unsigned int", "denDelayPtr" + sg->getPSModelTargetName(), VarLocation::HOST_DEVICE);
            }

            if (sg->getMatrixType() & SynapseMatrixWeight::INDIVIDUAL_PSM) {
                for(const auto &v : sg->getPSModel()->getVars()) {
                    mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                            v.type, v.name + sg->getPSModelTargetName(), sg->getPSVarLocation(v.name),
                                            sg->getTrgNeuronGroup()->getNumNeurons());
                }
            }
        }
    }
    allVarStreams << std::endl;

    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    allVarStreams << "// synapse connectivity" << std::endl;
    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    std::vector<std::string> connectivityPushPullFunctions;
    for(const auto &s : model.getSynapseGroups()) {
        const bool autoInitialized = !s.second.getConnectivityInitialiser().getSnippet()->getRowBuildCode().empty();

        if (s.second.getMatrixType() & SynapseMatrixConnectivity::BITMASK) {
            const size_t gpSize = ceilDivide((size_t)s.second.getSrcNeuronGroup()->getNumNeurons() * backend.getSynapticMatrixRowStride(s.second), 32);
            mem += genVariable(backend, definitionsVar, definitionsFunc, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                            runnerPushFunc, runnerPullFunc, "uint32_t", "gp" + s.second.getName(),
                            s.second.getSparseConnectivityLocation(), autoInitialized, gpSize, connectivityPushPullFunctions);

        }
        else if(s.second.getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
            const VarLocation varLoc = s.second.getSparseConnectivityLocation();
            const size_t size = s.second.getSrcNeuronGroup()->getNumNeurons() * backend.getSynapticMatrixRowStride(s.second);

            // Maximum row length constant
            definitionsVar << "EXPORT_VAR const unsigned int maxRowLength" << s.second.getName() << ";" << std::endl;
            runnerVarDecl << "const unsigned int maxRowLength" << s.second.getName() << " = " << backend.getSynapticMatrixRowStride(s.second) << ";" << std::endl;

            // Row lengths
            mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                    "unsigned int", "rowLength" + s.second.getName(), varLoc, s.second.getSrcNeuronGroup()->getNumNeurons());

            // Target indices
            mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                    s.second.getSparseIndType(), "ind" + s.second.getName(), varLoc, size);

            // **TODO** remap is not always required
            if(backend.isSynRemapRequired() && !s.second.getWUModel()->getSynapseDynamicsCode().empty()) {
                // Allocate synRemap
                // **THINK** this is over-allocating
                mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                        "unsigned int", "synRemap" + s.second.getName(), VarLocation::DEVICE, size + 1);
            }

            // **TODO** remap is not always required
            if(backend.isPostsynapticRemapRequired() && !s.second.getWUModel()->getLearnPostCode().empty()) {
                const size_t postSize = (size_t)s.second.getTrgNeuronGroup()->getNumNeurons() * (size_t)s.second.getMaxSourceConnections();

                // Allocate column lengths
                mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                        "unsigned int", "colLength" + s.second.getName(), VarLocation::DEVICE, s.second.getTrgNeuronGroup()->getNumNeurons());

                // Allocate remap
                mem += backend.genArray(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                        "unsigned int", "remap" + s.second.getName(), VarLocation::DEVICE, postSize);
            }

            // Generate push and pull functions for sparse connectivity
            genVarPushPullScope(definitionsFunc, runnerPushFunc, runnerPullFunc, s.second.getSparseConnectivityLocation(),
                                backend.isAutomaticCopyEnabled(), s.second.getName() + "Connectivity", connectivityPushPullFunctions,
                [&]()
                {
                    // Row lengths
                    backend.genVariablePushPull(runnerPushFunc, runnerPullFunc,
                                                "unsigned int", "rowLength" + s.second.getName(), s.second.getSparseConnectivityLocation(), autoInitialized, s.second.getSrcNeuronGroup()->getNumNeurons());

                    // Target indices
                    backend.genVariablePushPull(runnerPushFunc, runnerPullFunc,
                                                "unsigned int", "ind" + s.second.getName(), s.second.getSparseConnectivityLocation(), autoInitialized, size);
                });
        }
    }
    allVarStreams << std::endl;

    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    allVarStreams << "// synapse variables" << std::endl;
    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    for(const auto &s : model.getSynapseGroups()) {
        const auto *wu = s.second.getWUModel();
        const auto *psm = s.second.getPSModel();

        // If weight update variables should be individual
        std::vector<std::string> synapseGroupStatePushPullFunctions;
        if (s.second.getMatrixType() & SynapseMatrixWeight::INDIVIDUAL) {
            const size_t size = s.second.getSrcNeuronGroup()->getNumNeurons() * backend.getSynapticMatrixRowStride(s.second);

            const auto wuVars = wu->getVars();
            for(size_t i = 0; i < wuVars.size(); i++) {
                const bool autoInitialized = !s.second.getWUVarInitialisers()[i].getSnippet()->getCode().empty();
                mem += genVariable(backend, definitionsVar, definitionsFunc, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                                runnerPushFunc, runnerPullFunc, wuVars[i].type, wuVars[i].name + s.second.getName(),
                                s.second.getWUVarLocation(i), autoInitialized, size, synapseGroupStatePushPullFunctions);
            }
        }

        // Presynaptic W.U.M. variables
        const size_t preSize = (s.second.getDelaySteps() == NO_DELAY)
                ? s.second.getSrcNeuronGroup()->getNumNeurons()
                : s.second.getSrcNeuronGroup()->getNumNeurons() * s.second.getSrcNeuronGroup()->getNumDelaySlots();
        const auto wuPreVars = wu->getPreVars();
        for(size_t i = 0; i < wuPreVars.size(); i++) {
            const bool autoInitialized = !s.second.getWUPreVarInitialisers()[i].getSnippet()->getCode().empty();
            mem += genVariable(backend, definitionsVar, definitionsFunc, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                            runnerPushFunc, runnerPullFunc, wuPreVars[i].type, wuPreVars[i].name + s.second.getName(),
                            s.second.getWUPreVarLocation(i), autoInitialized, preSize, synapseGroupStatePushPullFunctions);
        }

        // Postsynaptic W.U.M. variables
        const size_t postSize = (s.second.getBackPropDelaySteps() == NO_DELAY)
                ? s.second.getTrgNeuronGroup()->getNumNeurons()
                : s.second.getTrgNeuronGroup()->getNumNeurons() * s.second.getTrgNeuronGroup()->getNumDelaySlots();
        const auto wuPostVars = wu->getPostVars();
        for(size_t i = 0; i < wuPostVars.size(); i++) {
            const bool autoInitialized = !s.second.getWUPostVarInitialisers()[i].getSnippet()->getCode().empty();
            mem += genVariable(backend, definitionsVar, definitionsFunc, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                            runnerPushFunc, runnerPullFunc, wuPostVars[i].type, wuPostVars[i].name + s.second.getName(),
                            s.second.getWUPostVarLocation(i), autoInitialized, postSize, synapseGroupStatePushPullFunctions);
        }

        // If this synapse group's postsynaptic models hasn't been merged (which makes pulling them somewhat ambiguous)
        // **NOTE** we generated initialisation and declaration code earlier - here we just generate push and pull as we want this per-synapse group
        if(!s.second.isPSModelMerged()) {
            // Add code to push and pull inSyn
            genVarPushPullScope(definitionsFunc, runnerPushFunc, runnerPullFunc, s.second.getInSynLocation(),
                                backend.isAutomaticCopyEnabled(), "inSyn" + s.second.getName(), synapseGroupStatePushPullFunctions,
                [&]()
                {
                    backend.genVariablePushPull(runnerPushFunc, runnerPullFunc, model.getPrecision(), "inSyn" + s.second.getName(), s.second.getInSynLocation(),
                                                true, s.second.getTrgNeuronGroup()->getNumNeurons());
                });

            // If this synapse group has individual postsynaptic model variables
            if (s.second.getMatrixType() & SynapseMatrixWeight::INDIVIDUAL_PSM) {
                const auto psmVars = psm->getVars();
                for(size_t i = 0; i < psmVars.size(); i++) {
                    const bool autoInitialized = !s.second.getPSVarInitialisers()[i].getSnippet()->getCode().empty();
                    genVarPushPullScope(definitionsFunc, runnerPushFunc, runnerPullFunc, s.second.getPSVarLocation(i),
                                        backend.isAutomaticCopyEnabled(), psmVars[i].name + s.second.getName(), synapseGroupStatePushPullFunctions,
                        [&]()
                        {
                            backend.genVariablePushPull(runnerPushFunc, runnerPullFunc, psmVars[i].type, psmVars[i].name + s.second.getName(), s.second.getPSVarLocation(i),
                                                        autoInitialized, s.second.getTrgNeuronGroup()->getNumNeurons());
                        });
                }
            }
        }

        // Add helper function to push and pull entire synapse group state
        if(!backend.isAutomaticCopyEnabled()) {
            genStatePushPull(definitionsFunc, runnerPushFunc, runnerPullFunc, 
                             s.second.getName(), backend.shouldGenerateEmptyStatePushPull(), 
                             synapseGroupStatePushPullFunctions, statePushPullFunctions);
        }

        const auto psmExtraGlobalParams = psm->getExtraGlobalParams();
        for(size_t i = 0; i < psmExtraGlobalParams.size(); i++) {
            genExtraGlobalParam(backend, definitionsVar, definitionsFunc, runnerVarDecl, runnerExtraGlobalParamFunc,
                                mergedStructData, psmExtraGlobalParams[i].type, psmExtraGlobalParams[i].name + s.second.getName(),
                                true, s.second.getPSExtraGlobalParamLocation(i));
        }

        const auto wuExtraGlobalParams = wu->getExtraGlobalParams();
        for(size_t i = 0; i < wuExtraGlobalParams.size(); i++) {
            genExtraGlobalParam(backend, definitionsVar, definitionsFunc, runnerVarDecl, runnerExtraGlobalParamFunc,
                                mergedStructData, wuExtraGlobalParams[i].type, wuExtraGlobalParams[i].name + s.second.getName(),
                                true, s.second.getWUExtraGlobalParamLocation(i));
        }

        const auto sparseConnExtraGlobalParams = s.second.getConnectivityInitialiser().getSnippet()->getExtraGlobalParams();
        for(size_t i = 0; i < sparseConnExtraGlobalParams.size(); i++) {
            genExtraGlobalParam(backend, definitionsVar, definitionsFunc, runnerVarDecl, runnerExtraGlobalParamFunc,
                                mergedStructData, sparseConnExtraGlobalParams[i].type, sparseConnExtraGlobalParams[i].name + s.second.getName(),
                                s.second.getConnectivityInitialiser().getSnippet()->getHostInitCode().empty(),
                                s.second.getSparseConnectivityExtraGlobalParamLocation(i));
        }
    }
    allVarStreams << std::endl;

    // End extern C block around variable declarations
    runnerVarDecl << "}  // extern \"C\"" << std::endl;
 

    // Write variable declarations to runner
    runner << runnerVarDeclStream.str();

    // Write extra global parameter functions to runner
    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << "// extra global params" << std::endl;
    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << runnerExtraGlobalParamFuncStream.str();
    runner << std::endl;

    // Write push function declarations to runner
    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << "// copying things to device" << std::endl;
    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << runnerPushFuncStream.str();
    runner << std::endl;

    // Write pull function declarations to runner
    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << "// copying things from device" << std::endl;
    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << runnerPullFuncStream.str();
    runner << std::endl;

    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << "// helper getter functions" << std::endl;
    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << runnerGetterFuncStream.str();
    runner << std::endl;

    if(!backend.isAutomaticCopyEnabled()) {
        // ---------------------------------------------------------------------
        // Function for copying all state to device
        runner << "void copyStateToDevice(bool uninitialisedOnly)";
        {
            CodeStream::Scope b(runner);
            for(const auto &g : statePushPullFunctions) {
                runner << "push" << g << "StateToDevice(uninitialisedOnly);" << std::endl;
            }
        }
        runner << std::endl;

        // ---------------------------------------------------------------------
        // Function for copying all connectivity to device
        runner << "void copyConnectivityToDevice(bool uninitialisedOnly)";
        {
            CodeStream::Scope b(runner);
            for(const auto &func : connectivityPushPullFunctions) {
                runner << "push" << func << "ToDevice(uninitialisedOnly);" << std::endl;
            }
        }
        runner << std::endl;

        // ---------------------------------------------------------------------
        // Function for copying all state from device
        runner << "void copyStateFromDevice()";
        {
            CodeStream::Scope b(runner);
            for(const auto &g : statePushPullFunctions) {
                runner << "pull" << g << "StateFromDevice();" << std::endl;
            }
        }
        runner << std::endl;

        // ---------------------------------------------------------------------
        // Function for copying all current spikes from device
        runner << "void copyCurrentSpikesFromDevice()";
        {
            CodeStream::Scope b(runner);
            for(const auto &func : currentSpikePullFunctions) {
                runner << "pull" << func << "FromDevice();" << std::endl;
            }
        }
        runner << std::endl;

        // ---------------------------------------------------------------------
        // Function for copying all current spikes events from device
        runner << "void copyCurrentSpikeEventsFromDevice()";
        {
            CodeStream::Scope b(runner);
            for(const auto &func : currentSpikeEventPullFunctions) {
                runner << "pull" << func << "FromDevice();" << std::endl;
            }
        }
        runner << std::endl;
    }

    // ---------------------------------------------------------------------
    // Function for setting the CUDA device and the host's global variables.
    // Also estimates memory usage on device ...
    runner << "void allocateMem()";
    {
        CodeStream::Scope b(runner);

        // Generate preamble -this is the first bit of generated code called by user simulations
        // so global initialisation is often performed here
        backend.genAllocateMemPreamble(runner, modelMerged);

        // Write variable allocations to runner
        runner << runnerVarAllocStream.str();

        // Write merged struct allocations to runner
        runner << runnerMergedStructAllocStream.str();
    }
    runner << std::endl;

    // ------------------------------------------------------------------------
    // Function to free all global memory structures
    runner << "void freeMem()";
    {
        CodeStream::Scope b(runner);

        // Write variable frees to runner
        runner << runnerVarFreeStream.str();
    }
    runner << std::endl;

    // ------------------------------------------------------------------------
    // Function to return amount of free device memory in bytes
    runner << "size_t getFreeDeviceMemBytes()";
    {
        CodeStream::Scope b(runner);

        // Generate code to return free memory
        backend.genReturnFreeDeviceMemoryBytes(runner);
    }
    runner << std::endl;

    // ------------------------------------------------------------------------
    // Function to free all global memory structures
    runner << "void stepTime()";
    {
        CodeStream::Scope b(runner);

        // Update synaptic state
        runner << "updateSynapses(t);" << std::endl;

        // Generate code to advance host-side spike queues
   
        for(const auto &n : model.getNeuronGroups()) {
            if (n.second.isDelayRequired()) {
                runner << "spkQuePtr" << n.first << " = (spkQuePtr" << n.first << " + 1) % " << n.second.getNumDelaySlots() << ";" << std::endl;
            }
        }

        // Update neuronal state
        runner << "updateNeurons(t);" << std::endl;

        // Generate code to advance host side dendritic delay buffers
        for(const auto &n : model.getNeuronGroups()) {
            // Loop through incoming synaptic populations
            for(const auto &m : n.second.getMergedInSyn()) {
                const auto *sg = m.first;
                if(sg->isDendriticDelayRequired()) {
                    runner << "denDelayPtr" << sg->getPSModelTargetName() << " = (denDelayPtr" << sg->getPSModelTargetName() << " + 1) % " << sg->getMaxDendriticDelayTimesteps() << ";" << std::endl;
                }
            }
        }
        // Advance time
        runner << "iT++;" << std::endl;
        runner << "t = iT*DT;" << std::endl;

        // Write step time finalize logic to runner
        runner << runnerStepTimeFinaliseStream.str();
    }
    runner << std::endl;

    // Write variable and function definitions to header
    definitions << definitionsVarStream.str();
    definitions << definitionsFuncStream.str();
    definitionsInternal << definitionsInternalVarStream.str();
    definitionsInternal << definitionsInternalFuncStream.str();

    // ---------------------------------------------------------------------
    // Function definitions
    definitions << "// Runner functions" << std::endl;
    if(!backend.isAutomaticCopyEnabled()) {
        definitions << "EXPORT_FUNC void copyStateToDevice(bool uninitialisedOnly = false);" << std::endl;
        definitions << "EXPORT_FUNC void copyConnectivityToDevice(bool uninitialisedOnly = false);" << std::endl;
        definitions << "EXPORT_FUNC void copyStateFromDevice();" << std::endl;
        definitions << "EXPORT_FUNC void copyCurrentSpikesFromDevice();" << std::endl;
        definitions << "EXPORT_FUNC void copyCurrentSpikeEventsFromDevice();" << std::endl;
    }
    definitions << "EXPORT_FUNC void allocateMem();" << std::endl;
    definitions << "EXPORT_FUNC void freeMem();" << std::endl;
    definitions << "EXPORT_FUNC size_t getFreeDeviceMemBytes();" << std::endl;
    definitions << "EXPORT_FUNC void stepTime();" << std::endl;
    definitions << std::endl;
    definitions << "// Functions generated by backend" << std::endl;
    definitions << "EXPORT_FUNC void updateNeurons(" << model.getTimePrecision() << " t);" << std::endl;
    definitions << "EXPORT_FUNC void updateSynapses(" << model.getTimePrecision() << " t);" << std::endl;
    definitions << "EXPORT_FUNC void initialize();" << std::endl;
    definitions << "EXPORT_FUNC void initializeSparse();" << std::endl;

#ifdef MPI_ENABLE
    definitions << "// MPI functions" << std::endl;
    definitions << "EXPORT_FUNC void generateMPI();" << std::endl;
#endif

    // End extern C block around definitions
    definitions << "}  // extern \"C\"" << std::endl;
    definitionsInternal << "}  // extern \"C\"" << std::endl;

    return mem;
}
