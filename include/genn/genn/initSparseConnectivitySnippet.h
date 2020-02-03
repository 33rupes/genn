#pragma once

// Standard C++ includes
#include <functional>
#include <vector>

// Standard C includes
#include <cassert>
#include <cmath>

// GeNN includes
#include "binomial.h"
#include "snippet.h"

//----------------------------------------------------------------------------
// Macros
//----------------------------------------------------------------------------
#define SET_ROW_BUILD_CODE(CODE) virtual std::string getRowBuildCode() const override{ return CODE; }
#define SET_ROW_BUILD_STATE_VARS(...) virtual ParamValVec getRowBuildStateVars() const override{ return __VA_ARGS__; }

#define SET_HOST_INIT_CODE(CODE) virtual std::string getHostInitCode() const override{ return CODE; }

#define SET_CALC_MAX_ROW_LENGTH_FUNC(FUNC) virtual CalcMaxLengthFunc getCalcMaxRowLengthFunc() const override{ return FUNC; }
#define SET_CALC_MAX_COL_LENGTH_FUNC(FUNC) virtual CalcMaxLengthFunc getCalcMaxColLengthFunc() const override{ return FUNC; }

#define SET_MAX_ROW_LENGTH(MAX_ROW_LENGTH) virtual CalcMaxLengthFunc getCalcMaxRowLengthFunc() const override{ return [](unsigned int, unsigned int, const std::vector<double> &){ return MAX_ROW_LENGTH; }; }
#define SET_MAX_COL_LENGTH(MAX_COL_LENGTH) virtual CalcMaxLengthFunc getCalcMaxColLengthFunc() const override{ return [](unsigned int, unsigned int, const std::vector<double> &){ return MAX_COL_LENGTH; }; }

#define SET_EXTRA_GLOBAL_PARAMS(...) virtual EGPVec getExtraGlobalParams() const override{ return __VA_ARGS__; }

//----------------------------------------------------------------------------
// InitSparseConnectivitySnippet::Base
//----------------------------------------------------------------------------
//! Base class for all sparse connectivity initialisation snippets
namespace InitSparseConnectivitySnippet
{
class GENN_EXPORT Base : public Snippet::Base
{
public:
    //----------------------------------------------------------------------------
    // Typedefines
    //----------------------------------------------------------------------------
    typedef std::function<unsigned int(unsigned int, unsigned int, const std::vector<double> &)> CalcMaxLengthFunc;
    
    //----------------------------------------------------------------------------
    // Declared virtuals
    //----------------------------------------------------------------------------
    virtual std::string getRowBuildCode() const{ return ""; }
    virtual ParamValVec getRowBuildStateVars() const{ return {}; }

    virtual std::string getHostInitCode() const{ return ""; }

    //! Get function to calculate the maximum row length of this connector based on the parameters and the size of the pre and postsynaptic population
    virtual CalcMaxLengthFunc getCalcMaxRowLengthFunc() const{ return CalcMaxLengthFunc(); }

    //! Get function to calculate the maximum column length of this connector based on the parameters and the size of the pre and postsynaptic population
    virtual CalcMaxLengthFunc getCalcMaxColLengthFunc() const{ return CalcMaxLengthFunc(); }

    //! Gets names and types (as strings) of additional
    //! per-population parameters for the connection initialisation snippet
    virtual EGPVec getExtraGlobalParams() const{ return {}; }

    //------------------------------------------------------------------------
    // Public methods
    //------------------------------------------------------------------------
    //! Find the index of a named extra global parameter
    size_t getExtraGlobalParamIndex(const std::string &paramName) const
    {
        return getNamedVecIndex(paramName, getExtraGlobalParams());
    }

    //! Can this neuron model be merged with other? i.e. can they be simulated using same generated code
    bool canBeMerged(const Base *other) const;
};

//----------------------------------------------------------------------------
// Init
//----------------------------------------------------------------------------
class Init : public Snippet::Init<InitSparseConnectivitySnippet::Base>
{
public:
    Init(const Base *snippet, const std::vector<double> &params)
        : Snippet::Init<Base>(snippet, params)
    {
    }

