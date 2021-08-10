// Google test includes
#include "gtest/gtest.h"

// GeNN includes
#include "modelSpecInternal.h"

// GeNN code generator includes
#include "code_generator/modelSpecMerged.h"

// (Single-threaded CPU) backend includes
#include "backend.h"

//--------------------------------------------------------------------------
// Anonymous namespace
//--------------------------------------------------------------------------
namespace
{
class STDPAdditive : public WeightUpdateModels::Base
{
public:
    DECLARE_WEIGHT_UPDATE_MODEL(STDPAdditive, 6, 1, 1, 1);

    SET_PARAM_NAMES({
      "tauPlus",  // 0 - Potentiation time constant (ms)
      "tauMinus", // 1 - Depression time constant (ms)
      "Aplus",    // 2 - Rate of potentiation
      "Aminus",   // 3 - Rate of depression
      "Wmin",     // 4 - Minimum weight
      "Wmax"});   // 5 - Maximum weight

    SET_VARS({{"g", "scalar"}});
    SET_PRE_VARS({{"preTrace", "scalar"}});
    SET_POST_VARS({{"postTrace", "scalar"}});

    SET_PRE_SPIKE_CODE(
        "scalar dt = $(t) - $(sT_pre);\n"
        "$(preTrace) = ($(preTrace) * exp(-dt / $(tauPlus))) + 1.0;\n");

    SET_POST_SPIKE_CODE(
        "scalar dt = $(t) - $(sT_post);\n"
        "$(postTrace) = ($(postTrace) * exp(-dt / $(tauMinus))) + 1.0;\n");

    SET_SIM_CODE(
        "$(addToInSyn, $(g));\n"
        "scalar dt = $(t) - $(sT_post); \n"
        "if (dt > 0) {\n"
        "    const scalar timing = $(postTrace) * exp(-dt / $(tauMinus));\n"
        "    const scalar newWeight = $(g) - ($(Aminus) * timing);\n"
        "    $(g) = fmax($(Wmin), newWeight);\n"
        "}\n");
    SET_LEARN_POST_CODE(
        "scalar dt = $(t) - $(sT_pre);\n"
        "if (dt > 0) {\n"
        "    const scalar timing = $(postTrace) * exp(-dt / $(tauPlus));\n"
        "    const scalar newWeight = $(g) + ($(Aplus) * timing);\n"
        "    $(g) = fmin($(Wmax), newWeight);\n"
        "}\n");

    SET_NEEDS_PRE_SPIKE_TIME(true);
    SET_NEEDS_POST_SPIKE_TIME(true);
};
IMPLEMENT_MODEL(STDPAdditive);

class Continuous : public WeightUpdateModels::Base
{
public:
    DECLARE_MODEL(Continuous, 0, 1);

    SET_VARS({{"g", "scalar"}});

    SET_SYNAPSE_DYNAMICS_CODE("$(addToInSyn, $(g) * $(V_pre));\n");
};
IMPLEMENT_MODEL(Continuous);

class PostRepeatVal : public InitVarSnippet::Base
{
public:
    DECLARE_SNIPPET(PostRepeatVal, 0);

    SET_CODE("$(value) = $(values)[$(id_post) % 10];");

    SET_EXTRA_GLOBAL_PARAMS({{"values", "scalar*"}});
};
IMPLEMENT_SNIPPET(PostRepeatVal);

class PreRepeatVal : public InitVarSnippet::Base
{
public:
    DECLARE_SNIPPET(PreRepeatVal, 0);

    SET_CODE("$(value) = $(values)[$(id_re) % 10];");

    SET_EXTRA_GLOBAL_PARAMS({{"values", "scalar*"}});
};
IMPLEMENT_SNIPPET(PreRepeatVal);

class Sum : public CustomUpdateModels::Base
{
    DECLARE_CUSTOM_UPDATE_MODEL(Sum, 0, 1, 2);

    SET_UPDATE_CODE("$(sum) = $(a) + $(b);\n");

    SET_VARS({{"sum", "scalar"}});
    SET_VAR_REFS({{"a", "scalar", VarAccessMode::READ_ONLY}, 
                  {"b", "scalar", VarAccessMode::READ_ONLY}});
};
IMPLEMENT_MODEL(Sum);
}   // Anonymous namespace

