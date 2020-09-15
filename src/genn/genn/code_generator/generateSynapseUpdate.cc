#include "code_generator/generateSynapseUpdate.h"

// Standard C++ includes
#include <string>

// GeNN code generator includes
#include "code_generator/codeGenUtils.h"
#include "code_generator/codeStream.h"
#include "code_generator/groupMerged.h"
#include "code_generator/modelSpecMerged.h"
#include "code_generator/substitutions.h"
#include "code_generator/teeStream.h"

//--------------------------------------------------------------------------
// Anonymous namespace
//--------------------------------------------------------------------------
namespace
{
void applySynapseSubstitutions(CodeGenerator::CodeStream &os, std::string code, const std::string &errorContext,
                               const CodeGenerator::SynapseGroupMergedBase &sg, const CodeGenerator::Substitutions &baseSubs,
                               const CodeGenerator::ModelSpecMerged &modelMerged, const bool backendSupportsNamespace)
{
    const ModelSpecInternal &model = modelMerged.getModel();
    const auto *wu = sg.getArchetype().getWUModel();

    CodeGenerator::Substitutions synapseSubs(&baseSubs);

    // Substitute parameter and derived parameter names
    synapseSubs.addParamValueSubstitution(wu->getParamNames(), sg.getArchetype().getWUParams(),
                                          [&sg](size_t i) { return sg.isWUParamHeterogeneous(i);  },
                                          "", "group->");
    synapseSubs.addVarValueSubstitution(wu->getDerivedParams(), sg.getArchetype().getWUDerivedParams(),
                                        [&sg](size_t i) { return sg.isWUDerivedParamHeterogeneous(i);  },
                                        "", "group->");
    synapseSubs.addVarNameSubstitution(wu->getExtraGlobalParams(), "", "group->");

    // Substitute names of pre and postsynaptic weight update variables
    const std::string delayedPreIdx = (sg.getArchetype().getDelaySteps() == NO_DELAY) ? synapseSubs["id_pre"] : "preReadDelayOffset + " + baseSubs["id_pre"];
    synapseSubs.addVarNameSubstitution(wu->getPreVars(), "", "group->",
                                       "[" + delayedPreIdx + "]");

    const std::string delayedPostIdx = (sg.getArchetype().getBackPropDelaySteps() == NO_DELAY) ? synapseSubs["id_post"] : "postReadDelayOffset + " + baseSubs["id_post"];
    synapseSubs.addVarNameSubstitution(wu->getPostVars(), "", "group->",
                                       "[" + delayedPostIdx + "]");

    // If weights are individual, substitute variables for values stored in global memory
    if (sg.getArchetype().getMatrixType() & SynapseMatrixWeight::INDIVIDUAL) {
        synapseSubs.addVarNameSubstitution(wu->getVars(), "", "group->",
                                           "[" + synapseSubs["id_syn"] + "]");
    }
    // Otherwise, if weights are procedual
    else if (sg.getArchetype().getMatrixType() & SynapseMatrixWeight::PROCEDURAL) {
        const auto vars = wu->getVars();

        for(size_t k = 0; k < vars.size(); k++) {
            const auto &varInit = sg.getArchetype().getWUVarInitialisers().at(k);

            // If this variable has any initialisation code
            if(!varInit.getSnippet()->getCode().empty()) {
                // Configure variable substitutions
                CodeGenerator::Substitutions varSubs(&synapseSubs);
                varSubs.addVarSubstitution("value", "l" + vars[k].name);
                varSubs.addParamValueSubstitution(varInit.getSnippet()->getParamNames(), varInit.getParams(),
                                                  [k, &sg](size_t p) { return sg.isWUVarInitParamHeterogeneous(k, p); },
                                                  "", "group->", vars[k].name);
                varSubs.addVarValueSubstitution(varInit.getSnippet()->getDerivedParams(), varInit.getDerivedParams(),
                                                [k, &sg](size_t p) { return sg.isWUVarInitDerivedParamHeterogeneous(k, p); },
                                                "", "group->", vars[k].name);
                varSubs.addVarNameSubstitution(varInit.getSnippet()->getExtraGlobalParams(),
                                               "", "group->", vars[k].name);

                // Generate variable initialization code
                std::string code = varInit.getSnippet()->getCode();
                varSubs.applyCheckUnreplaced(code, "initVar : merged" + vars[k].name + std::to_string(sg.getIndex()));

                // Declare local variable
                os << vars[k].type << " " << "l" << vars[k].name << ";" << std::endl;

                // Insert code to initialize variable into scope
                {
                    CodeGenerator::CodeStream::Scope b(os);
                    os << code << std::endl;;
                }
            }
        }

        // Substitute variables for newly-declared local variables
        synapseSubs.addVarNameSubstitution(vars, "", "l");
    }
    // Otherwise, if weights are kernels
    else if(sg.getArchetype().getMatrixType() & SynapseMatrixWeight::KERNEL) {
        // **HACK** this belongs in backend
        const auto &kernelSize = sg.getArchetype().getKernelSize();
        if(kernelSize.size() == 1) {
            assert(false);
        }
        else if(kernelSize.size() == 2) {
            // Generate texture coordinates
            const std::string texCoord = synapseSubs["id_kernel_1"] + ", " + synapseSubs["id_kernel_0"];
            
            // Substitute vars for calls to tex2D
            for(const auto &v : wu->getVars()) {
                synapseSubs.addVarSubstitution(v.name, "tex2D<" + v.type + ">(group->" + v.name + ", " + texCoord + ")");
            }
        }
        else if(kernelSize.size() == 3) {
            // Generate texture coordinates
            const std::string texCoord = synapseSubs["id_kernel_2"] + ", " + synapseSubs["id_kernel_1"] + ", " + synapseSubs["id_kernel_0"];

            // Substitute vars for calls to tex3D
            for(const auto &v : wu->getVars()) {
                synapseSubs.addVarSubstitution(v.name, "tex3D<" + v.type + ">(group->" + v.name + ", " + texCoord + ")");
            }
        }
        else {
            // Generate first two dimensions of texture coordinate
            const std::string texCoord2D = synapseSubs["id_kernel_1"] + ", " + synapseSubs["id_kernel_0"];

            os << "const unsigned int kernelDepth = ";
            const auto &kernelSize = sg.getArchetype().getKernelSize();
            for(size_t i = 2; i < kernelSize.size(); i++) {
                os << "(" << synapseSubs["id_kernel_" + std::to_string(i)];
                // Loop through remainind dimensions of kernel
                for(size_t j = i + 1; j < kernelSize.size(); j++) {
                    // If kernel size if heterogeneous in this dimension, multiply by value from group structure
                    if(sg.isKernelSizeHeterogeneous(j)) {
                        os << " * group->kernelSize" << j;
                    }
                    // Otherwise, multiply by literal
                    else {
                        os << " * " << kernelSize.at(j);
                    }
                }
                os << ")";
                
                // If this isn't the last dimension, add +
                if(i != (kernelSize.size() - 1)) {
                    os << " + ";
                }
            }
            os << ";" << std::endl;

            // Substitute vars for calls to tex3D
            for(const auto &v : wu->getVars()) {
                synapseSubs.addVarSubstitution(v.name, "tex3D<" + v.type + ">(group->" + v.name + ", kernelDepth, " + texCoord2D + ")");
            }
        }
        //const auto dimensionality = Utils::getKernelDimensionality(sg.getArchetype().getKernelSize());

        // Loop through kernel dimensions to calculate array index
        /*os << "const unsigned int kernelInd = ";
        const auto &kernelSize = sg.getArchetype().getKernelSize();
        for(size_t i = 0; i < kernelSize.size(); i++) {
            os << "(" << synapseSubs["id_kernel_" + std::to_string(i)];
            // Loop through remainind dimensions of kernel
            for(size_t j = i + 1; j < kernelSize.size(); j++) {
                // If kernel size if heterogeneous in this dimension, multiply by value from group structure
                if(sg.isKernelSizeHeterogeneous(j)) {
                    os << " * group->kernelSize" << j;
                }
                // Otherwise, multiply by literal
                else {
                    os << " * " << kernelSize.at(j);
                }
            }
            os << ")";

            // If this isn't the last dimension, add +
            if(i != (kernelSize.size() - 1)) {
                os << " + ";
            }
        }
        os << ";" << std::endl;

        // Use kernel index to index into variables
        synapseSubs.addVarNameSubstitution(wu->getVars(), "", "group->", "[kernelInd]");*/
    }
    // Otherwise, substitute variables for constant values
    else {
        synapseSubs.addVarValueSubstitution(wu->getVars(), sg.getArchetype().getWUConstInitVals(),
                                            [&sg](size_t v) { return sg.isWUGlobalVarHeterogeneous(v); },
                                            "", "group->");
    }

    // Make presynaptic neuron substitutions
    const std::string axonalDelayOffset = Utils::writePreciseString(model.getDT() * (double)(sg.getArchetype().getDelaySteps() + 1u)) + " + ";
    const std::string preOffset = sg.getArchetype().getSrcNeuronGroup()->isDelayRequired() ? "preReadDelayOffset + " : "";
    neuronSubstitutionsInSynapticCode(synapseSubs, sg.getArchetype().getSrcNeuronGroup(),
                                      preOffset, axonalDelayOffset, synapseSubs["id_pre"], "_pre", "Pre", "", "",
                                      [&sg](size_t paramIndex) { return sg.isSrcNeuronParamHeterogeneous(paramIndex); },
                                      [&sg](size_t derivedParamIndex) { return sg.isSrcNeuronDerivedParamHeterogeneous(derivedParamIndex); });


    // Make postsynaptic neuron substitutions
    const std::string backPropDelayMs = Utils::writePreciseString(model.getDT() * (double)(sg.getArchetype().getBackPropDelaySteps() + 1u)) + " + ";
    const std::string postOffset = sg.getArchetype().getTrgNeuronGroup()->isDelayRequired() ? "postReadDelayOffset + " : "";
    neuronSubstitutionsInSynapticCode(synapseSubs, sg.getArchetype().getTrgNeuronGroup(),
                                      postOffset, backPropDelayMs, synapseSubs["id_post"], "_post", "Post", "", "",
                                      [&sg](size_t paramIndex) { return sg.isTrgNeuronParamHeterogeneous(paramIndex); },
                                      [&sg](size_t derivedParamIndex) { return sg.isTrgNeuronDerivedParamHeterogeneous(derivedParamIndex); });

    // If the backend does not support namespaces then we substitute all support code functions with namepsace as prefix
    if (!backendSupportsNamespace) {
        if (!wu->getSimSupportCode().empty()) {
            code = CodeGenerator::disambiguateNamespaceFunction(wu->getSimSupportCode(), code, modelMerged.getPresynapticUpdateSupportCodeNamespace(wu->getSimSupportCode()));
        }
        if (!wu->getLearnPostSupportCode().empty()) {
            code = CodeGenerator::disambiguateNamespaceFunction(wu->getLearnPostSupportCode(), code, modelMerged.getPostsynapticUpdateSupportCodeNamespace(wu->getLearnPostSupportCode()));
        }
        if (!wu->getSynapseDynamicsSuppportCode().empty()) {
            code = CodeGenerator::disambiguateNamespaceFunction(wu->getSynapseDynamicsSuppportCode(), code, modelMerged.getSynapseDynamicsSupportCodeNamespace(wu->getSynapseDynamicsSuppportCode()));
        }
    }

    synapseSubs.apply(code);
    //synapseSubs.applyCheckUnreplaced(code, errorContext + " : " + sg.getName());
    code = CodeGenerator::ensureFtype(code, model.getPrecision());
    os << code;
}
}   // Anonymous namespace

