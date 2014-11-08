#!/bin/bash
apt-cache search rdma
apt-get install gcc make cmake libtool autoconf automake linux-tool-common linux-headers-$(uname -r) ibverbs-utils libibverbs-dev libibverbs1 librdmacm-dev librdmacm1 rdmacm-utils