//--------------------------------------------------------------------------
// Tests
//--------------------------------------------------------------------------
TEST(SynapseGroup, WUVarReferencedByCustomUpdate)
{
     ModelSpecInternal model;

    // Add two neuron group to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Pre", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Post", 25, paramVals, varVals);

    STDPAdditive::ParamValues wumParams(10.0, 10.0, 0.01, 0.01, 0.0, 1.0);
    STDPAdditive::VarValues wumVarVals(0.0);
    STDPAdditive::PreVarValues wumPreVarVals(0.0);
    STDPAdditive::PostVarValues wumPostVarVals(0.0);

    auto *sg1 = model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>(
        "Synapses1", SynapseMatrixType::DENSE_INDIVIDUALG, NO_DELAY,
        "Pre", "Post",
        wumParams, wumVarVals, wumPreVarVals, wumPostVarVals,
        {}, {});
    auto *sg2 = model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>(
        "Synapses2", SynapseMatrixType::DENSE_INDIVIDUALG, NO_DELAY,
        "Pre", "Post",
        wumParams, wumVarVals, wumPreVarVals, wumPostVarVals,
        {}, {});
    auto *sg3 = model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>(
        "Synapses3", SynapseMatrixType::DENSE_INDIVIDUALG, NO_DELAY,
        "Pre", "Post",
        wumParams, wumVarVals, wumPreVarVals, wumPostVarVals,
        {}, {});

    
    Sum::VarValues sumVarValues(0.0);
    Sum::WUVarReferences sumVarReferences2(createWUVarRef(sg2, "g"), createWUVarRef(sg2, "g"));
    Sum::VarReferences sumVarReferences3(createWUPreVarRef(sg3, "preTrace"), createWUPreVarRef(sg3, "preTrace"));

    model.addCustomUpdate<Sum>("SumWeight2", "CustomUpdate",
                               {}, sumVarValues, sumVarReferences2);
    model.addCustomUpdate<Sum>("SumWeight3", "CustomUpdate",
                               {}, sumVarValues, sumVarReferences3);
    model.finalize();

    ASSERT_FALSE(static_cast<SynapseGroupInternal*>(sg1)->areWUVarReferencedByCustomUpdate());
    ASSERT_TRUE(static_cast<SynapseGroupInternal*>(sg2)->areWUVarReferencedByCustomUpdate());
    ASSERT_FALSE(static_cast<SynapseGroupInternal*>(sg3)->areWUVarReferencedByCustomUpdate());
}
//--------------------------------------------------------------------------
TEST(SynapseGroup, CompareWUDifferentModel)
{
    ModelSpecInternal model;

    // Add two neuron groups to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons1", 10, paramVals, varVals);

    WeightUpdateModels::StaticPulse::VarValues staticPulseVarVals(0.1);
    WeightUpdateModels::StaticPulseDendriticDelay::VarValues staticPulseDendriticVarVals(0.1, 1);
    auto *sg0 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses0", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseVarVals,
                                                                                                           {}, {});
    auto *sg1 = model.addSynapsePopulation<WeightUpdateModels::StaticPulseDendriticDelay, PostsynapticModels::DeltaCurr>("Synapses1", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                                                         "Neurons0", "Neurons1",
                                                                                                                         {}, staticPulseDendriticVarVals,
                                                                                                                         {}, {});
    // Finalize model
    model.finalize();

    SynapseGroupInternal *sg0Internal = static_cast<SynapseGroupInternal*>(sg0);
    SynapseGroupInternal *sg1Internal = static_cast<SynapseGroupInternal*>(sg1);
    ASSERT_NE(sg0Internal->getWUHashDigest(), sg1Internal->getWUHashDigest());
    ASSERT_NE(sg0Internal->getWUInitHashDigest(), sg1Internal->getWUInitHashDigest());

    // Create a backend
    CodeGenerator::SingleThreadedCPU::Preferences preferences;
    CodeGenerator::SingleThreadedCPU::Backend backend(model.getPrecision(), preferences);

    // Merge model
    CodeGenerator::ModelSpecMerged modelSpecMerged(model, backend);

    // Check all groups are merged
    ASSERT_TRUE(modelSpecMerged.getMergedNeuronUpdateGroups().size() == 2);
    ASSERT_TRUE(modelSpecMerged.getMergedPresynapticUpdateGroups().size() == 2);
    ASSERT_TRUE(modelSpecMerged.getMergedPostsynapticUpdateGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseDynamicsGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedNeuronInitGroups().size() == 2);
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseSparseInitGroups().size() == 2);

}

