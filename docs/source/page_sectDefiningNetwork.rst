.. index:: pair: page; Defining a network model
.. _doxid-de/d42/sect_defining_network:

Defining a network model
========================

A network model is defined by the user by providing the function

.. ref-code-block:: cpp

	void modelDefinition(:ref:`ModelSpec <doxid-d1/de7/class_model_spec>` &model)

in a separate file, such as ``MyModel.cc``. In this function, the following tasks must be completed:

#. The name of the model must be defined:
   
   .. ref-code-block:: cpp
   
   	model.:ref:`setName <doxid-d1/de7/class_model_spec_1ada1aff7a94eeb36dff721f09d5cf94b4>`("MyModel");

#. Neuron populations (at least one) must be added (see :ref:`Defining neuron populations <doxid-de/d42/sect_defining_network_1subsect11>`). The user may add as many neuron populations as they wish. If resources run out, there will not be a warning but GeNN will fail. However, before this breaking point is reached, GeNN will make all necessary efforts in terms of block size optimisation to accommodate the defined models. All populations must have a unique name.

#. Synapse populations (zero or more) can be added (see :ref:`Defining synapse populations <doxid-de/d42/sect_defining_network_1subsect12>`). Again, the number of synaptic connection populations is unlimited other than by resources.



.. _doxid-de/d42/sect_defining_network_1subsect11:

Defining neuron populations
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Neuron populations are added using the function

.. ref-code-block:: cpp

	model.:ref:`addNeuronPopulation <doxid-d1/de7/class_model_spec_1a0b765be273f3c6cec15092d7dbfdd52b>`<NeuronModel>(name, num, paramValues, varInitialisers);

where the arguments are:

* ``NeuronModel`` : Template argument specifying the type of neuron model These should be derived off :ref:`NeuronModels::Base <doxid-df/d1b/class_neuron_models_1_1_base>` and can either be one of the standard models or user-defined (see :ref:`Neuron models <doxid-da/ddf/sect_neuron_models>`).

* ``const string &name`` : Unique name of the neuron population

* ``unsigned int size`` : number of neurons in the population

* ``NeuronModel::ParamValues paramValues`` : Parameters of this neuron type

* ``NeuronModel::VarValues varInitialisers`` : Initial values or initialisation snippets for variables of this neuron type

The user may add as many neuron populations as the model necessitates. They must all have unique names. The possible values for the arguments, predefined models and their parameters and initial values are detailed :ref:`Neuron models <doxid-da/ddf/sect_neuron_models>` below.





.. _doxid-de/d42/sect_defining_network_1subsect12:

Defining synapse populations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Synapse populations are added with the function

.. ref-code-block:: cpp

	model.:ref:`addSynapsePopulation <doxid-d1/de7/class_model_spec_1abd4e9128a5d4f5f993907134218af0c2>`<WeightUpdateModel, PostsynapticModel>(name, mType, delay, preName, postName, 
	                                                                 weightParamValues, weightVarValues, weightPreVarInitialisers, weightPostVarInitialisers,
	                                                                 postsynapticParamValues, postsynapticVarValues, connectivityInitialiser);

where the arguments are

* ``WeightUpdateModel`` : Template parameter specifying the type of weight update model. These should be derived off :ref:`WeightUpdateModels::Base <doxid-d8/d90/class_weight_update_models_1_1_base>` and can either be one of the standard models or user-defined (see :ref:`Weight update models <doxid-db/d11/sect_synapse_models>`).

* ``PostsynapticModel`` : Template parameter specifying the type of postsynaptic integration model. These should be derived off :ref:`PostsynapticModels::Base <doxid-d3/d2d/class_postsynaptic_models_1_1_base>` and can either be one of the standard models or user-defined (see :ref:`Postsynaptic integration methods <doxid-dd/de4/sect_postsyn>`).

* ``const string &name`` : The name of the synapse population

* ``unsigned int mType`` : How the synaptic matrix is stored. See :ref:`Synaptic matrix types <doxid-d5/d39/subsect34>` for available options.

* ``unsigned int delay`` : Homogeneous (axonal) delay for synapse population (in terms of the simulation time step ``DT``).

* ``const string preName`` : Name of the (existing!) pre-synaptic neuron population.

