// Google test includes
#include "gtest/gtest.h"

// GeNN includes
#include "modelSpecInternal.h"

//--------------------------------------------------------------------------
// Tests
//--------------------------------------------------------------------------
TEST(CurrentSource, CompareDifferentModel)
{
    ModelSpecInternal model;

    // Add neuron group to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0", 10, paramVals, varVals);

    // Add one gaussian current source
    CurrentSourceModels::GaussianNoise::ParamValues cs0ParamVals(0.0, 0.1);
    CurrentSource *cs0 = model.addCurrentSource<CurrentSourceModels::GaussianNoise>("CS0", "Neurons0",
                                                                                   cs0ParamVals, {});

    // Add one DC current source
    CurrentSourceModels::DC::ParamValues cs1ParamVals(0.4);
    CurrentSource *cs1 = model.addCurrentSource<CurrentSourceModels::DC>("CS1", "Neurons0",
                                                                         cs1ParamVals, {});

    // Finalize model
    model.finalize();

    CurrentSourceInternal *cs1Internal = static_cast<CurrentSourceInternal*>(cs1);
    ASSERT_FALSE(cs1Internal->canBeMerged(*cs0));
}

TEST(CurrentSource, CompareDifferentParameters)
{
    ModelSpecInternal model;

    // Add neuron group to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0", 10, paramVals, varVals);

    // Add one gaussian current source
    CurrentSourceModels::GaussianNoise::ParamValues cs0ParamVals(0.0, 0.1);
    CurrentSource *cs0 = model.addCurrentSource<CurrentSourceModels::GaussianNoise>("CS0", "Neurons0",
                                                                                   cs0ParamVals, {});

    // Add second gaussian current source
    CurrentSourceModels::GaussianNoise::ParamValues cs1ParamVals(0.0, 0.5);
    CurrentSource *cs1 = model.addCurrentSource<CurrentSourceModels::GaussianNoise>("CS1", "Neurons0",
                                                                                    cs1ParamVals, {});

    // Finalize model
    model.finalize();

    CurrentSourceInternal *cs1Internal = static_cast<CurrentSourceInternal*>(cs1);
    ASSERT_TRUE(cs1Internal->canBeMerged(*cs0));
}

TEST(CurrentSource, CompareSameParameters)
{
    ModelSpecInternal model;

    // Add neuron group to model
    NeuronModels::Izhikevich::ParamValues paramVals(0.02, 0.2, -65.0, 8.0);
    NeuronModels::Izhikevich::VarValues varVals(0.0, 0.0);
    model.addNeuronPopulation<NeuronModels::Izhikevich>("Neurons0", 10, paramVals, varVals);

    // Add one gaussian current source
    CurrentSourceModels::GaussianNoise::ParamValues cs0ParamVals(0.0, 0.1);
    CurrentSource *cs0 = model.addCurrentSource<CurrentSourceModels::GaussianNoise>("CS0", "Neurons0",
                                                                                   cs0ParamVals, {});

    // Add second gaussian current source
    CurrentSourceModels::GaussianNoise::ParamValues cs1ParamVals(0.0, 0.1);
    CurrentSource *cs1 = model.addCurrentSource<CurrentSourceModels::GaussianNoise>("CS1", "Neurons0",
                                                                                    cs1ParamVals, {});

    // Finalize model
    model.finalize();

    CurrentSourceInternal *cs1Internal = static_cast<CurrentSourceInternal*>(cs1);
    ASSERT_TRUE(cs1Internal->canBeMerged(*cs0));
}