TEST(SynapseGroup, CompareWUDifferentGlobalG)
{
    ModelSpecInternal model;

    // Add two neuron groups to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons1", 10, paramVals, varVals);

    WeightUpdateModels::StaticPulse::VarValues staticPulseAVarVals(0.1);
    WeightUpdateModels::StaticPulse::VarValues staticPulseBVarVals(0.2);
    auto *sg0 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses0", SynapseMatrixType::SPARSE_GLOBALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseAVarVals,
                                                                                                           {}, {});
    auto *sg1 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses1", SynapseMatrixType::SPARSE_GLOBALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseAVarVals,
                                                                                                           {}, {});
    auto *sg2 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses2", SynapseMatrixType::SPARSE_GLOBALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseBVarVals,
                                                                                                           {}, {});
    // Finalize model
    model.finalize();

    SynapseGroupInternal *sg0Internal = static_cast<SynapseGroupInternal*>(sg0);
    SynapseGroupInternal *sg1Internal = static_cast<SynapseGroupInternal*>(sg1);
    SynapseGroupInternal *sg2Internal = static_cast<SynapseGroupInternal*>(sg2);
    ASSERT_EQ(sg0Internal->getWUHashDigest(), sg1Internal->getWUHashDigest());
    ASSERT_EQ(sg0Internal->getWUHashDigest(), sg2Internal->getWUHashDigest());

    // Create a backend
    CodeGenerator::SingleThreadedCPU::Preferences preferences;
    CodeGenerator::SingleThreadedCPU::Backend backend(model.getPrecision(), preferences);

    // Merge model
    CodeGenerator::ModelSpecMerged modelSpecMerged(model, backend);

    // Check all groups are merged
    ASSERT_TRUE(modelSpecMerged.getMergedNeuronUpdateGroups().size() == 2);
    ASSERT_TRUE(modelSpecMerged.getMergedPresynapticUpdateGroups().size() == 1);
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseConnectivityInitGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseDenseInitGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseSparseInitGroups().empty());

    // Check that global g var is heterogeneous
    ASSERT_TRUE(modelSpecMerged.getMergedPresynapticUpdateGroups().at(0).isWUGlobalVarHeterogeneous(0));
}

TEST(SynapseGroup, CompareWUDifferentProceduralConnectivity)
{
    ModelSpecInternal model;

    // Add two neuron groups to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons1", 10, paramVals, varVals);

    InitSparseConnectivitySnippet::FixedProbability::ParamValues fixedProbParamsA(0.1);
    InitSparseConnectivitySnippet::FixedProbability::ParamValues fixedProbParamsB(0.4);
    WeightUpdateModels::StaticPulse::VarValues staticPulseVarVals(0.1);
    auto *sg0 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses0", SynapseMatrixType::PROCEDURAL_GLOBALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseVarVals,
                                                                                                           {}, {},
                                                                                                           initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParamsA));
    auto *sg1 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses1", SynapseMatrixType::PROCEDURAL_GLOBALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseVarVals,
                                                                                                           {}, {},
                                                                                                           initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParamsA));
    auto *sg2 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses2", SynapseMatrixType::PROCEDURAL_GLOBALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseVarVals,
                                                                                                           {}, {},
                                                                                                           initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParamsB));
    // Finalize model
    model.finalize();

    SynapseGroupInternal *sg0Internal = static_cast<SynapseGroupInternal*>(sg0);
    SynapseGroupInternal *sg1Internal = static_cast<SynapseGroupInternal*>(sg1);
    SynapseGroupInternal *sg2Internal = static_cast<SynapseGroupInternal*>(sg2);
    ASSERT_EQ(sg0Internal->getWUHashDigest(), sg1Internal->getWUHashDigest());
    ASSERT_EQ(sg0Internal->getWUHashDigest(), sg2Internal->getWUHashDigest());

    // Create a backend
    CodeGenerator::SingleThreadedCPU::Preferences preferences;
    CodeGenerator::SingleThreadedCPU::Backend backend(model.getPrecision(), preferences);

    // Merge model
    CodeGenerator::ModelSpecMerged modelSpecMerged(model, backend);

    // Check all groups are merged
    ASSERT_TRUE(modelSpecMerged.getMergedNeuronUpdateGroups().size() == 2);
    ASSERT_TRUE(modelSpecMerged.getMergedPresynapticUpdateGroups().size() == 1);
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseConnectivityInitGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseDenseInitGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseSparseInitGroups().empty());

    // Check that connectivity parameter is heterogeneous
    // **NOTE** raw parameter is NOT as only derived parameter is used in code
    ASSERT_FALSE(modelSpecMerged.getMergedPresynapticUpdateGroups().at(0).isConnectivityInitParamHeterogeneous(0));
    ASSERT_TRUE(modelSpecMerged.getMergedPresynapticUpdateGroups().at(0).isConnectivityInitDerivedParamHeterogeneous(0));
}