* ``const string postName`` : Name of the (existing!) post-synaptic neuron population.

* ``WeightUpdateModel::ParamValues weightParamValues`` : The parameter values (common to all synapses of the population) for the weight update model.

* ``WeightUpdateModel::VarValues weightVarInitialisers`` : Initial values or initialisation snippets for the weight update model's state variables

* ``WeightUpdateModel::PreVarValues weightPreVarInitialisers`` : Initial values or initialisation snippets for the weight update model's presynaptic state variables

* ``WeightUpdateModel::PostVarValues weightPostVarInitialisers`` : Initial values or initialisation snippets for the weight update model's postsynaptic state variables

* ``PostsynapticModel::ParamValues postsynapticParamValues`` : The parameter values (common to all postsynaptic neurons) for the postsynaptic model.

* ``PostsynapticModel::VarValues postsynapticVarInitialisers`` : Initial values or initialisation snippets for variables for the postsynaptic model's state variables

* ``:ref:`InitSparseConnectivitySnippet::Init <doxid-dc/d49/class_init_sparse_connectivity_snippet_1_1_init>` connectivityInitialiser`` : Optional argument, specifying the initialisation snippet for synapse population's sparse connectivity (see :ref:`Sparse connectivity initialisation <doxid-dc/df6/sect_sparse_connectivity_initialisation>`).

The :ref:`ModelSpec::addSynapsePopulation() <doxid-d1/de7/class_model_spec_1abd4e9128a5d4f5f993907134218af0c2>` function returns a pointer to the newly created :ref:`SynapseGroup <doxid-d2/d62/class_synapse_group>` object which can be further configured, namely with:

* :ref:`SynapseGroup::setMaxConnections() <doxid-d2/d62/class_synapse_group_1aab6b2fb0ad30189bc11ee3dd7d48dbb2>` and :ref:`SynapseGroup::setMaxSourceConnections() <doxid-d2/d62/class_synapse_group_1a93b12c08d634f1a2300f1b91ef34ea24>` to configure the maximum number of rows and columns respectively allowed in the synaptic matrix - this can improve performance and reduce memory usage when using :ref:`SynapseMatrixConnectivity::SPARSE <doxid-db/d08/synapse_matrix_type_8h_1aedb0946699027562bc78103a5d2a578da0459833ba9cad7cfd7bbfe10d7bbbe6e>` connectivity (see :ref:`Synaptic matrix types <doxid-d5/d39/subsect34>`). When using a sparse connectivity initialisation snippet, these values are set automatically.

* :ref:`SynapseGroup::setMaxDendriticDelayTimesteps() <doxid-d2/d62/class_synapse_group_1a220307d4043e8bf1bed07552829f2a17>` sets the maximum dendritic delay (in terms of the simulation time step ``DT``) allowed for synapses in this population. No values larger than this should be passed to the delay parameter of the ``addToDenDelay`` function in user code (see :ref:`Defining a new weight update model <doxid-db/d11/sect_synapse_models_1sect34>`).

* :ref:`SynapseGroup::setSpanType() <doxid-d2/d62/class_synapse_group_1a97cfec638d856e6e07628bc19490690c>` sets how incoming spike processing is parallelised for this synapse group. The default :ref:`SynapseGroup::SpanType::POSTSYNAPTIC <doxid-d2/d62/class_synapse_group_1a3da23a0e726b05a12e95c3d58645b1a2a39711e1ac5d5263471a6184f362dc02f>` is nearly always the best option, but :ref:`SynapseGroup::SpanType::PRESYNAPTIC <doxid-d2/d62/class_synapse_group_1a3da23a0e726b05a12e95c3d58645b1a2ac583511247567bdc79915d057babba12>` may perform better when there are large numbers of spikes every timestep or very few postsynaptic neurons.

If the synapse matrix uses one of the "GLOBALG" types then the global value of the synapse parameters are taken from the initial value provided in ``weightVarInitialisers`` therefore these must be constant rather than sampled from a distribution etc.

:ref:`Previous <doxid-d6/de1/_user_manual>` \| :ref:`Top <doxid-de/d42/sect_defining_network>` \| :ref:`Next <doxid-da/ddf/sect_neuron_models>`

