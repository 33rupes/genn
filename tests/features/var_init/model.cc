//--------------------------------------------------------------------------
/*! \file var_init/model.cc

\brief model definition file that is part of the feature testing
suite of minimal models with known analytic outcomes that are used for continuous integration testing.
*/
//--------------------------------------------------------------------------


#include "modelSpec.h"

//----------------------------------------------------------------------------
// Neuron
//----------------------------------------------------------------------------
class Neuron : public NeuronModels::Base
{
public:
    DECLARE_MODEL(Neuron, 0, 5);

    SET_VARS({{"constant", "scalar"}, {"uniform", "scalar"}, {"normal", "scalar"}, {"exponential", "scalar"}, {"gamma", "scalar"}});
};
IMPLEMENT_MODEL(Neuron);

//----------------------------------------------------------------------------
// CurrentSrc
//----------------------------------------------------------------------------
class CurrentSrc : public CurrentSourceModels::Base
{
public:
    DECLARE_MODEL(CurrentSrc, 0, 5);

    SET_VARS({{"constant", "scalar"}, {"uniform", "scalar"}, {"normal", "scalar"}, {"exponential", "scalar"}, {"gamma", "scalar"}});
};
IMPLEMENT_MODEL(CurrentSrc);

//----------------------------------------------------------------------------
// PostsynapticModel
//----------------------------------------------------------------------------
class PostsynapticModel : public PostsynapticModels::Base
{
public:
    DECLARE_MODEL(PostsynapticModel, 0, 5);

    SET_VARS({{"pconstant", "scalar"}, {"puniform", "scalar"}, {"pnormal", "scalar"}, {"pexponential", "scalar"}, {"pgamma", "scalar"}});
};
IMPLEMENT_MODEL(PostsynapticModel);

//----------------------------------------------------------------------------
// WeightUpdateModel
//----------------------------------------------------------------------------
class WeightUpdateModel : public WeightUpdateModels::Base
{
public:
    DECLARE_WEIGHT_UPDATE_MODEL(WeightUpdateModel, 0, 5, 5, 5);

    SET_VARS({{"constant", "scalar"}, {"uniform", "scalar"}, {"normal", "scalar"}, {"exponential", "scalar"}, {"gamma", "scalar"}});
    SET_PRE_VARS({{"pre_constant", "scalar"}, {"pre_uniform", "scalar"}, {"pre_normal", "scalar"}, {"pre_exponential", "scalar"}, {"pre_gamma", "scalar"}});
    SET_POST_VARS({{"post_constant", "scalar"}, {"post_uniform", "scalar"}, {"post_normal", "scalar"}, {"post_exponential", "scalar"}, {"post_gamma", "scalar"}});
};
IMPLEMENT_MODEL(WeightUpdateModel);

void modelDefinition(ModelSpec &model)
{
    model.setSeed(12345);
    model.setDT(0.1);
    model.setName("var_init");


    // Parameters for configuring uniform and normal distributions
    InitVarSnippet::Uniform::ParamValues uniformParams(
        0.0,        // 0 - min
        1.0);       // 1 - max

    InitVarSnippet::Uniform::ParamValues normalParams(
        0.0,        // 0 - mean
        1.0);       // 1 - sd

    InitVarSnippet::Exponential::ParamValues exponentialParams(
        1.0);       // 0 - lambda

    InitVarSnippet::Gamma::ParamValues gammaParams(
        4.0,        // 0 - a
        1.0);       // 1 - b
    
    // Neuron parameters
    Neuron::VarValues neuronInit(
        13.0,
        initVar<InitVarSnippet::Uniform>(uniformParams),
        initVar<InitVarSnippet::Normal>(normalParams),
        initVar<InitVarSnippet::Exponential>(exponentialParams),
        initVar<InitVarSnippet::Gamma>(gammaParams));

    // Current source parameters
    CurrentSrc::VarValues currentSourceInit(
        13.0,
        initVar<InitVarSnippet::Uniform>(uniformParams),
        initVar<InitVarSnippet::Normal>(normalParams),
        initVar<InitVarSnippet::Exponential>(exponentialParams),
        initVar<InitVarSnippet::Gamma>(gammaParams));

    // PostsynapticModel parameters
    PostsynapticModel::VarValues postsynapticInit(
        13.0,
        initVar<InitVarSnippet::Uniform>(uniformParams),
        initVar<InitVarSnippet::Normal>(normalParams),
        initVar<InitVarSnippet::Exponential>(exponentialParams),
        initVar<InitVarSnippet::Gamma>(gammaParams));

    // WeightUpdateModel parameters
    WeightUpdateModel::VarValues weightUpdateInit(
        13.0,
        initVar<InitVarSnippet::Uniform>(uniformParams),
        initVar<InitVarSnippet::Normal>(normalParams),
        initVar<InitVarSnippet::Exponential>(exponentialParams),
        initVar<InitVarSnippet::Gamma>(gammaParams));
    WeightUpdateModel::PreVarValues weightUpdatePreInit(
        13.0,
        initVar<InitVarSnippet::Uniform>(uniformParams),
        initVar<InitVarSnippet::Normal>(normalParams),
        initVar<InitVarSnippet::Exponential>(exponentialParams),
        initVar<InitVarSnippet::Gamma>(gammaParams));
    WeightUpdateModel::PostVarValues weightUpdatePostInit(
        13.0,
        initVar<InitVarSnippet::Uniform>(uniformParams),
        initVar<InitVarSnippet::Normal>(normalParams),
        initVar<InitVarSnippet::Exponential>(exponentialParams),
        initVar<InitVarSnippet::Gamma>(gammaParams));
    
    // Neuron populations
    model.addNeuronPopulation<NeuronModels::SpikeSource>("SpikeSource", 1, {}, {});
    model.addNeuronPopulation<Neuron>("Pop", 10000, {}, neuronInit);
    model.addCurrentSource<CurrentSrc>("CurrSource", "Pop", {}, currentSourceInit);

    // Dense synapse populations
    model.addSynapsePopulation<WeightUpdateModel, PostsynapticModel>(
        "Dense", SynapseMatrixType::DENSE_INDIVIDUALG, NO_DELAY,
        "SpikeSource", "Pop",
        {}, weightUpdateInit, weightUpdatePreInit, weightUpdatePostInit,
        {}, postsynapticInit);

    // Sparse synapse populations
    model.addSynapsePopulation<WeightUpdateModel, PostsynapticModel>(
        "Sparse", SynapseMatrixType::SPARSE_INDIVIDUALG, NO_DELAY,
        "SpikeSource", "Pop",
        {}, weightUpdateInit, weightUpdatePreInit, weightUpdatePostInit,
        {}, postsynapticInit);

    model.setPrecision(GENN_FLOAT);
}