TEST(SynapseGroup, CompareWUDifferentProceduralVars)
{
    ModelSpecInternal model;

    // Add two neuron groups to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons1", 10, paramVals, varVals);

    InitSparseConnectivitySnippet::FixedProbability::ParamValues fixedProbParams(0.1);
    InitVarSnippet::Uniform::ParamValues uniformParamsA(0.5, 1.0);
    InitVarSnippet::Uniform::ParamValues uniformParamsB(0.25, 0.5);
    WeightUpdateModels::StaticPulse::VarValues staticPulseVarValsA(initVar<InitVarSnippet::Uniform>(uniformParamsA));
    WeightUpdateModels::StaticPulse::VarValues staticPulseVarValsB(initVar<InitVarSnippet::Uniform>(uniformParamsB));
    auto *sg0 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses0", SynapseMatrixType::PROCEDURAL_PROCEDURALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseVarValsA,
                                                                                                           {}, {},
                                                                                                           initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
    auto *sg1 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses1", SynapseMatrixType::PROCEDURAL_PROCEDURALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseVarValsA,
                                                                                                           {}, {},
                                                                                                           initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
    auto *sg2 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses2", SynapseMatrixType::PROCEDURAL_PROCEDURALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseVarValsB,
                                                                                                           {}, {},
                                                                                                           initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
    // Finalize model
    model.finalize();

    SynapseGroupInternal *sg0Internal = static_cast<SynapseGroupInternal*>(sg0);
    SynapseGroupInternal *sg1Internal = static_cast<SynapseGroupInternal*>(sg1);
    SynapseGroupInternal *sg2Internal = static_cast<SynapseGroupInternal*>(sg2);
    ASSERT_EQ(sg0Internal->getWUHashDigest(), sg1Internal->getWUHashDigest());
    ASSERT_EQ(sg0Internal->getWUHashDigest(), sg2Internal->getWUHashDigest());

    // Create a backend
    CodeGenerator::SingleThreadedCPU::Preferences preferences;
    CodeGenerator::SingleThreadedCPU::Backend backend(model.getPrecision(), preferences);

    // Merge model
    CodeGenerator::ModelSpecMerged modelSpecMerged(model, backend);

    // Check all groups are merged
    ASSERT_TRUE(modelSpecMerged.getMergedNeuronUpdateGroups().size() == 2);
    ASSERT_TRUE(modelSpecMerged.getMergedPresynapticUpdateGroups().size() == 1);
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseConnectivityInitGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseDenseInitGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseSparseInitGroups().empty());

    // Check that only synaptic weight initialistion parameters are heterogeneous
    ASSERT_FALSE(modelSpecMerged.getMergedPresynapticUpdateGroups().at(0).isConnectivityInitParamHeterogeneous(0));
    ASSERT_FALSE(modelSpecMerged.getMergedPresynapticUpdateGroups().at(0).isConnectivityInitDerivedParamHeterogeneous(0));
    ASSERT_TRUE(modelSpecMerged.getMergedPresynapticUpdateGroups().at(0).isWUVarInitParamHeterogeneous(0, 0));
    ASSERT_TRUE(modelSpecMerged.getMergedPresynapticUpdateGroups().at(0).isWUVarInitParamHeterogeneous(0, 1));
}

