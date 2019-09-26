.. index:: pair: class; NeuronModels::TraubMiles
.. _doxid-d6/d4a/class_neuron_models_1_1_traub_miles:

class NeuronModels::TraubMiles
==============================

.. toctree::
	:hidden:

Overview
~~~~~~~~

Hodgkin-Huxley neurons with Traub & Miles algorithm. :ref:`More...<details-d6/d4a/class_neuron_models_1_1_traub_miles>`


.. ref-code-block:: cpp
	:class: doxyrest-overview-code-block

	#include <neuronModels.h>
	
	class TraubMiles: public :ref:`NeuronModels::Base<doxid-df/d1b/class_neuron_models_1_1_base>`
	{
	public:
		// typedefs
	
		typedef :ref:`Snippet::ValueBase<doxid-db/dd1/class_snippet_1_1_value_base>`<7> :target:`ParamValues<doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1a968f4e8ff125d312cf760d36f0dee95e>`;
		typedef :ref:`Models::VarInitContainerBase<doxid-d8/d31/class_models_1_1_var_init_container_base>`<4> :target:`VarValues<doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1a8ec817cdd9f7bbf5432ffca45dcb5fd2>`;
		typedef :ref:`Models::VarInitContainerBase<doxid-d8/d31/class_models_1_1_var_init_container_base>`<0> :target:`PreVarValues<doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1a8d8b78952c1df2ada32ce9ba9e89dba2>`;
		typedef :ref:`Models::VarInitContainerBase<doxid-d8/d31/class_models_1_1_var_init_container_base>`<0> :target:`PostVarValues<doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1a4a2c785c6a7b36aa64026e032c0eeaea>`;

		// methods
	
		static const NeuronModels::TraubMiles* :target:`getInstance<doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1ae47f72166dbcb1e55d2e07755b1526da>`();
		virtual std::string :ref:`getSimCode<doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1a868f03fe6e30b947586cdb6a8d29146a>`() const;
		virtual std::string :ref:`getThresholdConditionCode<doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1afdc035bc1c4f29c57c589b05678d232f>`() const;
		virtual :ref:`StringVec<doxid-d6/df7/class_snippet_1_1_base_1a06cd0f6da1424a20163e12b6fec62519>` :ref:`getParamNames<doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1a248a2c830b67bb80cb6dbbbda57f405d>`() const;
		virtual :ref:`VarVec<doxid-dc/d39/class_models_1_1_base_1a5a6bc95969a38ac1ac68ab4a0ba94c75>` :ref:`getVars<doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1a9c141ce37428493b4fdfb0f55e63d303>`() const;
	};

	// direct descendants

	class :ref:`TraubMilesAlt<doxid-da/da2/class_neuron_models_1_1_traub_miles_alt>`;
	class :ref:`TraubMilesFast<doxid-d2/dfb/class_neuron_models_1_1_traub_miles_fast>`;
	class :ref:`TraubMilesNStep<doxid-db/dfd/class_neuron_models_1_1_traub_miles_n_step>`;

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

.. _details-d6/d4a/class_neuron_models_1_1_traub_miles:

Detailed Documentation
~~~~~~~~~~~~~~~~~~~~~~

Hodgkin-Huxley neurons with Traub & Miles algorithm.

This conductance based model has been taken from :ref:`Traub1991 <doxid-d0/de3/citelist_1CITEREF_Traub1991>` and can be described by the equations:

.. math::

	\begin{eqnarray*} C \frac{d V}{dt} &=& -I_{{\rm Na}} -I_K-I_{{\rm leak}}-I_M-I_{i,DC}-I_{i,{\rm syn}}-I_i, \\ I_{{\rm Na}}(t) &=& g_{{\rm Na}} m_i(t)^3 h_i(t)(V_i(t)-E_{{\rm Na}}) \\ I_{{\rm K}}(t) &=& g_{{\rm K}} n_i(t)^4(V_i(t)-E_{{\rm K}}) \\ \frac{dy(t)}{dt} &=& \alpha_y (V(t))(1-y(t))-\beta_y(V(t)) y(t), \end{eqnarray*}

