#include "code_generator/runnerGroupMerged.h"


using namespace CodeGenerator;

//----------------------------------------------------------------------------
// CodeGenerator::NeuronInitGroupMerged
//----------------------------------------------------------------------------
const std::string NeuronRunnerGroupMerged::name = "NeuronRunner";
//----------------------------------------------------------------------------
NeuronRunnerGroupMerged::NeuronRunnerGroupMerged(size_t index, const std::string&, const std::string &timePrecision, const BackendBase &backend,
                                             const std::vector<std::reference_wrapper<const NeuronGroupInternal>> &groups)
:   RunnerGroupMergedBase<NeuronGroupInternal, NeuronRunnerGroupMerged>(index, groups)
{
    addField("unsigned int", "numNeurons",
              [](const NeuronGroupInternal &ng) { return std::to_string(ng.getNumNeurons()); });
    if(getArchetype().isDelayRequired()) {
        addField("unsigned int", "numDelaySlots",
                 [](const NeuronGroupInternal &ng) { return std::to_string(ng.getNumDelaySlots()); });
    }
    
    const bool delaySpikes = (getArchetype().isTrueSpikeRequired() && getArchetype().isDelayRequired());
    addField("unsigned int", "spkCnt", getArchetype().getSpikeLocation(), 0,
             delaySpikes ? "group->numDelaySlots" : "1");
    addField("unsigned int", "spk", getArchetype().getSpikeLocation(), 0,
             delaySpikes ? "group->numDelaySlots * group->numNeurons" : "group->numNeurons");
    
    if(getArchetype().isSpikeRecordingEnabled()) {
        addField("uint32_t", "recordSpk", VarLocation::HOST_DEVICE, 
                 POINTER_FIELD_MANUAL_ALLOC | POINTER_FIELD_GET);
    }

    const std::string numNeurons = "group->numNeurons";
    const std::string numNeuronsDelayed = getArchetype().isDelayRequired() ? "group->numDelaySlots * group->numNeurons" : numNeurons;
    if(getArchetype().isSpikeEventRequired()) {
        addField("unsigned int", "spkCntEvnt", getArchetype().getSpikeEventLocation(), 0,
                 getArchetype().isDelayRequired() ? "group->numDelaySlots" : "1");
        addField("unsigned int", "spkEvnt", getArchetype().getSpikeEventLocation(), 0,
                 numNeuronsDelayed);

        if(getArchetype().isSpikeEventRecordingEnabled()) {
            addField("uint32_t", "recordSpkEvent", VarLocation::HOST_DEVICE, 
                     POINTER_FIELD_MANUAL_ALLOC | POINTER_FIELD_GET);
        }
    }

    if(getArchetype().isDelayRequired()) {
        addField("unsigned int", "spkQuePtr", "0");
    }

    if(getArchetype().isSpikeTimeRequired()) {
        addField(timePrecision, "sT", getArchetype().getSpikeTimeLocation(), 0, numNeuronsDelayed);
    }
    if(getArchetype().isSpikeEventTimeRequired()) {
        addField(timePrecision, "seT", getArchetype().getSpikeTimeLocation(), 0, numNeuronsDelayed);
    }

    if(getArchetype().isPrevSpikeTimeRequired()) {
        addField(timePrecision, "prevST", getArchetype().getPrevSpikeTimeLocation(), 0, numNeuronsDelayed);
    }
    if(getArchetype().isPrevSpikeEventTimeRequired()) {
        addField(timePrecision, "prevSET", getArchetype().getPrevSpikeTimeLocation(), 0, numNeuronsDelayed);
    }

    // If this backend initialises population RNGs on device and this group requires on for simulation
    if(backend.isPopulationRNGRequired() && getArchetype().isSimRNGRequired()) {
        addField(backend.getMergedGroupSimRNGType(), "rng", VarLocation::DEVICE, 0, "group->numNeurons");
    }

    // Loop through variables
    for(const auto &var : getArchetype().getNeuronModel()->getVars()) {
        const auto *varInitSnippet = getArchetype().getVarInitialisers().at(var.name).getSnippet();
        const unsigned int autoInitialisedFlags = varInitSnippet->getCode().empty() ? 0 : POINTER_FIELD_AUTO_INITIALISED;
        const std::string count = getArchetype().isVarQueueRequired(var.name) ? numNeuronsDelayed : numNeurons;
        addField(var.type, var.name, getArchetype().getVarLocation(var.name), 
                 POINTER_FIELD_PUSH_PULL_GET | POINTER_FIELD_STATE | autoInitialisedFlags, 
                 count, getVarAccessDuplication(var.access));

        addEGPs(varInitSnippet->getExtraGlobalParams(), var.name);
    }

    // Add extra global parmeters
    addEGPs(getArchetype().getNeuronModel()->getExtraGlobalParams(), "",
            [this](const std::string &name) { return getArchetype().getExtraGlobalParamLocation(name); });
}
//----------------------------------------------------------------------------
void NeuronRunnerGroupMerged::genRecordingBufferAlloc(const BackendBase &backend, CodeStream &runner, unsigned int batchSize) const
{
    CodeStream::Scope b(runner);
    runner << "// merged neuron runner group " << getIndex() << std::endl;
    runner << "for(unsigned int g = 0; g < " << getGroups().size() << "; g++)";
    {
        CodeStream::Scope b(runner);

        // Get reference to group
        runner << "auto *group = &mergedNeuronRunnerGroup" << getIndex() << "[g]; " << std::endl;

        // Calculate number of words required for spike/spike event buffers
        if(getArchetype().isSpikeRecordingEnabled() || getArchetype().isSpikeEventRecordingEnabled()) {
            runner << "const unsigned int numWords = ((group->numNeurons + 31) / 32) * " << batchSize << " * numRecordingTimesteps;" << std::endl;
        }

        // Allocate spike array if required
        // **YUCK** maybe this should be renamed genDynamicArray
        if(getArchetype().isSpikeRecordingEnabled()) {
            CodeStream::Scope b(runner);
            backend.genFieldAllocation(runner, "uint32_t*", "recordSpk", VarLocation::HOST_DEVICE, "numWords");

            // Generate code to push updated pointer to all destinations
            genPushPointer(backend, runner, "recordSpk", "g");
        }

        // Allocate spike event array if required
        // **YUCK** maybe this should be renamed genDynamicArray
        if(getArchetype().isSpikeEventRecordingEnabled()) {
            CodeStream::Scope b(runner);
            backend.genFieldAllocation(runner, "uint32_t*", "recordSpkEvent", VarLocation::HOST_DEVICE, "numWords");

            // Generate code to push updated pointer to all destinations
            genPushPointer(backend, runner, "recordSpkEvent", "g");
        }
    }
}
//----------------------------------------------------------------------------
void NeuronRunnerGroupMerged::genRecordingBufferPull(const BackendBase &backend, CodeStream &runner, unsigned int batchSize) const
{
    CodeStream::Scope b(runner);
    runner << "// merged neuron runner group " << getIndex() << std::endl;
    runner << "for(unsigned int g = 0; g < " << getGroups().size() << "; g++)";
    {
        CodeStream::Scope b(runner);

        // Get reference to group
        runner << "auto *group = &mergedNeuronRunnerGroup" << getIndex() << "[g]; " << std::endl;

        // Calculate number of words required for spike/spike event buffers
        if(getArchetype().isSpikeRecordingEnabled() || getArchetype().isSpikeEventRecordingEnabled()) {
            runner << "const unsigned int numWords = ((group->numNeurons + 31) / 32) * " << batchSize << " * numRecordingTimesteps;" << std::endl;
        }

        // Pull spike array if required
        // **YUCK** maybe this should be renamed pullDynamicArray
        if(getArchetype().isSpikeRecordingEnabled()) {
            CodeStream::Scope b(runner);
            backend.genFieldPull(runner, "uint32_t*", "recordSpk", VarLocation::HOST_DEVICE, "numWords");
        }
        // Pull spike event array if required
        // **YUCK** maybe this should be renamed pullDynamicArray
        if(getArchetype().isSpikeEventRecordingEnabled()) {
            CodeStream::Scope b(runner);
            backend.genFieldPull(runner, "uint32_t*", "recordSpkEvent", VarLocation::HOST_DEVICE, "numWords");
        }
    }
}
//----------------------------------------------------------------------------
void NeuronRunnerGroupMerged::genSpikeQueuePtrUpdate(CodeStream &os) const
{
    // If this merged group has axonal delay
    if(getArchetype().isDelayRequired()) {
         // Loop through groups
         os << "for(unsigned int g = 0; g < " << getGroups().size() << "; g++)";
         {
             CodeStream::Scope b(os);

             // Generate update
             os << "auto *group = &mergedNeuronRunnerGroup" << getIndex() << "[g]; " << std::endl;
             os << "group->spkQuePtr = (group->spkQuePtr + 1) % group->numDelaySlots;" << std::endl;
         }
    }
}