TEST(SynapseGroup, CompareWUDifferentProceduralSnippet)
{
    ModelSpecInternal model;

    // Add two neuron groups to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons1", 10, paramVals, varVals);

    WeightUpdateModels::StaticPulse::VarValues staticPulseVarValsA(initVar<PostRepeatVal>());
    WeightUpdateModels::StaticPulse::VarValues staticPulseVarValsB(initVar<PreRepeatVal>());
    auto *sg0 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses0", SynapseMatrixType::DENSE_PROCEDURALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseVarValsA,
                                                                                                           {}, {});
    auto *sg1 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses1", SynapseMatrixType::DENSE_PROCEDURALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseVarValsA,
                                                                                                           {}, {});
    auto *sg2 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses2", SynapseMatrixType::DENSE_PROCEDURALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseVarValsB,
                                                                                                           {}, {});
    // Finalize model
    model.finalize();

    SynapseGroupInternal *sg0Internal = static_cast<SynapseGroupInternal*>(sg0);
    SynapseGroupInternal *sg1Internal = static_cast<SynapseGroupInternal*>(sg1);
    SynapseGroupInternal *sg2Internal = static_cast<SynapseGroupInternal*>(sg2);
    ASSERT_EQ(sg0Internal->getWUHashDigest(), sg1Internal->getWUHashDigest());
    ASSERT_NE(sg0Internal->getWUHashDigest(), sg2Internal->getWUHashDigest());

    // Create a backend
    CodeGenerator::SingleThreadedCPU::Preferences preferences;
    CodeGenerator::SingleThreadedCPU::Backend backend(model.getPrecision(), preferences);

    // Merge model
    CodeGenerator::ModelSpecMerged modelSpecMerged(model, backend);

    // Check all groups are merged
    ASSERT_TRUE(modelSpecMerged.getMergedNeuronUpdateGroups().size() == 2);
    ASSERT_TRUE(modelSpecMerged.getMergedPresynapticUpdateGroups().size() == 2);
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseConnectivityInitGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseDenseInitGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseSparseInitGroups().empty());
}

TEST(SynapseGroup, InitCompareWUDifferentVars)
{
    ModelSpecInternal model;

    // Add two neuron groups to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons1", 10, paramVals, varVals);

    InitSparseConnectivitySnippet::FixedProbability::ParamValues fixedProbParams(0.1);
    STDPAdditive::ParamValues params(10.0, 10.0, 0.01, 0.01, 0.0, 1.0);
    STDPAdditive::VarValues varValsA(0.0);
    STDPAdditive::VarValues varValsB(1.0);
    STDPAdditive::PreVarValues preVarVals(0.0);
    STDPAdditive::PostVarValues postVarVals(0.0);

    auto *sg0 = model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>("Synapses0", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                        "Neurons0", "Neurons1",
                                                                                        params, varValsA, preVarVals, postVarVals,
                                                                                        {}, {},
                                                                                        initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
    auto *sg1 = model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>("Synapses1", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                        "Neurons0", "Neurons1",
                                                                                        params, varValsA, preVarVals, postVarVals,
                                                                                        {}, {},
                                                                                        initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
    auto *sg2 = model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>("Synapses2", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                        "Neurons0", "Neurons1",
                                                                                        params, varValsB, preVarVals, postVarVals,
                                                                                        {}, {},
                                                                                        initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
    // Finalize model
    model.finalize();

    SynapseGroupInternal *sg0Internal = static_cast<SynapseGroupInternal *>(sg0);
    SynapseGroupInternal *sg1Internal = static_cast<SynapseGroupInternal *>(sg1);
    SynapseGroupInternal *sg2Internal = static_cast<SynapseGroupInternal *>(sg2);
    ASSERT_EQ(sg0Internal->getWUInitHashDigest(), sg1Internal->getWUInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUInitHashDigest(), sg2Internal->getWUInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUPreInitHashDigest(), sg1Internal->getWUPreInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUPreInitHashDigest(), sg2Internal->getWUPreInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUPostInitHashDigest(), sg1Internal->getWUPostInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUPostInitHashDigest(), sg2Internal->getWUPostInitHashDigest());

    // Create a backend
    CodeGenerator::SingleThreadedCPU::Preferences preferences;
    CodeGenerator::SingleThreadedCPU::Backend backend(model.getPrecision(), preferences);

    // Merge model
    CodeGenerator::ModelSpecMerged modelSpecMerged(model, backend);

    // Check all groups are merged
    ASSERT_TRUE(modelSpecMerged.getMergedNeuronUpdateGroups().size() == 2);
    ASSERT_TRUE(modelSpecMerged.getMergedPresynapticUpdateGroups().size() == 1);
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseConnectivityInitGroups().size() == 1);
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseDenseInitGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseSparseInitGroups().size() == 1);

    // Check that only synaptic weight initialistion parameters are heterogeneous
    ASSERT_FALSE(modelSpecMerged.getMergedSynapseConnectivityInitGroups().at(0).isConnectivityInitParamHeterogeneous(0));
    ASSERT_FALSE(modelSpecMerged.getMergedSynapseConnectivityInitGroups().at(0).isConnectivityInitDerivedParamHeterogeneous(0));
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseSparseInitGroups().at(0).isWUVarInitParamHeterogeneous(0, 0));
}