where :math:`y_i= m, h, n`, and

.. math::

	\begin{eqnarray*} \alpha_n&=& 0.032(-50-V)/\big(\exp((-50-V)/5)-1\big) \\ \beta_n &=& 0.5\exp((-55-V)/40) \\ \alpha_m &=& 0.32(-52-V)/\big(\exp((-52-V)/4)-1\big) \\ \beta_m &=& 0.28(25+V)/\big(\exp((25+V)/5)-1\big) \\ \alpha_h &=& 0.128\exp((-48-V)/18) \\ \beta_h &=& 4/\big(\exp((-25-V)/5)+1\big). \end{eqnarray*}

and typical parameters are :math:`C=0.143` nF, :math:`g_{{\rm leak}}= 0.02672` :math:`\mu` S, :math:`E_{{\rm leak}}= -63.563` mV, :math:`g_{{\rm Na}}=7.15` :math:`\mu` S, :math:`E_{{\rm Na}}= 50` mV, :math:`g_{{\rm {\rm K}}}=1.43` :math:`\mu` S, :math:`E_{{\rm K}}= -95` mV.

It has 4 variables:

* ``V`` - membrane potential E

* ``m`` - probability for Na channel activation m

* ``h`` - probability for not Na channel blocking h

* ``n`` - probability for K channel activation n

and 7 parameters:

* ``gNa`` - Na conductance in 1/(mOhms \* cm^2)

* ``ENa`` - Na equi potential in mV

* ``gK`` - K conductance in 1/(mOhms \* cm^2)

* ``EK`` - K equi potential in mV

* ``gl`` - Leak conductance in 1/(mOhms \* cm^2)

* ``El`` - Leak equi potential in mV

* ``Cmem`` - Membrane capacity density in muF/cm^2

Internally, the ordinary differential equations defining the model are integrated with a linear Euler algorithm and GeNN integrates 25 internal time steps for each neuron for each network time step. I.e., if the network is simulated at ``DT= 0.1`` ms, then the neurons are integrated with a linear Euler algorithm with ``lDT= 0.004`` ms. This variant uses IF statements to check for a value at which a singularity would be hit. If so, value calculated by L'Hospital rule is used.

Methods
-------

.. index:: pair: function; getSimCode
.. _doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1a868f03fe6e30b947586cdb6a8d29146a:

.. ref-code-block:: cpp
	:class: doxyrest-title-code-block

	virtual std::string getSimCode() const

Gets the code that defines the execution of one timestep of integration of the neuron model.

The code will refer to  for the value of the variable with name "NN". It needs to refer to the predefined variable "ISYN", i.e. contain , if it is to receive input.

.. index:: pair: function; getThresholdConditionCode
.. _doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1afdc035bc1c4f29c57c589b05678d232f:

.. ref-code-block:: cpp
	:class: doxyrest-title-code-block

	virtual std::string getThresholdConditionCode() const

Gets code which defines the condition for a true spike in the described neuron model.

This evaluates to a bool (e.g. "V > 20").

.. index:: pair: function; getParamNames
.. _doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1a248a2c830b67bb80cb6dbbbda57f405d:

.. ref-code-block:: cpp
	:class: doxyrest-title-code-block

	virtual :ref:`StringVec<doxid-d6/df7/class_snippet_1_1_base_1a06cd0f6da1424a20163e12b6fec62519>` getParamNames() const

Gets names of of (independent) model parameters.

.. index:: pair: function; getVars
.. _doxid-d6/d4a/class_neuron_models_1_1_traub_miles_1a9c141ce37428493b4fdfb0f55e63d303:

.. ref-code-block:: cpp
	:class: doxyrest-title-code-block

	virtual :ref:`VarVec<doxid-dc/d39/class_models_1_1_base_1a5a6bc95969a38ac1ac68ab4a0ba94c75>` getVars() const

Gets names and types (as strings) of model variables.

