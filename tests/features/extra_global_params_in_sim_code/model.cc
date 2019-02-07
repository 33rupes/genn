//--------------------------------------------------------------------------
/*! \file extra_global_params_in_sim_code/model.cc

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
    DECLARE_MODEL(Neuron, 0, 2);

    SET_SIM_CODE("$(x)= $(t)+$(shift)+$(input);\n");

    SET_EXTRA_GLOBAL_PARAMS({{"input", "scalar"}});

    SET_VARS({{"x", "scalar"}, {"shift", "scalar"}});
};

IMPLEMENT_MODEL(Neuron);

void modelDefinition(NNmodel &model)
{
  model.setDT(0.1);
  model.setName("extra_global_params_in_sim_code");

  model.addNeuronPopulation<Neuron>("pre", 10, {}, Neuron::VarValues(0.0, 0.0));
  model.setPrecision(GENN_FLOAT);
}