TEST(SynapseGroup, InitCompareWUDifferentPreVars)
{
    ModelSpecInternal model;

    // Add two neuron groups to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons1", 10, paramVals, varVals);

    InitSparseConnectivitySnippet::FixedProbability::ParamValues fixedProbParams(0.1);
    STDPAdditive::ParamValues params(10.0, 10.0, 0.01, 0.01, 0.0, 1.0);
    STDPAdditive::VarValues synVarVals(0.0);
    STDPAdditive::PreVarValues preVarValsA(0.0);
    STDPAdditive::PreVarValues preVarValsB(1.0);
    STDPAdditive::PostVarValues postVarVals(0.0);

    auto *sg0 = model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>("Synapses0", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                        "Neurons0", "Neurons1",
                                                                                        params, synVarVals, preVarValsA, postVarVals,
                                                                                        {}, {},
                                                                                        initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
    auto *sg1 = model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>("Synapses1", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                        "Neurons0", "Neurons1",
                                                                                        params, synVarVals, preVarValsA, postVarVals,
                                                                                        {}, {},
                                                                                        initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
    auto *sg2 = model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>("Synapses2", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                        "Neurons0", "Neurons1",
                                                                                        params, synVarVals, preVarValsB, postVarVals,
                                                                                        {}, {},
                                                                                        initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
    // Finalize model
    model.finalize();

    SynapseGroupInternal *sg0Internal = static_cast<SynapseGroupInternal *>(sg0);
    SynapseGroupInternal *sg1Internal = static_cast<SynapseGroupInternal *>(sg1);
    SynapseGroupInternal *sg2Internal = static_cast<SynapseGroupInternal *>(sg2);
    ASSERT_EQ(sg0Internal->getWUPreInitHashDigest(), sg1Internal->getWUPreInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUPreInitHashDigest(), sg2Internal->getWUPreInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUInitHashDigest(), sg1Internal->getWUInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUInitHashDigest(), sg2Internal->getWUInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUPostInitHashDigest(), sg1Internal->getWUPostInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUPostInitHashDigest(), sg2Internal->getWUPostInitHashDigest());
}

TEST(SynapseGroup, InitCompareWUDifferentPostVars)
{
    ModelSpecInternal model;

    // Add two neuron groups to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons1", 10, paramVals, varVals);

    InitSparseConnectivitySnippet::FixedProbability::ParamValues fixedProbParams(0.1);
    STDPAdditive::ParamValues params(10.0, 10.0, 0.01, 0.01, 0.0, 1.0);
    STDPAdditive::VarValues synVarVals(0.0);
    STDPAdditive::PreVarValues preVarVals(0.0);
    STDPAdditive::PostVarValues postVarValsA(0.0);
    STDPAdditive::PostVarValues postVarValsB(1.0);

    auto *sg0 = model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>("Synapses0", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                        "Neurons0", "Neurons1",
                                                                                        params, synVarVals, preVarVals, postVarValsA,
                                                                                        {}, {},
                                                                                        initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
    auto *sg1 = model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>("Synapses1", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                        "Neurons0", "Neurons1",
                                                                                        params, synVarVals, preVarVals, postVarValsA,
                                                                                        {}, {},
                                                                                        initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
    auto *sg2 = model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>("Synapses2", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                        "Neurons0", "Neurons1",
                                                                                        params, synVarVals, preVarVals, postVarValsB,
                                                                                        {}, {},
                                                                                        initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
    // Finalize model
    model.finalize();

    SynapseGroupInternal *sg0Internal = static_cast<SynapseGroupInternal *>(sg0);
    SynapseGroupInternal *sg1Internal = static_cast<SynapseGroupInternal *>(sg1);
    SynapseGroupInternal *sg2Internal = static_cast<SynapseGroupInternal *>(sg2);
    ASSERT_EQ(sg0Internal->getWUPostInitHashDigest(), sg1Internal->getWUPostInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUPostInitHashDigest(), sg2Internal->getWUPostInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUInitHashDigest(), sg1Internal->getWUInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUInitHashDigest(), sg2Internal->getWUInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUPreInitHashDigest(), sg1Internal->getWUPreInitHashDigest());
    ASSERT_EQ(sg0Internal->getWUPreInitHashDigest(), sg2Internal->getWUPreInitHashDigest());
}

TEST(SynapseGroup, InitCompareWUDifferentHeterogeneousParamVarState)
{
    ModelSpecInternal model;

    // Add two neuron groups to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons1", 10, paramVals, varVals);

    InitSparseConnectivitySnippet::FixedNumberPostWithReplacement::ParamValues fixedNumberPostParamsA(4);
    InitSparseConnectivitySnippet::FixedNumberPostWithReplacement::ParamValues fixedNumberPostParamsB(8);
    WeightUpdateModels::StaticPulse::VarValues staticPulseVarVals(0.1);
    auto *sg0 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses0", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseVarVals,
                                                                                                           {}, {},
                                                                                                           initConnectivity<InitSparseConnectivitySnippet::FixedNumberPostWithReplacement>(fixedNumberPostParamsA));
    auto *sg1 = model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>("Synapses1", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
                                                                                                           "Neurons0", "Neurons1",
                                                                                                           {}, staticPulseVarVals,
                                                                                                           {}, {},
                                                                                                           initConnectivity<InitSparseConnectivitySnippet::FixedNumberPostWithReplacement>(fixedNumberPostParamsB));
    // Finalize model
    model.finalize();

    SynapseGroupInternal *sg0Internal = static_cast<SynapseGroupInternal *>(sg0);
    SynapseGroupInternal *sg1Internal = static_cast<SynapseGroupInternal *>(sg1);
    ASSERT_EQ(sg0Internal->getWUHashDigest(), sg1Internal->getWUHashDigest());
    ASSERT_EQ(sg0Internal->getWUInitHashDigest(), sg1Internal->getWUInitHashDigest());

    // Create a backend
    CodeGenerator::SingleThreadedCPU::Preferences preferences;
    CodeGenerator::SingleThreadedCPU::Backend backend(model.getPrecision(), preferences);

    // Merge model
    CodeGenerator::ModelSpecMerged modelSpecMerged(model, backend);

    // Check all groups are merged
    ASSERT_TRUE(modelSpecMerged.getMergedNeuronUpdateGroups().size() == 2);
    ASSERT_TRUE(modelSpecMerged.getMergedPresynapticUpdateGroups().size() == 1);
    ASSERT_TRUE(modelSpecMerged.getMergedPostsynapticUpdateGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseDynamicsGroups().empty());
    ASSERT_TRUE(modelSpecMerged.getMergedNeuronInitGroups().size() == 2);
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseSparseInitGroups().size() == 1);

    // Check that fixed number post connectivity row length parameters are heterogeneous
    ASSERT_TRUE(modelSpecMerged.getMergedSynapseConnectivityInitGroups().at(0).isConnectivityInitParamHeterogeneous(0));
}

TEST(SynapseGroup, InvalidMatrixTypes)
{
    ModelSpecInternal model;

    // Add four neuron groups to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("NeuronsA", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("NeuronsB", 20, paramVals, varVals);

    // Check that making a synapse group with procedural connectivity fails if no connectivity initialiser is specified
    try {
        model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>(
            "NeuronsA_NeuronsB_1", SynapseMatrixType::PROCEDURAL_GLOBALG, NO_DELAY,
            "NeuronsA", "NeuronsB",
            {}, {1.0},
            {}, {});
        FAIL();
    }
    catch(const std::runtime_error &) {
    }

    // Check that making a synapse group with procedural connectivity and STDP fails
    try {
        InitSparseConnectivitySnippet::FixedProbability::ParamValues fixedProbParams(0.1);
        STDPAdditive::ParamValues params(10.0, 10.0, 0.01, 0.01, 0.0, 1.0);
        STDPAdditive::VarValues varVals(0.0);
        STDPAdditive::PreVarValues preVarVals(0.0);
        STDPAdditive::PostVarValues postVarVals(0.0);

        model.addSynapsePopulation<STDPAdditive, PostsynapticModels::DeltaCurr>("NeuronsA_NeuronsB_2", SynapseMatrixType::PROCEDURAL_GLOBALG, NO_DELAY,
                                                                                "NeuronsA", "NeuronsB",
                                                                                params, varVals, preVarVals, postVarVals,
                                                                                {}, {},
                                                                                initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
        FAIL();
    }
    catch(const std::runtime_error &) {
    }

    // Check that making a synapse group with procedural connectivity and synapse dynamics fails
    try {
        InitSparseConnectivitySnippet::FixedProbability::ParamValues fixedProbParams(0.1);
        model.addSynapsePopulation<Continuous, PostsynapticModels::DeltaCurr>("NeuronsA_NeuronsB_3", SynapseMatrixType::PROCEDURAL_GLOBALG, NO_DELAY,
                                                                              "NeuronsA", "NeuronsB",
                                                                              {}, {0.0}, {}, {},
                                                                              {}, {},
                                                                              initConnectivity<InitSparseConnectivitySnippet::FixedProbability>(fixedProbParams));
        FAIL();
    }
    catch(const std::runtime_error &) {
    }

    // Check that making a synapse group with dense connections and procedural weights fails if var initialialisers use random numbers
    try {
        model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>(
            "NeuronsA_NeuronsB_4", SynapseMatrixType::DENSE_PROCEDURALG, NO_DELAY,
            "NeuronsA", "NeuronsB",
            {}, {initVar<InitVarSnippet::Uniform>({0.0, 1.0})},
            {}, {});
        FAIL();
    }
    catch(const std::runtime_error &) {
    }
}

TEST(SynapseGroup, InvalidName)
{
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    
    ModelSpec model;
    auto *pre = model.addNeuronPopulation<NeuronModels::SpikeSource>("Pre", 10, {}, {});
    auto *post = model.addNeuronPopulation<NeuronModels::Izhikevich>("Post", 10, paramVals, varVals);
    try {
        model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>(
            "Syn-6", SynapseMatrixType::DENSE_GLOBALG, NO_DELAY,
            "Pre", "Post",
            {}, {1.0},
            {}, {});
        FAIL();
    }
    catch(const std::runtime_error &) {
    }
}

TEST(SynapseGroup, SharedWeightSlaveInvalidMethods)
{
    ModelSpecInternal model;

    // Add four neuron groups to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0A", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons1A", 20, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0B", 10, paramVals, varVals);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons1B", 20, paramVals, varVals);

    model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>(
        "Neurons0A_Neurons1A", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
        "Neurons0A", "Neurons1A",
        {}, { 1.0 },
        {}, {});
    auto *slave = model.addSlaveSynapsePopulation<PostsynapticModels::DeltaCurr>(
        "Neurons0B_Neurons1B", "Neurons0A_Neurons1A", NO_DELAY,
        "Neurons0B", "Neurons1B",
        {}, {});

   
    // Check that you can't call methods which make no snese 
    try {
        slave->setSparseConnectivityLocation(VarLocation::HOST_DEVICE);
        FAIL();
    }
    catch (const std::runtime_error &) {
    }

    try {
        slave->setMaxConnections(4);
        FAIL();
    }
    catch (const std::runtime_error &) {
    }

    try {
        slave->setNarrowSparseIndEnabled(true);
        FAIL();
    }
    catch (const std::runtime_error &) {
    }

    try {
        slave->setWUVarLocation("g", VarLocation::DEVICE);
        FAIL();
    }
    catch (const std::runtime_error &) {
    }
    //setSparseConnectivityExtraGlobalParamLocation
    //setMaxSourceConnections
}