//----------------------------------------------------------------------------
// CodeGenerator::SynapseRunnerGroupMerged
//----------------------------------------------------------------------------
const std::string SynapseRunnerGroupMerged::name = "SynapseRunner";
//----------------------------------------------------------------------------
SynapseRunnerGroupMerged::SynapseRunnerGroupMerged(size_t index, const std::string &precision, const std::string&, const BackendBase &backend,
                                                   const std::vector<std::reference_wrapper<const SynapseGroupInternal>> &groups)
:   RunnerGroupMergedBase<SynapseGroupInternal, SynapseRunnerGroupMerged>(index, groups)
{
    addField("unsigned int", "numSrcNeurons",
             [](const SynapseGroupInternal &sg) { return std::to_string(sg.getSrcNeuronGroup()->getNumNeurons()); });
    addField("unsigned int", "numTrgNeurons",
             [](const SynapseGroupInternal &sg) { return std::to_string(sg.getTrgNeuronGroup()->getNumNeurons()); });
    addField("unsigned int", "rowStride",
             [&backend](const SynapseGroupInternal &sg) { return std::to_string(backend.getSynapticMatrixRowStride(sg)); });
    addField("unsigned int", "colStride",
             [](const SynapseGroupInternal &sg) { return std::to_string(sg.getMaxSourceConnections()); });
    
    if(getArchetype().getMatrixType() & SynapseMatrixWeight::KERNEL) {
        addField("unsigned int", "kernelSizeFlattened",
                 [](const SynapseGroupInternal &sg) { return std::to_string(sg.getKernelSizeFlattened()); });
    }

    // Add pointers to connectivity data
    if(getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
        addField("unsigned int", "rowLength", getArchetype().getSparseConnectivityLocation(), 
                 POINTER_FIELD_CONNECTIVITY | POINTER_FIELD_GET, "group->numSrcNeurons", VarAccessDuplication::SHARED);
        addField(getArchetype().getSparseIndType(), "ind", getArchetype().getSparseConnectivityLocation(),
                 POINTER_FIELD_CONNECTIVITY | POINTER_FIELD_GET, "group->numSrcNeurons * group->rowStride", VarAccessDuplication::SHARED);

        // Add additional structure for postsynaptic access
        if(backend.isPostsynapticRemapRequired() && !getArchetype().getWUModel()->getLearnPostCode().empty()) {
            addField("unsigned int", "colLength", VarLocation::DEVICE, POINTER_FIELD_CONNECTIVITY | POINTER_FIELD_GET, 
                     "group->numTrgNeurons", VarAccessDuplication::SHARED);
            addField("unsigned int", "remap", VarLocation::DEVICE, POINTER_FIELD_CONNECTIVITY | POINTER_FIELD_GET, 
                     "group->numTrgNeurons * group->colStride", VarAccessDuplication::SHARED);
        }

        // Add additional structure for synapse dynamics access if required
        if(backend.isSynRemapRequired(getArchetype())) {
            addField("unsigned int", "synRemap", VarLocation::DEVICE, POINTER_FIELD_CONNECTIVITY | POINTER_FIELD_GET, 
                     "(group->numSrcNeurons * group->rowStride) + 1", VarAccessDuplication::SHARED);
        }
    }
    else if(getArchetype().getMatrixType() & SynapseMatrixConnectivity::BITMASK) {
        addField("uint32_t", "gp", getArchetype().getSparseConnectivityLocation(), POINTER_FIELD_CONNECTIVITY | POINTER_FIELD_GET, 
                 "(((group->numSrcNeurons * group->rowStride) + 31) / 32)", VarAccessDuplication::SHARED);
    }

    // Loop through EGPs
    const auto *sparseInitSnippet = getArchetype().getSparseConnectivityInitialiser().getSnippet();
    const std::string hostInitCode = sparseInitSnippet->getHostInitCode();
    for(const auto &egp : sparseInitSnippet->getExtraGlobalParams()) {
        // If EGP is a pointer, add field
        if(Utils::isTypePointer(egp.type)) {
            // If this EGP is allocated in host initialisation code, set manual allocation and no update flags
            unsigned int flags = 0;
            if(hostInitCode.find("$(allocate" + egp.name) != std::string::npos) {
                flags |= POINTER_FIELD_MANUAL_ALLOC | POINTER_FIELD_NO_UPDATE;
            }
            // If this EGP isn't already pushed in host initialisation code, set push pull get flag
            if(hostInitCode.find("$(push" + egp.name) == std::string::npos) {
                flags |= POINTER_FIELD_PUSH_PULL_GET;
            }
            addField(Utils::getUnderlyingType(egp.type), egp.name,
                     VarLocation::HOST_DEVICE, flags);
        }
    }

    addEGPs(getArchetype().getToeplitzConnectivityInitialiser().getSnippet()->getExtraGlobalParams());

  
    // If postsynaptic output model either isn't fused or archetype is the fuse source
    if(!getArchetype().isPostOutputModelFused() || getArchetype().isPostOutputModelFuseSource()) {
        // Add pointer to insyn
        addField(precision, "inSyn", getArchetype().getInSynLocation(), 
                 POINTER_FIELD_PUSH_PULL_GET, "group->numTrgNeurons");

        // Add pointer to dendritic delay buffer if required
        if(getArchetype().isDendriticDelayRequired()) {
            addField("unsigned int", "maxDendriticDelayTimesteps",
                     [this](const SynapseGroupInternal &sg) 
                     { 
                          return std::to_string(sg.getMaxDendriticDelayTimesteps());
                     });

            addField(precision, "denDelay", getArchetype().getDendriticDelayLocation(), 0,
                     "group->numTrgNeurons * group->maxDendriticDelayTimesteps");
            addField("unsigned int", "denDelayPtr", "0");
        }

        // Add PSM extra global parmeters
        addEGPs(getArchetype().getPSModel()->getExtraGlobalParams(), "",
                [this](const std::string &name) { return getArchetype().getPSExtraGlobalParamLocation(name); });

        // Loop through PSM variables
        for(const auto &var : getArchetype().getPSModel()->getVars()) {
            const auto *varInitSnippet = getArchetype().getPSVarInitialisers().at(var.name).getSnippet();
            unsigned int flags = 0;
            if(!varInitSnippet->getCode().empty()) {
                flags |= POINTER_FIELD_AUTO_INITIALISED;
            }
            if(!getArchetype().isPostOutputModelFused()) {
                flags |= (POINTER_FIELD_PUSH_PULL_GET | POINTER_FIELD_STATE);
            }
            addField(var.type, var.name, getArchetype().getPSVarLocation(var.name), 
                     flags, "group->numTrgNeurons", getVarAccessDuplication(var.access));
            addEGPs(varInitSnippet->getExtraGlobalParams(), var.name);
        }
    }

    // If presynaptic output is required and presynaptic output either isn't  
    // fused or archetype is the fuse source, add pointer to revInSyn    
    if(getArchetype().isPresynapticOutputRequired() &&
       (!getArchetype().isPreOutputModelFused() || getArchetype().isPreOutputModelFuseSource())) 
    {
        addField(precision, "revInSyn", getArchetype().getInSynLocation(), 0, "group->numSrcNeurons");
    }

    // Add WUM extra global parmeters
    addEGPs(getArchetype().getWUModel()->getExtraGlobalParams(), "",
            [this](const std::string &name) { return getArchetype().getWUExtraGlobalParamLocation(name); });

    // If WUM variables aren't global
    if(!(getArchetype().getMatrixType() & SynapseMatrixWeight::GLOBAL)) {
        // Loop through variables
        for(const auto &var : getArchetype().getWUModel()->getVars()) {
            const auto *varInitSnippet = getArchetype().getWUVarInitialisers().at(var.name).getSnippet();
            unsigned int flags = POINTER_FIELD_PUSH_PULL_GET | POINTER_FIELD_STATE;
            if(!varInitSnippet->getCode().empty()) {
                flags |= POINTER_FIELD_AUTO_INITIALISED;
            }

            if(getArchetype().getMatrixType() & SynapseMatrixWeight::INDIVIDUAL) {
                addField(var.type, var.name, getArchetype().getWUVarLocation(var.name), 
                         flags, "group->numSrcNeurons * group->rowStride", 
                         getVarAccessDuplication(var.access));
            }
            else if(getArchetype().getMatrixType() & SynapseMatrixWeight::KERNEL) {
                addField(var.type, var.name, getArchetype().getWUVarLocation(var.name), 
                         flags, "group->kernelSizeFlattened", 
                         getVarAccessDuplication(var.access));
            }

            addEGPs(varInitSnippet->getExtraGlobalParams(), var.name); 
        }
    }
    
    // If presynaptic weight update model either isn't fused or archetype is the fuse source
    if(!getArchetype().isWUPreModelFused() || getArchetype().isWUPreModelFuseSource()) {
        // Loop through WUM presynaptic variables
        for(const auto &var : getArchetype().getWUModel()->getPreVars()) {
            const auto *varInitSnippet = getArchetype().getWUPreVarInitialisers().at(var.name).getSnippet();
            unsigned int flags = 0;
            if(!varInitSnippet->getCode().empty()) {
                flags |= POINTER_FIELD_AUTO_INITIALISED;
            }
            if(!getArchetype().isWUPreModelFused()) {
                flags |= (POINTER_FIELD_PUSH_PULL_GET | POINTER_FIELD_STATE);
            }

            addField(var.type, var.name, getArchetype().getWUPreVarLocation(var.name),
                     flags, "group->numSrcNeurons", getVarAccessDuplication(var.access));
            addEGPs(varInitSnippet->getExtraGlobalParams(), var.name);
        }
    }

    // If postsynaptic weight update model either isn't fused or archetype is the fuse source
    if(!getArchetype().isWUPostModelFused() || getArchetype().isWUPostModelFuseSource()) {
        // Loop through WUM presynaptic variables
        for(const auto &var : getArchetype().getWUModel()->getPostVars()) {
            const auto *varInitSnippet = getArchetype().getWUPostVarInitialisers().at(var.name).getSnippet();
            unsigned int flags = 0;
            if(!varInitSnippet->getCode().empty()) {
                flags |= POINTER_FIELD_AUTO_INITIALISED;
            }
            if(!getArchetype().isWUPostModelFused()) {
                flags |= (POINTER_FIELD_PUSH_PULL_GET | POINTER_FIELD_STATE);
            }
            addField(var.type, var.name, getArchetype().getWUPostVarLocation(var.name),
                     flags, "group->numTrgNeurons", getVarAccessDuplication(var.access));
            addEGPs(varInitSnippet->getExtraGlobalParams(), var.name);
        }
    }
}
//----------------------------------------------------------------------------
void SynapseRunnerGroupMerged::genDenDelayPtrUpdate(CodeStream &os) const
{
    // If this merged group actually has a den delay pointer variable
    if((!getArchetype().isPostOutputModelFused() || getArchetype().isPostOutputModelFuseSource())
       && getArchetype().isDendriticDelayRequired())
    {
         // Loop through groups
         os << "for(unsigned int g = 0; g < " << getGroups().size() << "; g++)";
         {
             CodeStream::Scope b(os);

             // Generate update
             os << "auto *group = &mergedSynapseRunnerGroup" << getIndex() << "[g]; " << std::endl;
             os << "group->denDelayPtr  = (group->denDelayPtr + 1) % group->maxDendriticDelayTimesteps;" << std::endl;
         }
    }
}

