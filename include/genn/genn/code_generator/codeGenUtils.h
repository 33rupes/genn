#pragma once

// Standard includes
#include <algorithm>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>

// GeNN includes
#include "gennExport.h"
#include "gennUtils.h"
#include "neuronGroupInternal.h"
#include "variableMode.h"

// GeNN code generator includes
#include "backendBase.h"
#include "codeStream.h"
#include "substitutions.h"
#include "teeStream.h"

//--------------------------------------------------------------------------
// CodeGenerator
//--------------------------------------------------------------------------
namespace CodeGenerator
{
//--------------------------------------------------------------------------
//! \brief Tool for substituting strings in the neuron code strings or other templates
//--------------------------------------------------------------------------
GENN_EXPORT void substitute(std::string &s, const std::string &trg, const std::string &rep);

//--------------------------------------------------------------------------
/*! \brief Tool for substituting strings in the neuron code strings or other templates 
 * using 'lazy' evaluation - getRep is only called if 'trg' is found
 */
//--------------------------------------------------------------------------
template<typename V>
void substituteLazy(std::string &s, const std::string &trg, V getRepFn)
{
    size_t found= s.find(trg);
    while (found != std::string::npos) {
        s.replace(found,trg.length(), getRepFn());
        found= s.find(trg);
    }
}

//--------------------------------------------------------------------------
//! \brief Tool for substituting variable  names in the neuron code strings or other templates using regular expressions
//--------------------------------------------------------------------------
GENN_EXPORT bool regexVarSubstitute(std::string &s, const std::string &trg, const std::string &rep);

//--------------------------------------------------------------------------
//! \brief Tool for substituting function names in the neuron code strings or other templates using regular expressions
//--------------------------------------------------------------------------
GENN_EXPORT bool regexFuncSubstitute(std::string &s, const std::string &trg, const std::string &rep);

//--------------------------------------------------------------------------
/*! \brief This function substitutes function calls in the form:
 *
 *  $(functionName, parameter1, param2Function(0.12, "string"))
 *
 * with replacement templates in the form:
 *
 *  actualFunction(CONSTANT, $(0), $(1))
 *
 */
//--------------------------------------------------------------------------
GENN_EXPORT void functionSubstitute(std::string &code, const std::string &funcName,
                                    unsigned int numParams, const std::string &replaceFuncTemplate);

//! Divide two integers, rounding up i.e. effectively taking ceil
inline size_t ceilDivide(size_t numerator, size_t denominator)
{
    return ((numerator + denominator - 1) / denominator);
}

//! Pad an integer to a multiple of another
inline size_t padSize(size_t size, size_t blockSize)
{
    return ceilDivide(size, blockSize) * blockSize;
}

GENN_EXPORT void genTypeRange(CodeStream &os, const std::string &precision, const std::string &prefix);

//--------------------------------------------------------------------------
/*! \brief This function implements a parser that converts any floating point constant in a code snippet to a floating point constant with an explicit precision (by appending "f" or removing it).
 */
//--------------------------------------------------------------------------
GENN_EXPORT std::string ensureFtype(const std::string &oldcode, const std::string &type);


//--------------------------------------------------------------------------
/*! \brief This function checks for unknown variable definitions and returns a gennError if any are found
 */
//--------------------------------------------------------------------------
GENN_EXPORT void checkUnreplacedVariables(const std::string &code, const std::string &codeName);

//--------------------------------------------------------------------------
/*! \brief This function substitutes function names in a code with namespace as prefix of the function name for backends that do not support namespaces by checking that the function indeed exists in the support code and returns the substituted code.
 */
 //--------------------------------------------------------------------------
GENN_EXPORT std::string disambiguateNamespaceFunction(const std::string supportCode, const std::string code, std::string namespaceName);

//-------------------------------------------------------------------------
/*!
  \brief Function for performing the code and value substitutions necessary to insert neuron related variables, parameters, and extraGlobal parameters into synaptic code.
*/
//-------------------------------------------------------------------------
template<typename P, typename D>
void neuronSubstitutionsInSynapticCode(CodeGenerator::Substitutions &substitutions, const NeuronGroupInternal *archetypeNG, 
                                       const std::string &offset, const std::string &delayOffset, const std::string &idx, 
                                       const std::string &sourceSuffix, const std::string &destSuffix, 
                                       const std::string &varPrefix, const std::string &varSuffix,
                                       P getParamValueFn, D getDerivedParamValueFn)
{

    // Substitute spike times
    substitutions.addVarSubstitution("sT" + sourceSuffix,
                                     "(" + delayOffset + varPrefix + "group->sT" + destSuffix + "[" + offset + idx + "]" + varSuffix + ")");

    // Substitute neuron variables
    const auto *nm = archetypeNG->getNeuronModel();
    for(const auto &v : nm->getVars()) {
        const std::string varIdx = archetypeNG->isVarQueueRequired(v.name) ? offset + idx : idx;

        substitutions.addVarSubstitution(v.name + sourceSuffix,
                                         varPrefix + "group->" + v.name + destSuffix + "[" + varIdx + "]" + varSuffix);
    }

    // Substitute (potentially heterogeneous) parameters and derived parameters from neuron model
    substitutions.addParamValueSubstitution(nm->getParamNames(), getParamValueFn, sourceSuffix);
    substitutions.addVarValueSubstitution(nm->getDerivedParams(), getDerivedParamValueFn, sourceSuffix);

    // Substitute extra global parameters from neuron model
    substitutions.addVarNameSubstitution(nm->getExtraGlobalParams(), sourceSuffix, "group->", destSuffix);
}

template<typename G>
void genKernelIndex(std::ostream &os, const CodeGenerator::Substitutions &subs, G &sg)
{
    // Loop through kernel dimensions to calculate array index
    const auto &kernelSize = sg.getArchetype().getKernelSize();
    for(size_t i = 0; i < kernelSize.size(); i++) {
        os << "(" << subs["id_kernel_" + std::to_string(i)];
        // Loop through remainining dimensions of kernel
        for(size_t j = i + 1; j < kernelSize.size(); j++) {
            os << " * group->kernelSize" << sg.getKernelSize(j);
        }
        os << ")";

        // If this isn't the last dimension, add +
        if(i != (kernelSize.size() - 1)) {
            os << " + ";
        }
    }
}
}   // namespace CodeGenerator
