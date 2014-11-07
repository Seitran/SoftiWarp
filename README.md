<p>SoftiWARP: Software iWARP kernel driver and user library for Linux.</p>

<p>Overview
On servers handling heavy network traffic, a transport protocol stack with support for remote direct memory access (RDMA) can eliminate a bottleneck in network input/output (I/O) by avoiding data copies between the operating system and application buffers. The Internet Engineering Task Force (IETF) has defined a set of protocols for remote direct data placement over IP networks. The RDMA Consortium (RDMAC) has defined the semantics of an interface to an RDMA-capable network interface card (RNIC), the so-called RDMA protocol verbs. The IETF’s RDMA protocol stack, also known as the iWARP transport, is implemented on RNICs or, more generally, by verbs providers. The InfiniBand Trade Association (IBTA) is defining another transport providing RDMA services.
OS extensions and programming interfaces for RDMA represent a significant portion of the RDMA infrastructure, and their appropriate design is a key requirement for the success of RDMA technology.</p>

<p>Remote direct memory access</p>

<p>Within the Interconnect Software Consortium (ICSC) of The Open Group, we contributed to the standardization of RDMA-enabled programming interfaces, co-chairing both the Interconnect Transport API (IT-API) and the RNIC Programming Interface (RNICPI) work groups. We helped defining a modular, layered, and transport-neutral host software architecture for RDMA through contributions to an industry-driven Linux open-source project called OpenRDMA.</p>

<p>In this context, we have implemented a host software architecture for RDMA that provides the operating system (OS) integration for both iWARP and InfiniBand, supporting IT-API and an enhanced version of RNICPI. A key property of such an architecture is a clean separation of generic/OS functionality and verbs-provider-specific software functionality into user/kernel Access Layer (uAL/kAL) and user/kernel Verbs Provider (uVP/kVP) components, respectively. This approach permits a wide range of RNICs / verbs providers to register themselves through a standard programming interface and minimizes code bloat by keeping generic functionality such as OS-wide RDMA resource management, event handling and connection management in a single, OS-provided implementation. As a prototype, we designed a verbs provider called SoftRDMA, a pure software implementation of the IETF’s iWARP (RDMAP/DDP/MPA) protocol stack.</p>

<p>As a broadly supported industry effort, the OpenFabrics Alliance (OFA) develops, distributes and promotes an open-source software stack for RDMA-capable adapters and RDMA transports including InfiniBand and iWARP. Because OpenFabrics provides RDMA support in the Linux kernel and already supports a wide range of RDMA devices as well as RDMA-enabled upper-layer protocols, we are currently developing a fully software-based iWARP Linux driver called Soft-iWARP, which fits into the OFA RDMA environment. The outcome of our work will be a device driver exporting the OFA RDMA verbs and connection manager interfaces. The Soft-iWARP kVP implements the iWARP protocols on top of kernel TCP sockets. It provides standards-compliant iWARP RDMA functionality at a decent performance level. All basic RDMA operations (RDMA resource and connection managment, asynchronous work request posting and completion, Send/Receive as well as RDMA Write and Read operations) are implemented and functioning. We support user-level applications through an OFA-compliant uVP library.</p>

<p>A software-based iWARP stack that runs at reasonable performance levels and seamlessly fits into the OFA RDMA environment provides several benefits:
As a generic (RNIC-independent) iWARP device driver, it immediately enables RDMA services on all systems with conventional Ethernet adapters, which do not provide RDMA hardware support.</p>

<p>Soft-iWARP can be an intermediate step when migrating applications and systems to RDMA APIs and OpenFabrics. Soft-iWARP can be a reasonable solution for client systems, allowing RNIC-equipped peers/servers to enjoy the full benefits of RDMA communication.
Soft-iWARP seamlessly supports direct as well as asynchronous transmission with multiple outstanding work requests and RDMA operations.</p>

<p>A software-based iWARP stack may flexibly employ any available hardware assists for performance-critical operations such as MPA CRC checksum calculation and direct data placement. The resulting performance levels may approach those of a fully offloaded iWARP stack.</p>

<p>Besides contributing towards a more standardized RDMA ecosystem, we analyze the applicability of RDMA. We identify hidden costs in the setup of its interactions that, if not handled carefully, remove any performance advantage.</p>

<p>Package Including:
SoftiWARP Kernel:
SoftiWARP (siw) implements the iWARP protocol suite (MPA/DDP/RDMAP,
IETF-RFC 5044/5041/5040) completely in software as a Linux kernel module.
Targeted for integration with OpenFabrics (OFA) interfaces, it appears as
a kernel module in the drivers/infiniband/hw subdirectory of the Linux kernel.
SoftiWARP exports the OFA RDMA verbs interface, currently useable only
for user level applications. It makes use of the OFA connection manager
to set up connections. siw runs on top of TCP kernel sockets.</p>

<p>libsiw implements user level access to the siw kernel module using libibverbs and librdmacm.</p>

<p>softiwarp-userlib:
siw user library implementing access to siw kernel module using openfabrics rdma verbs</p>