//--------------------------------------------------------------------------
// CodeGenerator
//--------------------------------------------------------------------------
void CodeGenerator::generateSynapseUpdate(CodeStream &os, BackendBase::MemorySpaces &memorySpaces,
                                          const ModelSpecMerged &modelMerged, const BackendBase &backend)
{
    os << "#include \"definitionsInternal.h\"" << std::endl;
    if (backend.supportsNamespace()) {
        os << "#include \"supportCode.h\"" << std::endl;
    }
    os << std::endl;

    // Generate functions to push merged synapse group structures
    const ModelSpecInternal &model = modelMerged.getModel();

    // Synaptic update kernels
    backend.genSynapseUpdate(os, modelMerged, memorySpaces,
        // Preamble handler
        [&modelMerged, &backend](CodeStream &os)
        {
            modelMerged.genMergedGroupPush(os, modelMerged.getMergedSynapseDendriticDelayUpdateGroups(), backend);
            modelMerged.genMergedGroupPush(os, modelMerged.getMergedPresynapticUpdateGroups(), backend);
            modelMerged.genMergedGroupPush(os, modelMerged.getMergedPostsynapticUpdateGroups(), backend);
            modelMerged.genMergedGroupPush(os, modelMerged.getMergedSynapseDynamicsGroups(), backend);
        },
        // Presynaptic weight update threshold
        [&modelMerged, &backend](CodeStream &os, const PresynapticUpdateGroupMerged &sg, Substitutions &baseSubs)
        {
            Substitutions synapseSubs(&baseSubs);

            // Make weight update model substitutions
            synapseSubs.addParamValueSubstitution(sg.getArchetype().getWUModel()->getParamNames(), sg.getArchetype().getWUParams(),
                                                  [&sg](size_t i) { return sg.isWUParamHeterogeneous(i);  },
                                                  "", "group->");
            synapseSubs.addVarValueSubstitution(sg.getArchetype().getWUModel()->getDerivedParams(), sg.getArchetype().getWUDerivedParams(),
                                                [&sg](size_t i) { return sg.isWUDerivedParamHeterogeneous(i);  },
                                                "", "group->");
            synapseSubs.addVarNameSubstitution(sg.getArchetype().getWUModel()->getExtraGlobalParams(), "", "group->");

            // Get read offset if required and substitute in presynaptic neuron properties
            const std::string offset = sg.getArchetype().getSrcNeuronGroup()->isDelayRequired() ? "preReadDelayOffset + " : "";
            neuronSubstitutionsInSynapticCode(synapseSubs, sg.getArchetype().getSrcNeuronGroup(), offset, "", baseSubs["id_pre"], "_pre", "Pre", "", "",
                                              [&sg](size_t paramIndex) { return sg.isSrcNeuronParamHeterogeneous(paramIndex); },
                                              [&sg](size_t derivedParamIndex) { return sg.isSrcNeuronDerivedParamHeterogeneous(derivedParamIndex); });
            
            const auto* wum = sg.getArchetype().getWUModel();

            // Get event threshold condition code
            std::string code = wum->getEventThresholdConditionCode();
            synapseSubs.applyCheckUnreplaced(code, "eventThresholdConditionCode");
            code = ensureFtype(code, modelMerged.getModel().getPrecision());

            if (!backend.supportsNamespace() && !wum->getSimSupportCode().empty()) {
                code = disambiguateNamespaceFunction(wum->getSimSupportCode(), code, modelMerged.getPresynapticUpdateSupportCodeNamespace(wum->getSimSupportCode()));
            }

            os << code;
        },
        // Presynaptic spike
        [&modelMerged, &backend](CodeStream &os, const PresynapticUpdateGroupMerged &sg, Substitutions &baseSubs)
        {
            applySynapseSubstitutions(os, sg.getArchetype().getWUModel()->getSimCode(), "simCode",
                                      sg, baseSubs, modelMerged, backend.supportsNamespace());
        },
        // Presynaptic spike-like event
        [&modelMerged, &backend](CodeStream &os, const PresynapticUpdateGroupMerged &sg, Substitutions &baseSubs)
        {
            applySynapseSubstitutions(os, sg.getArchetype().getWUModel()->getEventCode(), "eventCode",
                                      sg, baseSubs, modelMerged, backend.supportsNamespace());
        },
        // Procedural connectivity
        [&model](CodeStream &os, const PresynapticUpdateGroupMerged &sg, Substitutions &baseSubs)
        {
            const auto &connectInit = sg.getArchetype().getConnectivityInitialiser();

            // Add substitutions
            baseSubs.addFuncSubstitution("endRow", 0, "break");
            baseSubs.addParamValueSubstitution(connectInit.getSnippet()->getParamNames(), connectInit.getParams(),
                                               [&sg](size_t i) { return sg.isConnectivityInitParamHeterogeneous(i);  },
                                               "", "group->");
            baseSubs.addVarValueSubstitution(connectInit.getSnippet()->getDerivedParams(), connectInit.getDerivedParams(),
                                             [&sg](size_t i) { return sg.isConnectivityInitDerivedParamHeterogeneous(i);  },
                                             "", "group->");
            baseSubs.addVarNameSubstitution(connectInit.getSnippet()->getExtraGlobalParams(), "", "group->");

            // Initialise row building state variables for procedural connectivity
            genParamValVecInit(os, baseSubs, connectInit.getSnippet()->getRowBuildStateVars(),
                               "proceduralSparseConnectivity row build state var : merged" + std::to_string(sg.getIndex()));

            // Loop through synapses in row
            os << "while(true)";
            {
                CodeStream::Scope b(os);

                // Apply substitutions to row building code
                std::string pCode = connectInit.getSnippet()->getRowBuildCode();
                baseSubs.addVarNameSubstitution(connectInit.getSnippet()->getRowBuildStateVars());
                baseSubs.applyCheckUnreplaced(pCode, "proceduralSparseConnectivity : merged " + std::to_string(sg.getIndex()));
                pCode = ensureFtype(pCode, model.getPrecision());

                // Write out code
                os << pCode << std::endl;
            }
        },
        // Postsynaptic learning code
        [&modelMerged, &backend](CodeStream &os, const PostsynapticUpdateGroupMerged &sg, const Substitutions &baseSubs)
        {
            const auto *wum = sg.getArchetype().getWUModel();
            if (!wum->getLearnPostSupportCode().empty() && backend.supportsNamespace()) {
                os << "using namespace " << modelMerged.getPostsynapticUpdateSupportCodeNamespace(wum->getLearnPostSupportCode()) <<  ";" << std::endl;
            }

            applySynapseSubstitutions(os, wum->getLearnPostCode(), "learnPostCode",
                                      sg, baseSubs, modelMerged, backend.supportsNamespace());
        },
        // Synapse dynamics
        [&modelMerged, &backend](CodeStream &os, const SynapseDynamicsGroupMerged &sg, const Substitutions &baseSubs)
        {
            const auto *wum = sg.getArchetype().getWUModel();
            if (!wum->getSynapseDynamicsSuppportCode().empty() && backend.supportsNamespace()) {
                os << "using namespace " << modelMerged.getSynapseDynamicsSupportCodeNamespace(wum->getSynapseDynamicsSuppportCode()) <<  ";" << std::endl;
            }

            applySynapseSubstitutions(os, wum->getSynapseDynamicsCode(), "synapseDynamics",
                                      sg, baseSubs, modelMerged, backend.supportsNamespace());
        },
        // Push EGP handler
        [&backend, &modelMerged](CodeStream &os)
        {
            modelMerged.genScalarEGPPush(os, "PresynapticUpdate", backend);
            modelMerged.genScalarEGPPush(os, "PostsynapticUpdate", backend);
            modelMerged.genScalarEGPPush(os, "SynapseDynamics", backend);
        });
}