    bool canBeMerged(const Init &other) const
    {
        return Snippet::Init<Base>::canBeMerged(other, getSnippet()->getRowBuildCode());
    }
};

//----------------------------------------------------------------------------
// InitSparseConnectivitySnippet::Uninitialised
//----------------------------------------------------------------------------
//! Used to mark connectivity as uninitialised - no initialisation code will be run
class Uninitialised : public Base
{
public:
    DECLARE_SNIPPET(InitSparseConnectivitySnippet::Uninitialised, 0);
};

//----------------------------------------------------------------------------
// InitSparseConnectivitySnippet::OneToOne
//----------------------------------------------------------------------------
//! Initialises connectivity to a 'one-to-one' diagonal matrix
class OneToOne : public Base
{
public:
    DECLARE_SNIPPET(InitSparseConnectivitySnippet::OneToOne, 0);

    SET_ROW_BUILD_CODE(
        "$(addSynapse, $(id_pre));\n"
        "$(endRow);\n");

    SET_MAX_ROW_LENGTH(1);
    SET_MAX_COL_LENGTH(1);
};

//----------------------------------------------------------------------------
// InitSparseConnectivitySnippet::FixedProbabilityBase
//----------------------------------------------------------------------------
//! Base class for snippets which initialise connectivity with a fixed probability
//! of a synapse existing between a pair of pre and postsynaptic neurons.
class FixedProbabilityBase : public Base
{
public:
    virtual std::string getRowBuildCode() const override = 0;

    SET_ROW_BUILD_STATE_VARS({{"prevJ", "int", -1}});

    SET_PARAM_NAMES({"prob"});
    SET_DERIVED_PARAMS({{"probLogRecip", [](const std::vector<double> &pars, double){ return 1.0 / log(1.0 - pars[0]); }}});

    SET_CALC_MAX_ROW_LENGTH_FUNC(
        [](unsigned int numPre, unsigned int numPost, const std::vector<double> &pars)
        {
            // Calculate suitable quantile for 0.9999 change when drawing numPre times
            const double quantile = pow(0.9999, 1.0 / (double)numPre);

            return binomialInverseCDF(quantile, numPost, pars[0]);
        });
    SET_CALC_MAX_COL_LENGTH_FUNC(
        [](unsigned int numPre, unsigned int numPost, const std::vector<double> &pars)
        {
            // Calculate suitable quantile for 0.9999 change when drawing numPos times
            const double quantile = pow(0.9999, 1.0 / (double)numPost);

            return binomialInverseCDF(quantile, numPre, pars[0]);
        });
};

//----------------------------------------------------------------------------
// InitSparseConnectivitySnippet::FixedProbability
//----------------------------------------------------------------------------
//! Initialises connectivity with a fixed probability of a synapse existing
//! between a pair of pre and postsynaptic neurons.
/*! Whether a synapse exists between a pair of pre and a postsynaptic
    neurons can be modelled using a Bernoulli distribution. While this COULD
    br sampling directly by repeatedly drawing from the uniform distribution,
    this is innefficient. Instead we sample from the gemetric distribution
    which describes "the probability distribution of the number of Bernoulli
    trials needed to get one success" -- essentially the distribution of the
    'gaps' between synapses. We do this using the "inversion method"
    described by Devroye (1986) -- essentially inverting the CDF of the
    equivalent continuous distribution (in this case the exponential distribution)*/
class FixedProbability : public FixedProbabilityBase
{
public:
    DECLARE_SNIPPET(InitSparseConnectivitySnippet::FixedProbability, 1);

    SET_ROW_BUILD_CODE(
        "const scalar u = $(gennrand_uniform);\n"
        "prevJ += (1 + (int)(log(u) * $(probLogRecip)));\n"
        "if(prevJ < $(num_post)) {\n"
        "   $(addSynapse, prevJ + $(id_post_begin));\n"
        "}\n"
        "else {\n"
        "   $(endRow);\n"
        "}\n");
};

//----------------------------------------------------------------------------
// InitSparseConnectivitySnippet::FixedProbabilityNoAutapse
//----------------------------------------------------------------------------
//! Initialises connectivity with a fixed probability of a synapse existing
//! between a pair of pre and postsynaptic neurons. This version ensures there
//! are no autapses - connections between neurons with the same id
//! so should be used for recurrent connections.
/*! Whether a synapse exists between a pair of pre and a postsynaptic
    neurons can be modelled using a Bernoulli distribution. While this COULD
    br sampling directly by repeatedly drawing from the uniform distribution, 
    this is innefficient. Instead we sample from the gemetric distribution 
    which describes "the probability distribution of the number of Bernoulli 
    trials needed to get one success" -- essentially the distribution of the 
    'gaps' between synapses. We do this using the "inversion method"
    described by Devroye (1986) -- essentially inverting the CDF of the
    equivalent continuous distribution (in this case the exponential distribution)*/
class FixedProbabilityNoAutapse : public FixedProbabilityBase
{
public:
    DECLARE_SNIPPET(InitSparseConnectivitySnippet::FixedProbabilityNoAutapse, 1);

