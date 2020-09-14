#include "gennUtils.h"

// Standard C++ includes
#include <algorithm>
#include <numeric>

// GeNN includes
#include "models.h"

namespace
{
//--------------------------------------------------------------------------
// GenericFunction
//--------------------------------------------------------------------------
//! Immutable structure for specifying the name and number of
//! arguments of a generic funcion e.g. gennrand_uniform
struct GenericFunction
{
    //! Generic name used to refer to function in user code
    const std::string genericName;

    //! Number of function arguments
    const unsigned int numArguments;
};


GenericFunction randomFuncs[] = {
    {"gennrand_uniform", 0},
    {"gennrand_normal", 0},
    {"gennrand_exponential", 0},
    {"gennrand_log_normal", 2},
    {"gennrand_gamma", 1}
};
}

//--------------------------------------------------------------------------
// Utils
//--------------------------------------------------------------------------
namespace Utils
{
bool isRNGRequired(const std::string &code)
{
    // Loop through random functions
    for(const auto &r : randomFuncs) {
        // If this function takes no arguments, return true if
        // generic function name enclosed in $() markers is found
        if(r.numArguments == 0) {
            if(code.find("$(" + r.genericName + ")") != std::string::npos) {
                return true;
            }
        }
        // Otherwise, return true if generic function name
        // prefixed by $( and suffixed with comma is found
        else if(code.find("$(" + r.genericName + ",") != std::string::npos) {
            return true;
        }
    }
    return false;

}
//--------------------------------------------------------------------------
bool isRNGRequired(const std::vector<Models::VarInit> &varInitialisers)
{
    // Return true if any of these variable initialisers require an RNG
    return std::any_of(varInitialisers.cbegin(), varInitialisers.cend(),
                       [](const Models::VarInit &varInit)
                       {
                           return isRNGRequired(varInit.getSnippet()->getCode());
                       });
}
//--------------------------------------------------------------------------
bool isTypePointer(const std::string &type)
{
    return (type.back() == '*');
}
//--------------------------------------------------------------------------
bool isTypePointerToPointer(const std::string &type)
{
    const size_t len = type.length();
    return (type[len - 1] == '*' && type[len - 2] == '*');
}
//--------------------------------------------------------------------------
std::string getUnderlyingType(const std::string &type)
{
    // Check that type is a pointer type
    assert(isTypePointer(type));

    // if type is actually a pointer to a pointer, return string without last 2 characters
    if(isTypePointerToPointer(type)) {
        return type.substr(0, type.length() - 2);
    }
    // Otherwise, return string without last character
    else {
        return type.substr(0, type.length() - 1);
    }
}
//--------------------------------------------------------------------------
size_t getFlattenedKernelSize(const std::vector<unsigned int> &size)
{
    return std::accumulate(size.cbegin(), size.cend(), 1, std::multiplies<unsigned int>());
}
//--------------------------------------------------------------------------
std::vector<size_t> getKernelDimensionality(const std::vector<unsigned int> &size)
{
    // Loop through dimensions
    std::vector<size_t> dimensionality;
    for(size_t i = 0; i < size.size(); i++) {
        // If size in this dimension is greater than one, add index to vector
        if(size[i] > 1) {
            dimensionality.push_back(i);
        }
    }
    return dimensionality;
}
}   // namespace utils
