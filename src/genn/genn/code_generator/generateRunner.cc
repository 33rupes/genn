#include "code_generator/generateRunner.h"

// Standard C++ includes
#include <fstream>
#include <numeric>
#include <random>
#include <set>
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
#include "code_generator/modelSpecMerged.h"

using namespace CodeGenerator;

//--------------------------------------------------------------------------
// Anonymous namespace
//--------------------------------------------------------------------------
namespace
{
void genHostScalar(CodeStream &definitionsVar, CodeStream &runnerVarDecl, const std::string &type, const std::string &name, const std::string &value)
{
    definitionsVar << "EXPORT_VAR " << type << " " << name << ";" << std::endl;
    runnerVarDecl << type << " " << name << " = " << value << ";" << std::endl;
}
//--------------------------------------------------------------------------
void genGlobalHostRNG(CodeStream &definitionsVar, CodeStream &runnerVarDecl,
                      CodeStream &runnerVarAlloc, unsigned int seed)
{
    definitionsVar << "EXPORT_VAR " << "std::mt19937 hostRNG;" << std::endl;
    runnerVarDecl << "std::mt19937 hostRNG;" << std::endl;

    // If no seed is specified, use system randomness to generate seed sequence
    CodeStream::Scope b(runnerVarAlloc);
    if(seed == 0) {
        runnerVarAlloc << "uint32_t seedData[std::mt19937::state_size];" << std::endl;
        runnerVarAlloc << "std::random_device seedSource;" << std::endl;
        runnerVarAlloc << "for(int i = 0; i < std::mt19937::state_size; i++)";
        {
            CodeStream::Scope b(runnerVarAlloc);
            runnerVarAlloc << "seedData[i] = seedSource();" << std::endl;
        }
        runnerVarAlloc << "std::seed_seq seeds(std::begin(seedData), std::end(seedData));" << std::endl;
    }
    // Otherwise, create a seed sequence from model seed
    // **NOTE** this is a terrible idea see http://www.pcg-random.org/posts/cpp-seeding-surprises.html
    else {
        runnerVarAlloc << "std::seed_seq seeds{" << seed << "};" << std::endl;
    }

    // Seed RNG from seed sequence
    runnerVarAlloc << "hostRNG.seed(seeds);" << std::endl;
}
//-------------------------------------------------------------------------
template<typename G>
void genPushAllGroupState(CodeStream &os, const std::vector<G> &mergedRunnerGroups)
{
    for(const auto &m : mergedRunnerGroups) {
        if(m.hasPushStateFunction()) {
            os << "for(unsigned int i = 0; i < " << m.getGroups().size() << "; i++)";
            {
                CodeStream::Scope b(os);
                os << "push" << m.name << "Group" << m.getIndex() << "StateToDevice(i, uninitialisedOnly);" << std::endl;
            }
        }
    }
}
//-------------------------------------------------------------------------
template<typename G>
void genExtraGlobalParamTargets(const BackendBase &backend, CodeStream &os, const G &runnerGroup, const ModelSpecMerged &modelMerged)
{
    // Loop through fields
    for(const auto &f : runnerGroup.getFields()) {
        // If this is pointer field
        if(std::holds_alternative<typename G::PointerField>(std::get<2>(f))) {
            const auto pointerField = std::get<typename G::PointerField>(std::get<2>(f));

            // If field doesn't have a count i.e. it's allocated dynamically
            const std::string &count = std::get<2>(pointerField);
            const unsigned int flags = std::get<3>(pointerField);
            if(((flags & G::POINTER_FIELD_NO_UPDATE) == 0) && count.empty()) {
                // Create three sub-streams to generate seperate data structure into
                std::ostringstream fieldPushFunctionStart;
                std::ostringstream fieldPushFunctionEnd;
                std::ostringstream fieldPushFunction;

                // Start declaration of three data structures
                fieldPushFunctionStart << "const unsigned int start" << std::get<1>(f) << G::name << "Group" << runnerGroup.getIndex() << "[]{";
                fieldPushFunctionEnd << "const unsigned int end" << std::get<1>(f) << G::name << "Group" << runnerGroup.getIndex() << "[]{";
                fieldPushFunction << "const std::function<void(" << backend.getMergedGroupFieldHostType(std::get<0>(f)) << ")> update" << std::get<1>(f) << G::name << "Group" << runnerGroup.getIndex() << "MergedGroups[]{";
               
                // Loop through groups
                size_t numPushFunctions = 0;
                for(size_t i = 0; i < runnerGroup.getGroups().size(); i++) {
                    // Write index of current group's starting push function
                    fieldPushFunctionStart << numPushFunctions << ", ";

                    // Assemble field name
                    // **YUCK** no need for this to be a single string
                    const std::string fieldName = "merged" + G::name + "Group" + std::to_string(runnerGroup.getIndex()) + "[" + std::to_string(i) + "]." + backend.getDeviceVarPrefix() + std::get<1>(f);

                    // Get targets in merged runtime groups
                    // **TODO** rename
                    const auto &targets = modelMerged.getMergedEGPDestinations(fieldName);
                    assert(!targets.empty());

                    // Loop through targets and bind group index to push function
                    for(const auto &t : targets) {
                        
                        fieldPushFunction << "std::bind(&pushMerged" << t.first << t.second.mergedGroupIndex << t.second.fieldName << "ToDevice, ";
                        fieldPushFunction << t.second.groupIndex << ", std::placeholders::_1),";
                        numPushFunctions++;
                    }

                    // Write index of current group's ending push function
                    fieldPushFunctionEnd << numPushFunctions << ", ";
                }

                // End all three arrays
                fieldPushFunctionStart << "};" << std::endl;
                fieldPushFunctionEnd << "};" << std::endl;
                fieldPushFunction << "};" << std::endl;
                
                // Write all to runner
                os << fieldPushFunctionStart.str() << fieldPushFunctionEnd.str() << fieldPushFunction.str() << std::endl;
            }
        }
    }
}
}   // Anonymous namespace