    SET_ROW_BUILD_CODE(
        "int nextJ;\n"
        "do {\n"
        "   const scalar u = $(gennrand_uniform);\n"
        "   nextJ = prevJ + (1 + (int)(log(u) * $(probLogRecip)));\n"
        "} while(nextJ == $(id_pre));\n"
        "prevJ = nextJ;\n"
        "if(prevJ < $(num_post)) {\n"
        "   $(addSynapse, prevJ + $(id_post_begin));\n"
        "}\n"
        "else {\n"
        "   $(endRow);\n"
        "}\n");
};

//----------------------------------------------------------------------------
// InitSparseConnectivitySnippet::FixedNumberPostWithReplacement
//----------------------------------------------------------------------------
//! Initialises connectivity with a fixed number of random synapses per row.
/*! The postsynaptic targets of the synapses can be initialised in parallel by sampling from the discrete
    uniform distribution. However, to sample connections in ascending order, we sample from the 1st order statistic
    of the uniform distribution -- Beta[1, Npost] -- essentially the next smallest value. In this special case
    this is equivalent to the exponential distribution which can be sampled in constant time using the inversion method.*/
class FixedNumberPostWithReplacement : public Base
{
public:
    DECLARE_SNIPPET(InitSparseConnectivitySnippet::FixedNumberPostWithReplacement, 1);

    SET_ROW_BUILD_CODE(
        "if(c == 0) {\n"
        "   $(endRow);\n"
        "}\n"
        "const scalar u = $(gennrand_uniform);\n"
        "x += (1.0 - x) * (1.0 - pow(u, 1.0 / (scalar)c));\n"
        "unsigned int postIdx = (unsigned int)(x * $(num_post));\n"
        "postIdx = (postIdx < $(num_post)) ? postIdx : ($(num_post) - 1);\n"
        "$(addSynapse, postIdx + $(id_post_begin));\n"
        "c--;\n");
    SET_ROW_BUILD_STATE_VARS({{"x", "scalar", 0.0},{"c", "unsigned int", "$(rowLength)"}});

    SET_PARAM_NAMES({"rowLength"});

    SET_CALC_MAX_ROW_LENGTH_FUNC(
        [](unsigned int, unsigned int, const std::vector<double> &pars)
        {
            return (unsigned int)pars[0];
        });

    SET_CALC_MAX_COL_LENGTH_FUNC(
        [](unsigned int numPre, unsigned int numPost, const std::vector<double> &pars)
        {
            // Calculate suitable quantile for 0.9999 change when drawing numPost times
            const double quantile = pow(0.9999, 1.0 / (double)numPost);

            // In each row the number of connections that end up in a column are distributed
            // binomially distributed with n=numConnections and p=1.0 / numPost. As there are numPre rows the total number
            // of connections that end up in each column are distributed binomially with n=numConnections * numPre and p=1.0 / numPost
            return binomialInverseCDF(quantile, (unsigned int)pars[0] * numPre, 1.0 / (double)numPost);
        });
};

//----------------------------------------------------------------------------
// InitSparseConnectivitySnippet::FixedNumberTotalWithReplacement
//----------------------------------------------------------------------------
//! Initialises connectivity with a total number of random synapses.
//! The first stage in using this connectivity is to determine how many of the total synapses end up in each row.
//! This can be determined by sampling from the multinomial distribution. However, this operation cannot be
//! efficiently parallelised so must be performed on the host and the result passed as an extra global parameter array.
/*! Once the length of each row is determined, the postsynaptic targets of the synapses can be initialised in parallel
    by sampling from the discrete uniform distribution. However, to sample connections in ascending order, we sample
    from the 1st order statistic of the uniform distribution -- Beta[1, Npost] -- essentially the next smallest value.
    In this special case this is equivalent to the exponential distribution which can be sampled in constant time using the inversion method.*/
class FixedNumberTotalWithReplacement : public Base
{
public:
    DECLARE_SNIPPET(InitSparseConnectivitySnippet::FixedNumberTotalWithReplacement, 1);