//----------------------------------------------------------------------------
// CodeGenerator::CurrentSourceRunnerGroupMerged
//----------------------------------------------------------------------------
const std::string CurrentSourceRunnerGroupMerged::name = "CurrentSourceRunner";
//----------------------------------------------------------------------------
CurrentSourceRunnerGroupMerged::CurrentSourceRunnerGroupMerged(size_t index, const std::string&, const std::string&, const BackendBase&,
                                                               const std::vector<std::reference_wrapper<const CurrentSourceInternal>> &groups)
:   RunnerGroupMergedBase<CurrentSourceInternal, CurrentSourceRunnerGroupMerged>(index, groups)
{
    addField("unsigned int", "numNeurons",
              [](const CurrentSourceInternal &cs) { return std::to_string(cs.getTrgNeuronGroup()->getNumNeurons()); });

    // Add extra global parmeters
    addEGPs(getArchetype().getCurrentSourceModel()->getExtraGlobalParams(), "",
            [this](const std::string &name) { return getArchetype().getExtraGlobalParamLocation(name); });

    // Loop through variables
    for(const auto &var : getArchetype().getCurrentSourceModel()->getVars()) {
        const auto *varInitSnippet = getArchetype().getVarInitialisers().at(var.name).getSnippet();
        unsigned int flags = POINTER_FIELD_PUSH_PULL_GET | POINTER_FIELD_STATE;
        if(!varInitSnippet->getCode().empty()) {
            flags |= POINTER_FIELD_AUTO_INITIALISED;
        }

        addField(var.type, var.name, getArchetype().getVarLocation(var.name), 
                 flags, "group->numNeurons", getVarAccessDuplication(var.access));
        addEGPs(varInitSnippet->getExtraGlobalParams(), var.name);
    }
}

