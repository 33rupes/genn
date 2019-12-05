#pragma once

// Standard includes
#include <iomanip>
#include <limits>
#include <string>
#include <sstream>
#include <vector>

// GeNN includes
#include "models.h"
#include "snippet.h"
#include "variableMode.h"

// GeNN code generator includes
#include "codeStream.h"
#include "teeStream.h"

// Forward declarations
class ModelSpecInternal;
class SynapseGroupInternal;

namespace CodeGenerator
{
class NeuronGroupMerged;
class Substitutions;
class SynapseGroupMerged;
}

//--------------------------------------------------------------------------
// CodeGenerator
//--------------------------------------------------------------------------
namespace CodeGenerator
{
//--------------------------------------------------------------------------
// FunctionTemplate
//--------------------------------------------------------------------------
//! Immutable structure for specifying how to implement
//! a generic function e.g. gennrand_uniform
/*! **NOTE** for the sake of easy initialisation first two parameters of GenericFunction are repeated (C++17 fixes) */
struct FunctionTemplate
{
    // **HACK** while GCC and CLang automatically generate this fine/don't require it, VS2013 seems to need it
    FunctionTemplate operator = (const FunctionTemplate &o)
    {
        return FunctionTemplate{o.genericName, o.numArguments, o.doublePrecisionTemplate, o.singlePrecisionTemplate};
    }

    //! Generic name used to refer to function in user code
    const std::string genericName;

    //! Number of function arguments
    const unsigned int numArguments;

    //! The function template (for use with ::functionSubstitute) used when model uses double precision
    const std::string doublePrecisionTemplate;

