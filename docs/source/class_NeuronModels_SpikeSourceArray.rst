.. index:: pair: class; NeuronModels::SpikeSourceArray
.. _doxid-dc/d9e/class_neuron_models_1_1_spike_source_array:

class NeuronModels::SpikeSourceArray
====================================

.. toctree::
	:hidden:

Overview
~~~~~~~~

Spike source array. :ref:`More...<details-dc/d9e/class_neuron_models_1_1_spike_source_array>`


.. ref-code-block:: cpp
	:class: doxyrest-overview-code-block

	#include <neuronModels.h>
	
	class SpikeSourceArray: public :ref:`NeuronModels::Base<doxid-df/d1b/class_neuron_models_1_1_base>`
	{
	public:
		// typedefs
	
		typedef :ref:`Snippet::ValueBase<doxid-db/dd1/class_snippet_1_1_value_base>`<0> :target:`ParamValues<doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1a1c5592508bb0871f90c4281fea4e294e>`;
		typedef :ref:`Models::VarInitContainerBase<doxid-d8/d31/class_models_1_1_var_init_container_base>`<2> :target:`VarValues<doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1ac5fd1ae89de3b5eaaf91d3db4772ed21>`;
		typedef :ref:`Models::VarInitContainerBase<doxid-d8/d31/class_models_1_1_var_init_container_base>`<0> :target:`PreVarValues<doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1a8115ed2991ab2fef64e25b6b094d5553>`;
		typedef :ref:`Models::VarInitContainerBase<doxid-d8/d31/class_models_1_1_var_init_container_base>`<0> :target:`PostVarValues<doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1aea120d0d87f8f9fe9f8547df9a9fc358>`;

		// methods
	
		static const NeuronModels::SpikeSourceArray* :target:`getInstance<doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1acff8ee4eb8e80db46e6772efd75c4043>`();
		virtual std::string :ref:`getSimCode<doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1acb3b3dc38079cda1012f2103ed9369e4>`() const;
		virtual std::string :ref:`getThresholdConditionCode<doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1a75781ab0650430033f32c9a93c34f301>`() const;
		virtual std::string :ref:`getResetCode<doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1af47ef1d61637a5fc74cc603f2df36363>`() const;
		virtual :ref:`VarVec<doxid-dc/d39/class_models_1_1_base_1a5a6bc95969a38ac1ac68ab4a0ba94c75>` :ref:`getVars<doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1ac1d1832a768aa939197b1d6097a49bc7>`() const;
		virtual :ref:`EGPVec<doxid-d6/df7/class_snippet_1_1_base_1a43ece29884e2c6cabffe9abf985807c6>` :ref:`getExtraGlobalParams<doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1ab2b96e84342a6b65e36691c07757fa6c>`() const;
		:target:`SET_NEEDS_AUTO_REFRACTORY<doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1a620726ef71e563fcc6e25f66ed494c71>`(false);
	};

Inherited Members
-----------------