//----------------------------------------------------------------------------
// CodeGenerator::CustomUpdateRunnerGroupMerged
//----------------------------------------------------------------------------
const std::string CustomUpdateRunnerGroupMerged::name = "CustomUpdateRunner";
//----------------------------------------------------------------------------
CustomUpdateRunnerGroupMerged::CustomUpdateRunnerGroupMerged(size_t index, const std::string&, const std::string&, const BackendBase&,
                                                             const std::vector<std::reference_wrapper<const CustomUpdateInternal>> &groups)
:   CustomUpdateRunnerGroupMergedBase<CustomUpdateInternal, CustomUpdateRunnerGroupMerged>(index, groups)
{
    addField("unsigned int", "size",
              [](const CustomUpdateInternal &cu) { return std::to_string(cu.getSize()); });
}

//----------------------------------------------------------------------------
// CodeGenerator::CustomUpdateWURunnerGroupMerged
//----------------------------------------------------------------------------
const std::string CustomUpdateWURunnerGroupMerged::name = "CustomUpdateWURunner";
//----------------------------------------------------------------------------
CustomUpdateWURunnerGroupMerged::CustomUpdateWURunnerGroupMerged(size_t index, const std::string&, const std::string&, const BackendBase &backend,
                                                                 const std::vector<std::reference_wrapper<const CustomUpdateWUInternal>> &groups)
    : CustomUpdateRunnerGroupMergedBase<CustomUpdateWUInternal, CustomUpdateWURunnerGroupMerged>(index, groups)
{
    addField("unsigned int", "size",
             [&backend](const CustomUpdateWUInternal &cu) 
             {
                 return std::to_string(cu.getSynapseGroup()->getSrcNeuronGroup()->getNumNeurons() * backend.getSynapticMatrixRowStride(*cu.getSynapseGroup())); 
             });
}
