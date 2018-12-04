//--------------------------------------------------------------------------
/*! \file decode_matrix_conn_gen_globalg_ragged/model_new.cc

\brief model definition file that is part of the feature testing
suite of minimal models with known analytic outcomes that are used for continuous integration testing.
*/
//--------------------------------------------------------------------------


#include "modelSpec.h"

//----------------------------------------------------------------------------
// Decoder
//----------------------------------------------------------------------------
class Decoder : public InitSparseConnectivitySnippet::Base
{
public:
    DECLARE_SNIPPET(Decoder, 0);

    SET_ROW_BUILD_CODE(
        "if(j < $(num_post)) {\n"
        "   const unsigned int jValue = (1 << j);\n"
        "   if((($(id_pre) + 1) & jValue) != 0)\n"
        "   {\n"
        "       $(addSynapse, j);\n"
        "   }\n"
        "}\n"
        "else {\n"
        "   $(endRow);\n"
        "}\n"
        "j++;\n");
    SET_ROW_BUILD_STATE_VARS({{"j", {"unsigned int", 0}}});
};
IMPLEMENT_SNIPPET(Decoder);

//----------------------------------------------------------------------------
// Neuron
//----------------------------------------------------------------------------
class Neuron : public NeuronModels::Base
{
public:
    DECLARE_MODEL(Neuron, 0, 1);

    SET_SIM_CODE("$(x)= $(Isyn);\n");

    SET_VARS({{"x", "scalar"}});
};

IMPLEMENT_MODEL(Neuron);


void modelDefinition(NNmodel &model)
{
    initGeNN();

    GENN_PREFERENCES::autoInitSparseVars = true;
    GENN_PREFERENCES::defaultVarMode = VarMode::LOC_HOST_DEVICE_INIT_DEVICE;
    GENN_PREFERENCES::defaultSparseConnectivityMode = VarMode::LOC_HOST_DEVICE_INIT_DEVICE;

    model.setDT(0.1);
    model.setName("decode_matrix_conn_gen_globalg_ragged_new");

    // Static synapse parameters
    WeightUpdateModels::StaticPulse::VarValues staticSynapseInit(1.0);    // 0 - Wij (nA)

    model.addNeuronPopulation<NeuronModels::SpikeSource>("Pre", 10, {}, {});
    model.addNeuronPopulation<Neuron>("Post", 4, {}, Neuron::VarValues(0.0));


    model.addSynapsePopulation<WeightUpdateModels::StaticPulse, PostsynapticModels::DeltaCurr>(
        "Syn", SynapseMatrixType::RAGGED_GLOBALG, NO_DELAY, "Pre", "Post",
        {}, staticSynapseInit,
        {}, {},
        initConnectivity<Decoder>({}));

    model.setPrecision(GENN_FLOAT);
    model.finalize();
}
