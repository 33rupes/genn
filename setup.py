import os
import sys
from copy import deepcopy
from platform import system

from pybind11.setup_helpers import Pybind11Extension, build_ext, WIN, MACOS
from setuptools import find_packages, setup

# Get CUDA path from environment variable - setting this up is a required CUDA post-install step
cuda_path = os.environ.get("CUDA_PATH")

# Is CUDA installed?
cuda_installed = cuda_path is not None and os.path.exists(cuda_path)

# Get OpenCL path from environment variable
opencl_path = os.environ.get("OPENCL_PATH")

# Is OpenCL installed
opencl_installed = opencl_path is not None and os.path.exists(opencl_path)

# Are we on Linux? 
# **NOTE** Pybind11Extension provides WIN and MAC
LINUX = system() == "Linux"

# Determine correct suffix for GeNN libraries
if WIN:
    genn_lib_suffix = "_Release_DLL"# "_Debug_DLL" if debug_build else "_Release_DLL"
else:
    genn_lib_suffix = "_dynamic"#"_dynamic_debug" if debug_build else "_dynamic"

genn_path = os.path.dirname(os.path.abspath(__file__))

pygenn_path = os.path.join(genn_path, "pygenn")
genn_wrapper_path = os.path.join(pygenn_path, "genn_wrapper")
genn_wrapper_src = os.path.join(genn_wrapper_path, "src")
genn_wrapper_include = os.path.join(genn_wrapper_path, "include")
genn_include = os.path.join(genn_path, "include", "genn", "genn")
genn_third_party_include = os.path.join(genn_path, "include", "genn", "third_party")

genn_extension_kwargs = {
    "include_dirs": [genn_include, genn_third_party_include, genn_wrapper_include],
    "library_dirs": [genn_wrapper_path],
    "libraries": ["genn" + genn_lib_suffix],
    "define_macros": [("LINKING_GENN_DLL", "1"), ("LINKING_BACKEND_DLL", "1")],
    "cxx_std": 17,
    "extra_link_args":[]}

# Always package LibGeNN
package_data = ["genn_wrapper/genn" + genn_lib_suffix + ".*" if WIN 
                else "genn_wrapper/libgenn" + genn_lib_suffix + ".*"]

# On Linux, we want to add extension directory i.e. $ORIGIN to runtime
# directories so libGeNN and backends can be found wherever package is installed
if LINUX:
    genn_extension_kwargs["runtime_library_dirs"] = ["$ORIGIN"]

# By default build single-threaded CPU backend
backends = []#[("single_threaded_cpu", "SingleThreadedCPU", {})]

# If CUDA was found, add backend configuration
if cuda_installed:
    # Get CUDA library directory
    if MACOS:
        cuda_library_dir = os.path.join(cuda_path, "lib")
    elif WIN:
        cuda_library_dir = os.path.join(cuda_path, "lib", "x64")
    else:
        cuda_library_dir = os.path.join(cuda_path, "lib64")

    # Add backend
    # **NOTE** on Mac OS X, a)runtime_library_dirs doesn't work b)setting rpath is required to find CUDA
    backends.append(("cuda", "CUDA",
                     {"libraries": ["cuda", "cudart"],
                      "include_dirs": [os.path.join(cuda_path, "include")],
                      "library_dirs": [cuda_library_dir],
                      "extra_link_args": ["-Wl,-rpath," + cuda_library_dir] if MACOS else []}))

ext_modules = [
    Pybind11Extension("genn",
                      [os.path.join(genn_wrapper_src, "genn.cc")],
                      **genn_extension_kwargs),
    Pybind11Extension("neuron_models",
                      [os.path.join(genn_wrapper_src, "neuronModels.cc")],
                      **genn_extension_kwargs)]
    
 # Loop through namespaces of supported backends
for filename, namespace, kwargs in backends:
    # Take a copy of the standard extension kwargs
    backend_extension_kwargs = deepcopy(genn_extension_kwargs)

    # Extend any settings specified by backend
    for n, v in kwargs.items():
        backend_extension_kwargs[n].extend(v)

    # Add relocatable version of backend library to libraries
    # **NOTE** this is added BEFORE libGeNN as this library needs symbols FROM libGeNN
    if WIN:
        package_data.append("genn_wrapper/genn_" + filename + "_backend" + genn_lib_suffix + ".*")
    else:
        package_data.append("genn_wrapper/libgenn_" + filename + "_backend" + genn_lib_suffix + ".*")

    # Add backend include directory to both SWIG and C++ compiler options
    backend_include_dir = os.path.join(genn_path, "include", "genn", "backends", filename)
    backend_extension_kwargs["libraries"].insert(0, "genn_" + filename + "_backend" + genn_lib_suffix)
    backend_extension_kwargs["include_dirs"].append(backend_include_dir)

    # Add extension to list
    ext_modules.append(Pybind11Extension(filename + "_backend", 
                                         [os.path.join(genn_wrapper_src, filename + "Backend.cc")],
                                         **backend_extension_kwargs))

setup(
    name="pygenn",
    version="0.0.1",
    packages = find_packages(),
    package_data={"pygenn": package_data},

    url="https://github.com/genn_team/genn",
    ext_package="pygenn.genn_wrapper",
    ext_modules=ext_modules,
    zip_safe=False,
    python_requires=">=3.6",
)
