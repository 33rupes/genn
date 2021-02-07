FROM nvidia/cuda:11.0-devel

ENV CUDA_PATH=/usr/local/cuda
ENV GENN_PATH=/usr/local/genn
ENV PATH=$PATH:$CUDA_PATH/bin:$GENN_PATH/bin

WORKDIR /usr/local
RUN apt-get update && \
	apt-get install -y git && \
	git clone https://github.com/genn-team/genn $GENN_PATH

WORKDIR	$GENN_PATH
RUN make -j8 DYNAMIC=1 LIBRARY_DIRECTORY=`pwd`/pygenn/genn_wrapper/ && \
        #python setup.py develop && \
        apt-get autoremove --purge -y git

WORKDIR /