//--------------------------------------------------------------------------
// CodeGenerator
//--------------------------------------------------------------------------
void CodeGenerator::generateRunner(const filesystem::path &outputPath, const ModelSpecMerged &modelMerged, 
                                   const BackendBase &backend, const std::string &suffix)
{
    // Create output streams to write to file and wrap in CodeStreams
    std::ofstream definitionsStream((outputPath / ("definitions" + suffix + ".h")).str());
    std::ofstream definitionsInternalStream((outputPath / ("definitionsInternal" + suffix + ".h")).str());
    std::ofstream runnerStream((outputPath / ("runner" + suffix + ".cc")).str());
    CodeStream definitions(definitionsStream);
    CodeStream definitionsInternal(definitionsInternalStream);
    CodeStream runner(runnerStream);

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
    definitionsInternal << "#include \"definitions" << suffix << ".h\"" << std::endl << std::endl;
    backend.genDefinitionsInternalPreamble(definitionsInternal, modelMerged);
    
    // write DT macro
    const ModelSpecInternal &model = modelMerged.getModel();
    const unsigned int batchSize = model.getBatchSize();
    if (model.getTimePrecision() == "float") {
        definitions << "#define DT " << Utils::writePreciseString(model.getDT()) << "f" << std::endl;
    } else {
        definitions << "#define DT " << Utils::writePreciseString(model.getDT()) << std::endl;
    }

    // Write ranges of scalar and time types
    genTypeRange(definitions, model.getPrecision(), "SCALAR");
    genTypeRange(definitions, model.getTimePrecision(), "TIME");

    definitions << "// ------------------------------------------------------------------------" << std::endl;
    definitions << "// bit tool macros" << std::endl;
    definitions << "#define B(x,i) ((x) & (0x80000000 >> (i))) //!< Extract the bit at the specified position i from x" << std::endl;
    definitions << "#define setB(x,i) x= ((x) | (0x80000000 >> (i))) //!< Set the bit at the specified position i in x to 1" << std::endl;
    definitions << "#define delB(x,i) x= ((x) & (~(0x80000000 >> (i)))) //!< Set the bit at the specified position i in x to 0" << std::endl;
    definitions << std::endl;
    
    // Typedefine scalar type
    definitions << "typedef " << model.getPrecision() << " scalar;" << std::endl;

    // Write runner preamble
    runner << "#include \"definitionsInternal" << suffix << ".h\"" << std::endl << std::endl;
    runner << "#include <functional>" << std::endl << std::endl;

    // If runtime population lookup is enabled
    if(backend.getPreferences().generateRuntimePopulationLookup) {
        // Definite suitable struct
        definitions << "struct Population";
        {
            CodeStream::Scope b(definitions);
            definitions << "unsigned int mergedGroupIndex;" << std::endl;
            definitions << "unsigned int groupIndex;" << std::endl;
            definitions << "const char *groupType;" << std::endl;
        }
        definitions << ";";

        // Include a couple of additional STL headers
        runner << "#include <unordered_map>" << std::endl;
    }

    

    // Create codestreams to generate different sections of runner and definitions
    std::stringstream runnerVarDeclStream;
    std::stringstream runnerVarAllocStream;
    std::stringstream runnerMergedRuntimeStructAllocStream;
    std::stringstream runnerMergedRunnerStructAllocStream;
    std::stringstream runnerVarFreeStream;
    std::stringstream runnerPushFuncStream;
    std::stringstream runnerPullFuncStream;
    std::stringstream runnerGetterFuncStream;
    std::stringstream runnerAllocateFuncStream;
    std::stringstream runnerFreeFuncStream;
    std::stringstream runnerStepTimeFinaliseStream;
    std::stringstream definitionsVarStream;
    std::stringstream definitionsFuncStream;
    std::stringstream definitionsInternalVarStream;
    std::stringstream definitionsInternalFuncStream;
    CodeStream runnerVarDecl(runnerVarDeclStream);
    CodeStream runnerVarAlloc(runnerVarAllocStream);
    CodeStream runnerMergedRuntimeStructAlloc(runnerMergedRuntimeStructAllocStream);
    CodeStream runnerMergedRunnerStructAlloc(runnerMergedRunnerStructAllocStream);
    CodeStream runnerVarFree(runnerVarFreeStream);
    CodeStream runnerPushFunc(runnerPushFuncStream);
    CodeStream runnerPullFunc(runnerPullFuncStream);
    CodeStream runnerGetterFunc(runnerGetterFuncStream);
    CodeStream runnerAllocateFunc(runnerAllocateFuncStream);
    CodeStream runnerFreeFunc(runnerFreeFuncStream);
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

    if(model.isRecordingInUse()) {
        runnerVarDecl << "unsigned long long numRecordingTimesteps = 0;" << std::endl;
    }
    // If backend requires a global device RNG to simulate (or initialize) this model
    if(backend.isGlobalDeviceRNGRequired(modelMerged)) {
        backend.genGlobalDeviceRNG(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree);
    }
    // If backend required a global host RNG to simulate (or initialize) this model, generate a standard Mersenne Twister
    if(backend.isGlobalHostRNGRequired(modelMerged)) {
        genGlobalHostRNG(definitionsVar, runnerVarDecl, runnerVarAlloc, model.getSeed());
    }
    allVarStreams << std::endl;

    // Generate preamble for the final stage of time step
    // **NOTE** this is done now as there can be timing logic here
    backend.genStepTimeFinalisePreamble(runnerStepTimeFinalise, modelMerged);

    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    allVarStreams << "// timers" << std::endl;
    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;

    // Build set containing union of all custom update groupsnames
    std::set<std::string> customUpdateGroups;
    std::transform(model.getCustomUpdates().cbegin(), model.getCustomUpdates().cend(),
                   std::inserter(customUpdateGroups, customUpdateGroups.end()),
                   [](const ModelSpec::CustomUpdateValueType &v) { return v.second.getUpdateGroupName(); });
    std::transform(model.getCustomWUUpdates().cbegin(), model.getCustomWUUpdates().cend(),
                   std::inserter(customUpdateGroups, customUpdateGroups.end()),
                   [](const ModelSpec::CustomUpdateWUValueType &v) { return v.second.getUpdateGroupName(); });

    // Generate variables to store total elapsed time
    // **NOTE** we ALWAYS generate these so usercode doesn't require #ifdefs around timing code
    genHostScalar(definitionsVar, runnerVarDecl, "double", "initTime", "0.0");
    genHostScalar(definitionsVar, runnerVarDecl, "double", "initSparseTime", "0.0");
    genHostScalar(definitionsVar, runnerVarDecl, "double", "neuronUpdateTime", "0.0");
    genHostScalar(definitionsVar, runnerVarDecl, "double", "presynapticUpdateTime", "0.0");
    genHostScalar(definitionsVar, runnerVarDecl, "double", "postsynapticUpdateTime", "0.0");
    genHostScalar(definitionsVar, runnerVarDecl, "double", "synapseDynamicsTime", "0.0");

    // Generate variables to store total elapsed time for each custom update group
    for(const auto &g : customUpdateGroups) {
        genHostScalar(definitionsVar, runnerVarDecl, "double", "customUpdate" + g + "Time", "0.0");
        genHostScalar(definitionsVar, runnerVarDecl, "double", "customUpdate" + g + "TransposeTime", "0.0");
    }
    
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

        // Add timers for each custom update group
        for(const auto &g : customUpdateGroups) {
            backend.genTimer(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                             runnerStepTimeFinalise, "customUpdate" + g, false);
            backend.genTimer(definitionsVar, definitionsInternalVar, runnerVarDecl, runnerVarAlloc, runnerVarFree,
                             runnerStepTimeFinalise, "customUpdate" + g + "Transpose", false);
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

    // End extern C block around variable declarations
    runnerVarDecl << "}  // extern \"C\"" << std::endl;

    // Open anonymous namespace in runner around structs etc which shouldn't be exported
    runnerVarDecl << "namespace {" << std::endl;

    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    allVarStreams << "// neuron groups" << std::endl;
    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    for(const auto &m : modelMerged.getMergedNeuronRunnerGroups()) {
        m.generateRunner(backend, definitionsFunc, runnerVarDecl,
                         runnerMergedRunnerStructAlloc, runnerVarAlloc,
                         runnerVarFree, runnerPushFunc, runnerPullFunc,
                         runnerGetterFunc, runnerAllocateFunc, runnerFreeFunc, 
                         batchSize);
        genExtraGlobalParamTargets(backend, runnerVarDecl, m, modelMerged);
    }

    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    allVarStreams << "// synapse groups" << std::endl;
    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    for(const auto &m : modelMerged.getMergedSynapseRunnerGroups()) {
        m.generateRunner(backend, definitionsFunc, runnerVarDecl,
                         runnerMergedRunnerStructAlloc, runnerVarAlloc,
                         runnerVarFree, runnerPushFunc, runnerPullFunc,
                         runnerGetterFunc, runnerAllocateFunc, runnerFreeFunc, 
                         batchSize);
        genExtraGlobalParamTargets(backend, runnerVarDecl, m, modelMerged);
    }

    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    allVarStreams << "// current sources" << std::endl;
    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    for(const auto &m : modelMerged.getMergedCurrentSourceRunnerGroups()) {
        m.generateRunner(backend, definitionsFunc, runnerVarDecl,
                         runnerMergedRunnerStructAlloc, runnerVarAlloc,
                         runnerVarFree, runnerPushFunc, runnerPullFunc,
                         runnerGetterFunc, runnerAllocateFunc, runnerFreeFunc, 
                         batchSize);
        genExtraGlobalParamTargets(backend, runnerVarDecl, m, modelMerged);
    }

    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    allVarStreams << "// custom updates" << std::endl;
    allVarStreams << "// ------------------------------------------------------------------------" << std::endl;
    for(const auto &m : modelMerged.getMergedCustomUpdateRunnerGroups()) {
        m.generateRunner(backend, definitionsFunc, runnerVarDecl,
                         runnerMergedRunnerStructAlloc, runnerVarAlloc,
                         runnerVarFree, runnerPushFunc, runnerPullFunc,
                         runnerGetterFunc, runnerAllocateFunc, runnerFreeFunc, 
                         batchSize);
        genExtraGlobalParamTargets(backend, runnerVarDecl, m, modelMerged);
    }
    for(const auto &m : modelMerged.getMergedCustomUpdateWURunnerGroups()) {
        m.generateRunner(backend, definitionsFunc, runnerVarDecl,
                         runnerMergedRunnerStructAlloc, runnerVarAlloc,
                         runnerVarFree, runnerPushFunc, runnerPullFunc,
                         runnerGetterFunc, runnerAllocateFunc, runnerFreeFunc, 
                         batchSize);
        genExtraGlobalParamTargets(backend, runnerVarDecl, m, modelMerged);
    }

    runnerVarDecl << "// ------------------------------------------------------------------------" << std::endl;
    runnerVarDecl << "// merged group arrays" << std::endl;
    runnerVarDecl << "// ------------------------------------------------------------------------" << std::endl;

    definitionsInternalFunc << "// ------------------------------------------------------------------------" << std::endl;
    definitionsInternalFunc << "// copying merged group structures to device" << std::endl;
    definitionsInternalFunc << "// ------------------------------------------------------------------------" << std::endl;
    // Loop through merged synapse connectivity host initialisation groups
    for(const auto &m : modelMerged.getMergedSynapseConnectivityHostInitGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl,
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through merged synapse connectivity host init groups and generate host init code
    // **NOTE** this is done here so valid pointers get copied straight into subsequent structures and merged EGP system isn't required
    for(const auto &sg : modelMerged.getMergedSynapseConnectivityHostInitGroups()) {
        sg.generateHostInit(backend, runnerMergedRuntimeStructAlloc);
    }

    // Generate merged neuron initialisation groups
    for(const auto &m : modelMerged.getMergedNeuronInitGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl,
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Generate merged custom update initialisation groups
    for(const auto &m : modelMerged.getMergedCustomUpdateInitGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl,
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Generate merged custom dense WU update initialisation groups
    for(const auto &m : modelMerged.getMergedCustomWUUpdateDenseInitGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through merged dense synapse init groups
    for(const auto &m : modelMerged.getMergedSynapseDenseInitGroups()) {
         m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                          runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }
    
    // Loop through merged kernel synapse init groups
    for(const auto &m : modelMerged.getMergedSynapseKernelInitGroups()) {
         m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                          runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through merged synapse connectivity initialisation groups
    for(const auto &m : modelMerged.getMergedSynapseConnectivityInitGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl,
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through merged sparse synapse init groups
    for(const auto &m : modelMerged.getMergedSynapseSparseInitGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl,
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Generate merged custom sparse WU update initialisation groups
    for(const auto &m : modelMerged.getMergedCustomWUUpdateSparseInitGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through merged neuron update groups
    for(const auto &m : modelMerged.getMergedNeuronUpdateGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through merged presynaptic update groups
    for(const auto &m : modelMerged.getMergedPresynapticUpdateGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through merged postsynaptic update groups
    for(const auto &m : modelMerged.getMergedPostsynapticUpdateGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through synapse dynamics groups
    for(const auto &m : modelMerged.getMergedSynapseDynamicsGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through neuron groups whose previous spike times need resetting
    for(const auto &m : modelMerged.getMergedNeuronPrevSpikeTimeUpdateGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through neuron groups whose spike queues need resetting
    for(const auto &m : modelMerged.getMergedNeuronSpikeQueueUpdateGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through synapse groups whose dendritic delay pointers need updating
    for(const auto &m : modelMerged.getMergedSynapseDendriticDelayUpdateGroups()) {
       m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }
    
    // Loop through custom variable update groups
    for(const auto &m : modelMerged.getMergedCustomUpdateGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through custom WU variable update groups
    for(const auto &m : modelMerged.getMergedCustomUpdateWUGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through custom WU transpose variable update groups
    for(const auto &m : modelMerged.getMergedCustomUpdateTransposeWUGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through custom update host reduction groups
    for(const auto &m : modelMerged.getMergedCustomUpdateHostReductionGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // Loop through custom weight update host reduction groups
    for(const auto &m : modelMerged.getMergedCustomWUUpdateHostReductionGroups()) {
        m.generateRunner(backend, definitionsInternalFunc, runnerVarDecl, 
                         runnerMergedRuntimeStructAlloc, modelMerged.getMergedRunnerGroups());
    }

    // If runtime population lookup is enabled
    if(backend.getPreferences().generateRuntimePopulationLookup) {
        // Definite suitable struct
        runnerVarDecl << "const std::unordered_map<std::string, Population> populationLookup";
        {
            CodeStream::Scope b(runnerVarDecl);
            for(const auto &m : modelMerged.getMergedRunnerGroups().getGroups()) {
                if(std::get<3>(m.second)) {
                    runnerVarDecl << "{\"" << m.first << "\", {" << std::get<0>(m.second) << ", " << std::get<1>(m.second) << ", \"" << std::get<2>(m.second) << "\"}}, " << std::endl;
                }
            }
        } 
        runnerVarDecl << ";" << std::endl;
    }

    // End anonymous namespace in runner around structs etc which shouldn't be exported
    runnerVarDecl << "}" << std::endl;

    std::vector<std::string> currentSpikePullFunctions;
    std::vector<std::string> currentSpikeEventPullFunctions;
    std::vector<std::string> statePushPullFunctions;
    allVarStreams << std::endl;
 
    // Write pre-amble to runner
    backend.genRunnerPreamble(runner, modelMerged);

    // Write variable declarations to runner
    runner << runnerVarDeclStream.str();

    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << "// variable push functions" << std::endl;
    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << runnerPushFuncStream.str();
    runner << std::endl;

    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << "// variable pull functions" << std::endl;
    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << runnerPullFuncStream.str();
    runner << std::endl;

    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << "// variable getter functions" << std::endl;
    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << runnerGetterFuncStream.str();
    runner << std::endl;

    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << "// EGP allocation functions" << std::endl;
    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << runnerAllocateFuncStream.str();
    runner << std::endl;

    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << "// EGP free functions" << std::endl;
    runner << "// ------------------------------------------------------------------------" << std::endl;
    runner << runnerFreeFuncStream.str();
    runner << std::endl;

    if(backend.getPreferences().generateRuntimePopulationLookup) {
        runner << "Population getPopulation(const char *name)";
        {
            CodeStream::Scope b(runner);
            runner << "return populationLookup.at(name);" << std::endl;
        }
        runner << std::endl;
    }
    if(!backend.getPreferences().automaticCopy) {
        // ---------------------------------------------------------------------
        // Function for pushing all state to device
        runner << "void pushStateToDevice(bool uninitialisedOnly)";
        {
            CodeStream::Scope b(runner);
            genPushAllGroupState(runner, modelMerged.getMergedNeuronRunnerGroups());
            genPushAllGroupState(runner, modelMerged.getMergedSynapseRunnerGroups());
            genPushAllGroupState(runner, modelMerged.getMergedCurrentSourceRunnerGroups());
            genPushAllGroupState(runner, modelMerged.getMergedCustomUpdateRunnerGroups());
            genPushAllGroupState(runner, modelMerged.getMergedCustomUpdateWURunnerGroups());
        }
        runner << std::endl;

        // ---------------------------------------------------------------------
        // Function for pushing all connectivity to device
        runner << "void pushConnectivityToDevice(bool uninitialisedOnly)";
        {
            CodeStream::Scope b(runner);
            for(const auto &m : modelMerged.getMergedSynapseRunnerGroups()) {
                if(m.hasPushConnectivityFunction()) {
                    runner << "for(unsigned int i = 0; i < " << m.getGroups().size() << "; i++)";
                    {
                        CodeStream::Scope b(runner);
                        runner << "push" << m.name << "Group" << m.getIndex() << "ConnectivityToDevice(i, uninitialisedOnly);" << std::endl;
                    }
                }
            }
        }
        runner << std::endl;
    }

    // If model uses recording
    if(model.isRecordingInUse()) {
        runner << "void allocateRecordingBuffers(unsigned int timesteps)";
        {
            CodeStream::Scope b(runner);

            // Cache number of recording timesteps in global variable
            runner << "numRecordingTimesteps = timesteps;" << std::endl;

            // Loop through merged groups and generate allocation code
            for(const auto &m : modelMerged.getMergedNeuronRunnerGroups()) {
                m.genRecordingBufferAlloc(backend, runner, batchSize);
            }
        }
        runner << std::endl;

        runner << "void pullRecordingBuffersFromDevice()";
        {
            CodeStream::Scope b(runner);
            
            // Check recording buffer has been allocated
            runner << "if(numRecordingTimesteps == 0)";
            {
                CodeStream::Scope b(runner);
                runner << "throw std::runtime_error(\"Recording buffer not allocated - cannot pull from device\");" << std::endl;
            }

            // Loop through merged groups and generate allocation code
            for(const auto &m : modelMerged.getMergedNeuronRunnerGroups()) {
                m.genRecordingBufferPull(backend, runner, batchSize);
            }
        }
        runner << std::endl;
    }

    // ---------------------------------------------------------------------
    // Function for setting the device and the host's global variables.
    // Also estimates memory usage on device ...
    runner << "void allocateMem(" << backend.getAllocateMemParams(modelMerged) << ")";
    {
        CodeStream::Scope b(runner);

        // Generate preamble - this is the first bit of generated code called by user simulations
        // so global initialisation is often performed here
        backend.genAllocateMemPreamble(runner, modelMerged);

        // Write merged runner struct allocations to runner
        runner << runnerMergedRunnerStructAllocStream.str();

        // Write variable allocations to runner
        runner << runnerVarAllocStream.str();

        // Write merged runtime struct allocations to runner
        runner << runnerMergedRuntimeStructAllocStream.str();
    }
    runner << std::endl;

    // ------------------------------------------------------------------------
    // Function to free all global memory structures
    runner << "void freeMem()";
    {
        CodeStream::Scope b(runner);

        // Generate backend-specific preamble
        backend.genFreeMemPreamble(runner, modelMerged);

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
        for(const auto &m : modelMerged.getMergedNeuronRunnerGroups()) {
            m.genSpikeQueuePtrUpdate(runner);
        }

        // Update neuronal state
        runner << "updateNeurons(t";
        if(model.isRecordingInUse()) {
            runner << ", (unsigned int)(iT % numRecordingTimesteps)";
        }
        runner << "); " << std::endl;

        // Generate code to advance host side dendritic delay buffers
        for(const auto &m : modelMerged.getMergedSynapseRunnerGroups()) {
            m.genDenDelayPtrUpdate(runner);
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
    if(backend.getPreferences().generateRuntimePopulationLookup) {
        definitions << "EXPORT_FUNC Population getPopulation(const char *name);" << std::endl;
    }
    if(!backend.getPreferences().automaticCopy) {
        definitions << "EXPORT_FUNC void pushStateToDevice(bool uninitialisedOnly = false);" << std::endl;
        definitions << "EXPORT_FUNC void pushConnectivityToDevice(bool uninitialisedOnly = false);" << std::endl;
    }

    if(model.isRecordingInUse()) {
        definitions << "EXPORT_FUNC void allocateRecordingBuffers(unsigned int timesteps);" << std::endl;
        definitions << "EXPORT_FUNC void pullRecordingBuffersFromDevice();" << std::endl;
    }
    definitions << "EXPORT_FUNC void allocateMem(" << backend.getAllocateMemParams(modelMerged) << ");" << std::endl;
    definitions << "EXPORT_FUNC void freeMem();" << std::endl;
    definitions << "EXPORT_FUNC size_t getFreeDeviceMemBytes();" << std::endl;
    definitions << "EXPORT_FUNC void stepTime();" << std::endl;
    definitions << std::endl;
    definitions << "// Functions generated by backend" << std::endl;
    definitions << "EXPORT_FUNC void updateNeurons(" << model.getTimePrecision() << " t";
    if(model.isRecordingInUse()) {
        definitions << ", unsigned int recordingTimestep";
    }
    definitions << "); " << std::endl;
    definitions << "EXPORT_FUNC void updateSynapses(" << model.getTimePrecision() << " t);" << std::endl;
    definitions << "EXPORT_FUNC void initialize();" << std::endl;
    definitions << "EXPORT_FUNC void initializeSparse();" << std::endl;
    
    // Generate function definitions for each custom update
    for(const auto &g : customUpdateGroups) {
        definitions << "EXPORT_FUNC void update" << g << "();" << std::endl;
    }
#ifdef MPI_ENABLE
    definitions << "// MPI functions" << std::endl;
    definitions << "EXPORT_FUNC void generateMPI();" << std::endl;
#endif

    // End extern C block around definitions
    definitions << "}  // extern \"C\"" << std::endl;
    definitionsInternal << "}  // extern \"C\"" << std::endl;
}