.. ref-code-block:: cpp
	:class: doxyrest-overview-inherited-code-block

	public:
		// typedefs
	
		typedef std::vector<std::string> :ref:`StringVec<doxid-d6/df7/class_snippet_1_1_base_1a06cd0f6da1424a20163e12b6fec62519>`;
		typedef std::vector<:ref:`EGP<doxid-d1/d1f/struct_snippet_1_1_base_1_1_e_g_p>`> :ref:`EGPVec<doxid-d6/df7/class_snippet_1_1_base_1a43ece29884e2c6cabffe9abf985807c6>`;
		typedef std::vector<:ref:`ParamVal<doxid-d5/dcc/struct_snippet_1_1_base_1_1_param_val>`> :ref:`ParamValVec<doxid-d6/df7/class_snippet_1_1_base_1a0156727ddf8f9c9cbcbc0d3d913b6b48>`;
		typedef std::vector<:ref:`DerivedParam<doxid-d4/d21/struct_snippet_1_1_base_1_1_derived_param>`> :ref:`DerivedParamVec<doxid-d6/df7/class_snippet_1_1_base_1ad14217cebf11eddffa751a4d5c4792cb>`;
		typedef std::vector<:ref:`Var<doxid-db/db6/struct_models_1_1_base_1_1_var>`> :ref:`VarVec<doxid-dc/d39/class_models_1_1_base_1a5a6bc95969a38ac1ac68ab4a0ba94c75>`;

		// structs
	
		struct :ref:`DerivedParam<doxid-d4/d21/struct_snippet_1_1_base_1_1_derived_param>`;
		struct :ref:`EGP<doxid-d1/d1f/struct_snippet_1_1_base_1_1_e_g_p>`;
		struct :ref:`ParamVal<doxid-d5/dcc/struct_snippet_1_1_base_1_1_param_val>`;
		struct :ref:`Var<doxid-db/db6/struct_models_1_1_base_1_1_var>`;

		// methods
	
		virtual :ref:`~Base<doxid-d6/df7/class_snippet_1_1_base_1a17a9ca158277401f2c190afb1e791d1f>`();
		virtual :ref:`StringVec<doxid-d6/df7/class_snippet_1_1_base_1a06cd0f6da1424a20163e12b6fec62519>` :ref:`getParamNames<doxid-d6/df7/class_snippet_1_1_base_1a0c8374854fbdc457bf0f75e458748580>`() const;
		virtual :ref:`DerivedParamVec<doxid-d6/df7/class_snippet_1_1_base_1ad14217cebf11eddffa751a4d5c4792cb>` :ref:`getDerivedParams<doxid-d6/df7/class_snippet_1_1_base_1ab01de002618efa59541c927ffdd463f5>`() const;
		virtual :ref:`VarVec<doxid-dc/d39/class_models_1_1_base_1a5a6bc95969a38ac1ac68ab4a0ba94c75>` :ref:`getVars<doxid-dc/d39/class_models_1_1_base_1a9df8ba9bf6d971a574ed4745f6cf946c>`() const;
		virtual :ref:`EGPVec<doxid-d6/df7/class_snippet_1_1_base_1a43ece29884e2c6cabffe9abf985807c6>` :ref:`getExtraGlobalParams<doxid-dc/d39/class_models_1_1_base_1a7fdddb7d19382736b330ade62c441de1>`() const;
		size_t :ref:`getVarIndex<doxid-dc/d39/class_models_1_1_base_1afa0e39df5002efc76448e180f82825e4>`(const std::string& varName) const;
		size_t :ref:`getExtraGlobalParamIndex<doxid-dc/d39/class_models_1_1_base_1ae046c19ad56dfb2808c5f4d2cc7475fe>`(const std::string& paramName) const;
		virtual std::string :ref:`getSimCode<doxid-df/d1b/class_neuron_models_1_1_base_1a3de4c7ff580f63c5b0ec12cb461ebd3a>`() const;
		virtual std::string :ref:`getThresholdConditionCode<doxid-df/d1b/class_neuron_models_1_1_base_1a00ffe96ee864dc67936ce75592c6b198>`() const;
		virtual std::string :ref:`getResetCode<doxid-df/d1b/class_neuron_models_1_1_base_1a4bdc01f203f92c2da4d3b1b48109975d>`() const;
		virtual std::string :ref:`getSupportCode<doxid-df/d1b/class_neuron_models_1_1_base_1ada27dc79296ef8368ac2c7ab20ca8c8e>`() const;
		virtual :ref:`Models::Base::ParamValVec<doxid-d6/df7/class_snippet_1_1_base_1a0156727ddf8f9c9cbcbc0d3d913b6b48>` :ref:`getAdditionalInputVars<doxid-df/d1b/class_neuron_models_1_1_base_1afef62c84373334fe4656a754dbb661c7>`() const;
		virtual bool :ref:`isAutoRefractoryRequired<doxid-df/d1b/class_neuron_models_1_1_base_1a32c9b73420bbdf11a373faa4e0cceb09>`() const;

.. _details-dc/d9e/class_neuron_models_1_1_spike_source_array:

Detailed Documentation
~~~~~~~~~~~~~~~~~~~~~~

Spike source array.

A neuron which reads spike times from a global spikes array It has 2 variables:

* ``startSpike`` - Index of the next spike in the global array

* ``endSpike`` - Index of the spike next to the last in the globel array

and 1 global parameter:

* ``spikeTimes`` - Array with all spike times

Methods
-------

.. index:: pair: function; getSimCode
.. _doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1acb3b3dc38079cda1012f2103ed9369e4:

.. ref-code-block:: cpp
	:class: doxyrest-title-code-block

	virtual std::string getSimCode() const

Gets the code that defines the execution of one timestep of integration of the neuron model.

The code will refer to  for the value of the variable with name "NN". It needs to refer to the predefined variable "ISYN", i.e. contain , if it is to receive input.

.. index:: pair: function; getThresholdConditionCode
.. _doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1a75781ab0650430033f32c9a93c34f301:

.. ref-code-block:: cpp
	:class: doxyrest-title-code-block

	virtual std::string getThresholdConditionCode() const

Gets code which defines the condition for a true spike in the described neuron model.

This evaluates to a bool (e.g. "V > 20").

.. index:: pair: function; getResetCode
.. _doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1af47ef1d61637a5fc74cc603f2df36363:

.. ref-code-block:: cpp
	:class: doxyrest-title-code-block

	virtual std::string getResetCode() const

Gets code that defines the reset action taken after a spike occurred. This can be empty.

.. index:: pair: function; getVars
.. _doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1ac1d1832a768aa939197b1d6097a49bc7:

.. ref-code-block:: cpp
	:class: doxyrest-title-code-block

	virtual :ref:`VarVec<doxid-dc/d39/class_models_1_1_base_1a5a6bc95969a38ac1ac68ab4a0ba94c75>` getVars() const

Gets names and types (as strings) of model variables.

.. index:: pair: function; getExtraGlobalParams
.. _doxid-dc/d9e/class_neuron_models_1_1_spike_source_array_1ab2b96e84342a6b65e36691c07757fa6c:

.. ref-code-block:: cpp
	:class: doxyrest-title-code-block

	virtual :ref:`EGPVec<doxid-d6/df7/class_snippet_1_1_base_1a43ece29884e2c6cabffe9abf985807c6>` getExtraGlobalParams() const

Gets names and types (as strings) of additional per-population parameters for the weight update model.

