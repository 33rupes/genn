/*--------------------------------------------------------------------------
   Author: Thomas Nowotny
  
   Institute: Center for Computational Neuroscience and Robotics
              University of Sussex
	      Falmer, Brighton BN1 9QJ, UK 
  
   email to:  T.Nowotny@sussex.ac.uk
  
   initial version: 2010-02-07
  
--------------------------------------------------------------------------*/

/*! \file generateALL.cpp
  \brief Main file combining the code for code generation. Part of the code generation section.

  The file includes separate files for generating kernels (generateKernels.cc),
  generating the CPU side code for running simulations on either the CPU or GPU (generateRunner.cc) and for CPU-only simulation code (generateCPU.cc).
*/

#include "global.h"
//#include "utils.h"

#include "currentModel.cc"

#include "generateKernels.cc"
#include "generateRunner.cc"
#include "generateCPU.cc"

#ifdef _WIN32
#include <direct.h>
#include <stdlib.h>
#else
#include <sys/stat.h> // needed for mkdir
#endif

/*! \brief This function will call the necessary sub-functions to generate the code for simulating a model. */

void generate_model_runner(NNmodel &model,  //!< Model description
			   string path      //!< Path where the generated code will be deposited
			   )
{
#ifdef _WIN32
  _mkdir((path + "\\" + model.name + "_CODE").c_str());
#else // UNIX
  mkdir((path + "/" + model.name + "_CODE").c_str(), 0777); 
#endif

  // general shared code for GPU and CPU versions
  genRunner(model, path, cerr);

  // GPU specific code generation
  genRunnerGPU(model, path, cerr);
  
  // generate neuron kernels
  genNeuronKernel(model, path, cerr);

  // generate synapse and learning kernels
  if (model.synapseGrpN > 0) genSynapseKernel(model, path, cerr);

  // CPU specific code generation
  genRunnerCPU(model, path, cerr);

  // Generate the equivalent of neuron kernel
  genNeuronFunction(model, path, cerr);
  
  // Generate the equivalent of synapse and learning kernel
  if (model.synapseGrpN > 0) genSynapseFunction(model, path, cerr);
}


//--------------------------------------------------------------------------
/*! 
  \brief Helper function that prepares data structures and detects the hardware properties to enable the code generation code that follows.

  The main tasks in this function are the detection and characterization of the GPU device present (if any), choosing which GPU device to use, finding and appropriate block size, taking note of the major and minor version of the CUDA enabled device chosen for use, and populating the list of standard neuron models. The chosen device number is returned.
*/
//--------------------------------------------------------------------------