    //! The function template (for use with ::functionSubstitute) used when model uses single precision
    const std::string singlePrecisionTemplate;
};

//--------------------------------------------------------------------------
//! \brief Tool for substituting strings in the neuron code strings or other templates
//--------------------------------------------------------------------------
void substitute(std::string &s, const std::string &trg, const std::string &rep);

//--------------------------------------------------------------------------
//! \brief Tool for substituting variable  names in the neuron code strings or other templates using regular expressions
//--------------------------------------------------------------------------
bool regexVarSubstitute(std::string &s, const std::string &trg, const std::string &rep);

//--------------------------------------------------------------------------
//! \brief Tool for substituting function names in the neuron code strings or other templates using regular expressions
//--------------------------------------------------------------------------
bool regexFuncSubstitute(std::string &s, const std::string &trg, const std::string &rep);

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
void functionSubstitute(std::string &code, const std::string &funcName,
                        unsigned int numParams, const std::string &replaceFuncTemplate);

//--------------------------------------------------------------------------
//! \brief This function writes a floating point value to a stream -setting the precision so no digits are lost
//--------------------------------------------------------------------------
template<class T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
void writePreciseString(std::ostream &os, T value)
{
    // Cache previous precision
    const std::streamsize previousPrecision = os.precision();

    // Set scientific formatting
    os << std::scientific;

    // Set precision to what is required to fully represent T
    os << std::setprecision(std::numeric_limits<T>::max_digits10);

    // Write value to stream
    os << value;

    // Reset to default formatting
    // **YUCK** GCC 4.8.X doesn't seem to include std::defaultfloat
    os.unsetf(std::ios_base::floatfield);
    //os << std::defaultfloat;

    // Restore previous precision
    os << std::setprecision(previousPrecision);
}

//--------------------------------------------------------------------------
//! \brief This function writes a floating point value to a string - setting the precision so no digits are lost
//--------------------------------------------------------------------------
template<class T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
std::string writePreciseString(T value)
{
    std::stringstream s;
    writePreciseString(s, value);
    return s.str();
}

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

void genMergedGroupSpikeCountReset(CodeStream &os, const NeuronGroupMerged &n);

template<typename T>
void genMergedGroupPush(CodeStream &os, const std::vector<T> &groups, const std::string &suffix)
{
    // Loop through merged neuron groups
    std::stringstream mergedGroupArrayStream;
    std::stringstream mergedGroupFuncStream;
    CodeStream mergedGroupArray(mergedGroupArrayStream);
    CodeStream mergedGroupFunc(mergedGroupFuncStream);
    TeeStream mergedGroupStreams(mergedGroupArray, mergedGroupFunc);
    for(const auto &g : groups) {
        // Declare static array to hold merged neuron groups
        const size_t idx = g.getIndex();
        const size_t numGroups = g.getGroups().size();

        // **TODO** backend-specific
        mergedGroupArray << "__device__ __constant__ Merged" << suffix << "Group" << idx << " dd_merged" << suffix << "Group" << idx << "[" << numGroups << "];" << std::endl;

        // Write function to update
        mergedGroupFunc << "void pushMerged" << suffix << "Group" << idx << "ToDevice(const Merged" << suffix << "Group" << idx << " *group)";
        {
            CodeStream::Scope b(mergedGroupFunc);
            // **TODO** backend-specific
            mergedGroupFunc << "CHECK_CUDA_ERRORS(cudaMemcpyToSymbol(dd_merged" << suffix << "Group" << idx << ", group, ";
            mergedGroupFunc << numGroups << " * sizeof(Merged" << suffix << "Group" << idx << ")));" << std::endl;
        }
    }

    if(!groups.empty()) {
        os << "// ------------------------------------------------------------------------" << std::endl;
        os << "// merged group arrays" << std::endl;
        os << "// ------------------------------------------------------------------------" << std::endl;
        os << mergedGroupArrayStream.str();
        os << std::endl;

        os << "// ------------------------------------------------------------------------" << std::endl;
        os << "// merged group functions" << std::endl;
        os << "// ------------------------------------------------------------------------" << std::endl;
        os << mergedGroupFuncStream.str();
        os << std::endl;
    }
}

//--------------------------------------------------------------------------
/*! \brief This function implements a parser that converts any floating point constant in a code snippet to a floating point constant with an explicit precision (by appending "f" or removing it).
 */
//--------------------------------------------------------------------------
std::string ensureFtype(const std::string &oldcode, const std::string &type);


//--------------------------------------------------------------------------
/*! \brief This function checks for unknown variable definitions and returns a gennError if any are found
 */
//--------------------------------------------------------------------------
void checkUnreplacedVariables(const std::string &code, const std::string &codeName);

void preNeuronSubstitutionsInSynapticCode(
    Substitutions &substitutions,
    const SynapseGroupInternal &sg,
    const std::string &offset,
    const std::string &axonalDelayOffset,
    const std::string &postIdx,
    const std::string &preVarPrefix = "",    //!< prefix to be used for presynaptic variable accesses - typically combined with suffix to wrap in function call such as __ldg(&XXX)
    const std::string &preVarSuffix = "");   //!< suffix to be used for presynaptic variable accesses - typically combined with prefix to wrap in function call such as __ldg(&XXX)

void postNeuronSubstitutionsInSynapticCode(
    Substitutions &substitutions,
    const SynapseGroupInternal &sg,
    const std::string &offset,
    const std::string &backPropDelayOffset,
    const std::string &preIdx,
    const std::string &postVarPrefix = "",   //!< prefix to be used for postsynaptic variable accesses - typically combined with suffix to wrap in function call such as __ldg(&XXX)
    const std::string &postVarSuffix = "");  //!< suffix to be used for postsynaptic variable accesses - typically combined with prefix to wrap in function call such as __ldg(&XXX)

//-------------------------------------------------------------------------
/*!
  \brief Function for performing the code and value substitutions necessary to insert neuron related variables, parameters, and extraGlobal parameters into synaptic code.
*/
//-------------------------------------------------------------------------
void neuronSubstitutionsInSynapticCode(
    Substitutions &substitutions,
    const SynapseGroupInternal &sg,             //!< the synapse group connecting the pre and postsynaptic neuron populations whose parameters might need to be substituted
    const std::string &preIdx,               //!< index of the pre-synaptic neuron to be accessed for _pre variables; differs for different Span)
    const std::string &postIdx,              //!< index of the post-synaptic neuron to be accessed for _post variables; differs for different Span)
    double dt,                          //!< simulation timestep (ms)
    const std::string &preVarPrefix = "",    //!< prefix to be used for presynaptic variable accesses - typically combined with suffix to wrap in function call such as __ldg(&XXX)
    const std::string &preVarSuffix = "",    //!< suffix to be used for presynaptic variable accesses - typically combined with prefix to wrap in function call such as __ldg(&XXX)
    const std::string &postVarPrefix = "",   //!< prefix to be used for postsynaptic variable accesses - typically combined with suffix to wrap in function call such as __ldg(&XXX)
    const std::string &postVarSuffix = "");  //!< suffix to be used for postsynaptic variable accesses - typically combined with prefix to wrap in function call such as __ldg(&XXX)
}   // namespace CodeGenerator