    SET_ROW_BUILD_CODE(
        "if(c == 0) {\n"
        "   $(endRow);\n"
        "}\n"
        "const scalar u = $(gennrand_uniform);\n"
        "x += (1.0 - x) * (1.0 - pow(u, 1.0 / (scalar)c));\n"
        "unsigned int postIdx = (unsigned int)(x * $(num_post));\n"
        "postIdx = (postIdx < $(num_post)) ? postIdx : ($(num_post) - 1);\n"
        "$(addSynapse, postIdx + $(id_post_begin));\n"
        "c--;\n");
    SET_ROW_BUILD_STATE_VARS({{"x", "scalar", 0.0},{"c", "unsigned int", "$(preCalcRowLength)[($(id_pre) * $(num_threads)) + $(id_thread)]"}});

    SET_PARAM_NAMES({"total"});
    SET_EXTRA_GLOBAL_PARAMS({{"preCalcRowLength", "uint16_t*"}})

    SET_HOST_INIT_CODE(
        "// Allocate pre-calculated row length array\n"
    	"$(allocatepreCalcRowLength, $(num_pre) * $(num_threads));\n"
        "// Calculate row lengths\n"
        "const size_t numPostPerThread = ($(num_post) + $(num_threads) - 1) / $(num_threads);\n"
        "const size_t leftOverNeurons = $(num_post) % numPostPerThread;\n"
        "size_t remainingConnections = $(total);\n"
        "size_t matrixSize = (size_t)$(num_pre) * (size_t)$(num_post);\n"
        "uint16_t *subRowLengths = $(preCalcRowLength);\n"
        "// Loop through rows\n"
        "for(size_t i = 0; i < $(num_pre); i++) {\n"
        "    const bool lastPre = (i == ($(num_pre) - 1));\n"
        "    // Loop through subrows\n"
        "    for(size_t j = 0; j < $(num_threads); j++) {\n"
        "        const bool lastSubRow = (j == ($(num_threads) - 1));\n"
        "        // If this isn't the last sub-row of the matrix\n"
        "        if(!lastPre || ! lastSubRow) {\n"
        "            // Get length of this subrow\n"
        "            const unsigned int numSubRowNeurons = (leftOverNeurons != 0 && lastSubRow) ? leftOverNeurons : numPostPerThread;\n"
        "            // Calculate probability\n"
        "            const double probability = (double)numSubRowNeurons / (double)matrixSize;\n"
        "            // Create distribution to sample row length\n"
        "            std::binomial_distribution<size_t> rowLengthDist(remainingConnections, probability);\n"
        "            // Sample row length;\n"
        "            const size_t subRowLength = rowLengthDist($(rng));\n"
        "            // Update counters\n"
        "            remainingConnections -= subRowLength;\n"
        "            matrixSize -= numSubRowNeurons;\n"
        "            // Add row length to array\n"
        "            assert(subRowLength < std::numeric_limits<uint16_t>::max());\n"
        "            *subRowLengths++ = (uint16_t)subRowLength;\n"
        "        }\n"
        "    }\n"
        "}\n"
        "// Insert remaining connections into last sub-row\n"
        "*subRowLengths = (uint16_t)remainingConnections;\n"
        "// Push populated row length array\n"
        "$(pushpreCalcRowLength, $(num_pre) * $(num_threads));\n");

    SET_CALC_MAX_ROW_LENGTH_FUNC(
        [](unsigned int numPre, unsigned int numPost, const std::vector<double> &pars)
        {
            // Calculate suitable quantile for 0.9999 change when drawing numPre times
            const double quantile = pow(0.9999, 1.0 / (double)numPre);

            // There are numConnections connections amongst the numPre*numPost possible connections.
            // Each of the numConnections connections has an independent p=float(numPost)/(numPre*numPost)
            // probability of being selected and the number of synapses in the sub-row is binomially distributed
            return binomialInverseCDF(quantile, (unsigned int)pars[0], (double)numPost / ((double)numPre * (double)numPost));
        });

    SET_CALC_MAX_COL_LENGTH_FUNC(
        [](unsigned int numPre, unsigned int numPost, const std::vector<double> &pars)
        {
            // Calculate suitable quantile for 0.9999 change when drawing numPost times
            const double quantile = pow(0.9999, 1.0 / (double)numPost);

            // There are numConnections connections amongst the numPre*numPost possible connections.
            // Each of the numConnections connections has an independent p=float(numPre)/(numPre*numPost)
            // probability of being selected and the number of synapses in the sub-row is binomially distributed
            return binomialInverseCDF(quantile, (unsigned int)pars[0], (double)numPre / ((double)numPre * (double)numPost));
        });
};
}   // namespace InitVarSnippet