int chooseDevice(ostream &mos,   //!< output stream for messages
		 NNmodel *&model, //!< the nn model we are generating code for
		 string path     //!< path the generated code will be deposited
		 )
{
  // Get the specifications of all available cuda devices, then work out which one we will use.
  int deviceCount, chosenDevice = 0;
  size_t globalMem, mostGlobalMem = 0;
  CHECK_CUDA_ERRORS(cudaGetDeviceCount(&deviceCount));
  deviceProp = new cudaDeviceProp[deviceCount];

  if (optimiseBlockSize) { // IF OPTIMISATION IS ON: Choose the device which supports the highest warp occupancy.
    mos << "optimizing block size..." << endl;

    char buffer[256];
    stringstream command, ptxInfo;
    string kernelName, junk;
    int reqRegs, reqSmem, requiredBlocks, ptxInfoFound = 0;

    unsigned int *bestSynBlkSz= new unsigned int[deviceCount];
    unsigned int *bestLrnBlkSz= new unsigned int[deviceCount];
    unsigned int *bestNrnBlkSz= new unsigned int[deviceCount];
    unsigned int **blockSizePtr;
    vector<unsigned int> *groupSizePtr;
    float blockLimit, mainBlockLimit, bestOccupancy;
    int deviceOccupancy, bestDeviceOccupancy = 0, smallModel = 0;
    int devstart, devcount;

    if (model->chooseGPUDevice == AUTODEVICE) {
      devstart= 0; 
      devcount= deviceCount;
    }
    else {
      devstart= model->chooseGPUDevice;
      devcount= devstart+1;
    }
    for (int device = devstart; device < devcount; device++) {
      theDev = device;
      CHECK_CUDA_ERRORS(cudaSetDevice(device));
      CHECK_CUDA_ERRORS(cudaGetDeviceProperties(&(deviceProp[device]), device));      
      generate_model_runner(*model, path);
      bestSynBlkSz[device] = 0;
      bestLrnBlkSz[device] = 0;
      bestNrnBlkSz[device] = 0;

      // Run NVCC and pipe output to this process.
      mos << "dry-run compile for device " << device << endl;
      command.str("");
      command << string(NVCC) << " -x cu -cubin -Xptxas=-v -arch=sm_" << deviceProp[device].major;
      command << deviceProp[device].minor << " -DDT -D\"CHECK_CUDA_ERRORS(call){call;}\" ";
      command << path << "/" << (*model).name << "_CODE/runner.cc 2>&1";
      mos << command.str() << endl;

#ifdef _WIN32
      FILE *nvccPipe = _popen(command.str().c_str(), "r");
#else // UNIX
      FILE *nvccPipe = popen(command.str().c_str(), "r");
#endif

      if (!nvccPipe) {
	mos << "ERROR: failed to open nvcc pipe" << endl;
	exit(EXIT_FAILURE);
      }

      // Get the sizes of each synapse / learn / neuron group present on this host and device
      vector<unsigned int> synapseN, learnN;
      for (int group = 0; group < (*model).synapseGrpN; group++) {
	if ((*model).synapseConnType[group] == SPARSE) {
	  synapseN.push_back((*model).maxConn[group]);
	}
	else {
	  synapseN.push_back((*model).neuronN[(*model).synapseTarget[group]]);
	}
	if ((*model).synapseType[group] == LEARN1SYNAPSE) {
	  learnN.push_back((*model).neuronN[(*model).synapseSource[group]]);
	}
      }

      // This data is required for block size optimisation, but cannot be found in deviceProp.
      float warpAllocGran, regAllocGran, smemAllocGran, maxBlocksPerSM;
      if (deviceProp[device].major == 1) {
	smemAllocGran = 512;
	warpAllocGran = 2;
	regAllocGran = (deviceProp[device].minor < 2) ? 256 : 512;
	maxBlocksPerSM = 8;
      }
      else if (deviceProp[device].major == 2) {
	smemAllocGran = 128;
	warpAllocGran = 2;
	regAllocGran = 128;
	maxBlocksPerSM = 8;
      }
      else if (deviceProp[device].major == 3) {
	smemAllocGran = 256;
	warpAllocGran = 4;
	regAllocGran = 256;
	maxBlocksPerSM = 16;
      }
      else {
	cerr << "Error: unsupported CUDA device major version: " << deviceProp[device].major << endl;
	exit(EXIT_FAILURE);
      }

      // Read pipe until reg / smem usage is found, then calculate optimum block size for each kernel.
      while (fgets(buffer, 256, nvccPipe) != NULL) {
	if (strstr(buffer, "error:") != NULL) {
	  cout << buffer;
	}
	else if (strstr(buffer, "calcSynapses") != NULL) {
	  kernelName = "synapse";
	  blockSizePtr = &bestSynBlkSz;
	  groupSizePtr = &synapseN;
	}
	else if (strstr(buffer, "learnSynapses") != NULL) {
	  kernelName = "learn";
	  blockSizePtr = &bestLrnBlkSz;
	  groupSizePtr = &learnN;
	}
	else if (strstr(buffer, "calcNeurons") != NULL) {
	  kernelName = "neuron";
	  blockSizePtr = &bestNrnBlkSz;
	  groupSizePtr = &(*model).neuronN;
	}
	if (strncmp(buffer, "ptxas info    : Used", 20) == 0) {
	  ptxInfoFound = 1;
	  bestOccupancy = 0;
	  ptxInfo.str("");
	  ptxInfo << buffer;
	  ptxInfo >> junk >> junk >> junk >> junk >> reqRegs >> junk >> reqSmem;
	  mos << "kernel: " << kernelName << ", regs needed: " << reqRegs << ", smem needed: " << reqSmem << endl;

	  // Test all block sizes (in warps) up to [max warps per block].
	  for (int blockSize = 1; blockSize <= deviceProp[device].maxThreadsPerBlock / 32; blockSize++) {

	    // BLOCK LIMIT DUE TO THREADS
	    blockLimit = floor(deviceProp[device].maxThreadsPerMultiProcessor / 32 / blockSize);
	    if (blockLimit > maxBlocksPerSM) blockLimit = maxBlocksPerSM;
	    mainBlockLimit = blockLimit;

	    // BLOCK LIMIT DUE TO REGISTERS
	    if (deviceProp[device].major == 1) { // if register allocation is per block
	      blockLimit = ceil(blockSize / warpAllocGran) * warpAllocGran;
	      blockLimit = ceil(blockLimit * reqRegs * 32 / regAllocGran) * regAllocGran;
	      blockLimit = floor(deviceProp[device].regsPerBlock / blockLimit);
	    }
	    else { // if register allocation is per warp
	      blockLimit = ceil(reqRegs * 32 / regAllocGran) * regAllocGran;
	      blockLimit = floor(deviceProp[device].regsPerBlock / blockLimit / warpAllocGran) * warpAllocGran;
	      blockLimit = floor(blockLimit / blockSize);
	    }
	    if (blockLimit < mainBlockLimit) mainBlockLimit = blockLimit;

	    // BLOCK LIMIT DUE TO SHARED MEMORY
	    blockLimit = ceil(reqSmem / smemAllocGran) * smemAllocGran;
	    blockLimit = floor(deviceProp[device].sharedMemPerBlock / blockLimit);
	    if (blockLimit < mainBlockLimit) mainBlockLimit = blockLimit;

	    // The number of [blockSize] padded thread blocks required to simulate all groups
	    requiredBlocks = 0;
	    for (int group = 0; group < groupSizePtr->size(); group++) {
	      requiredBlocks += ceil((float) (*groupSizePtr)[group] / (float) (blockSize * 32));
	    }

	    // Use a small block size if it allows all groups to occupy the device concurrently
	    if (requiredBlocks <= (mainBlockLimit * deviceProp[device].multiProcessorCount)) {
	      smallModel = 1;
	      globalMem = deviceProp[device].totalGlobalMem;
	      if (globalMem >= mostGlobalMem) {
		mostGlobalMem = globalMem;
		chosenDevice = device;
		(*blockSizePtr)[device] = (unsigned int) blockSize * 32;
		bestDeviceOccupancy = blockSize * mainBlockLimit;
	      }
	      break;
	    }

	    // Update the best warp occupancy and the block size which enables it.
	    if ((!smallModel) && ((blockSize * mainBlockLimit) > bestOccupancy)) {
	      bestOccupancy = blockSize * mainBlockLimit;
	      (*blockSizePtr)[device] = (unsigned int) blockSize * 32;

	      // Choose this device and set optimal block sizes if it enables higher neuron kernel occupancy.
	      if (blockSizePtr == &bestNrnBlkSz) {
		deviceOccupancy = bestOccupancy;
		if (deviceOccupancy >= bestDeviceOccupancy) {
		  bestDeviceOccupancy = deviceOccupancy;
		  chosenDevice = device;
		}
	      }
	    }
	  }
	}
      }

      // Close the NVCC pipe after each invocation.
#ifdef _WIN32
      _pclose(nvccPipe);
#else // UNIX
      pclose(nvccPipe);
#endif

    }

    if (!ptxInfoFound) {
      mos << "ERROR: did not find any PTX info" << endl;
      mos << "ensure nvcc is on your $PATH, and fix any NVCC errors listed above" << endl;
      exit(EXIT_FAILURE);
    }

    synapseBlkSz = bestSynBlkSz[chosenDevice];
    learnBlkSz = bestLrnBlkSz[chosenDevice];
    neuronBlkSz = bestNrnBlkSz[chosenDevice];
    delete model;
    model = new NNmodel();
    modelDefinition(*model);
    mos << "Using device " << chosenDevice << ", with up to " << bestDeviceOccupancy << " warps of neuron kernel occupancy per SM." << endl;
    delete[] bestSynBlkSz;
    delete[] bestLrnBlkSz;
    delete[] bestNrnBlkSz;
  }

  else { // IF OPTIMISATION IS OFF: Simply choose the device with the most global memory.
    mos << "skipping block size optimisation..." << endl;
    for (int device = 0; device < deviceCount; device++) {
      CHECK_CUDA_ERRORS(cudaSetDevice(device));
      CHECK_CUDA_ERRORS(cudaGetDeviceProperties(&(deviceProp[device]), device));
      globalMem = deviceProp[device].totalGlobalMem;
      if (globalMem >= mostGlobalMem) {
	mostGlobalMem = globalMem;
	chosenDevice = device;
      }
    }
    mos << "Using device " << chosenDevice << ", which has " << mostGlobalMem << " bytes of global memory." << endl;
  }

  ofstream sm_os("sm_Version.mk");
  sm_os << "NVCCFLAGS += -arch sm_" << deviceProp[chosenDevice].major << deviceProp[chosenDevice].minor << endl;
  sm_os.close();

  mos << "synapse block size: " << synapseBlkSz << endl;
  mos << "learn block size: " << learnBlkSz << endl;
  mos << "neuron block size: " << neuronBlkSz << endl;

  return chosenDevice;
}

/*! \brief Main entry point for the generateALL executable that generates
  the code for GPU and CPU.

  The main function is the entry point for the code generation engine. It 
  prepares the system and then invokes generate_model_runner to inititate
  the different parts of actual code generation.
*/

int main(int argc,     //!< number of arguments; expected to be 2
	 char *argv[]  //!< Arguments; expected to contain the target directory for code generation.
)
{
  if (argc != 2) {
    cerr << "usage: generateALL <target dir>" << endl;
    exit(EXIT_FAILURE);
  }
  cerr << "call was ";
  for (int i= 0; i < argc; i++) {
    cerr << argv[i] << " ";
  }
  cerr << endl;
  
  synapseBlkSz = 256;
  learnBlkSz = 256;
  neuronBlkSz = 256;
  
  NNmodel *model = new NNmodel();
  prepareStandardModels();
  preparePostSynModels();
  modelDefinition(*model);
  string path= toString(argv[1]);
  theDev = chooseDevice(cerr, model, path);
  generate_model_runner(*model, path);
  
  return EXIT_SUCCESS;
}
