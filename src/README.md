# AlcorControlAgent
Next Generation Cloud Network Control Agent

# Summary
Source code folder:

- Comm: Library for communication with network controllers and transit daemon
- grpc: Auto generated source codes and library for gRPC interface with Alcor Controllers
- net_config: Library for configurating host networking components
- proto3: Auto generated source codes and library for proto3 scheme for communication with Alcor Controllers
- transit_rpc: Library for RPC interface with transit daemon
- test: Unit and integration test code

# Build and Execution Instructions using Dockerfile
Since the Alcor Control Agent relies on a few external dependencies, Dockerfile was used for fast build and test environment setup.

## Cloning alcor-control-agent Repo
The Alcor Control Agent includes the Alcor controller and Transit submodules to consume the needed proto3 schemas and RPC definitions. Therefore, the below commands are needed when cloning:

```Shell
cd ~/dev
git clone --recurse-submodules https://github.com/futurewei-cloud/alcor-control-agent.git AlcorControlAgent
git submodule update --init --recursive
```

## Run the build script to set up the build container and compile the alcor-control-agent
Assuming alcor-control-agent was cloned into ~/dev/AlcorControlAgent directory:
```Shell
cd ~/dev/AlcorControlAgent
./build/build.sh
```
## Running alcor-control-agent and tests
You can run the test (optional):
```Shell
root@ca62b6feec63:/mnt/host/code/AlcorControlAgent# ./build/tests/aca_tests
```

You should be ready to run the executable:
```Shell
root@ca62b6feec63:/mnt/host/code/AlcorControlAgent# ./build/bin/AlcorControlAgent
```

# Build the container while behind a proxy

If the docker installation environment is behind proxy the Docker.proxy file needs
to be used to build the container.

The proxy imposes the following constrains to the build.
The following paragraphs explains the changes made in the Dockerfile

### The build environment _inside_ the container must be proxy aware.

To do this follow these steps.

1. $ mkdir -p /etc/systemd/system/docker.service.d
2. $ create and edit the http-proxy.conf to add the following content to it:
3. $ cat http-proxy.conf

```
[Service]
Environment="HTTP_PROXY=user:pwd@proxy.com:12345"
Environment="NO_PROXY=localhost,127.0.0.1"
```

After this reload systemd and restart docker.

1. systemctl daemon-reload
2. systemctl restart docker

### The container might not trust the github certificate

The error message is:

```
fatal: unable to access 'https://github.com/grpc/grpc/': server certificate
verification failed. CAfile: /etc/ssl/certs/ca-certificates.crt CRLfile: none
```

The solution is to modify the Dockerfile and add this line before downloading
the grpc package.

1. git config --global http.sslverify false

### git clone of grpc may fail

The error message is:

```
error: RPC failed; curl 56 GnuTLS recv error (-110): The TLS connection was
non-properly terminated.
```
This errors seems to happen on ubuntu hosts behind a proxy when the
package to download is quite big which is the case of grpc. By default
git on ubuntu installed by _apt install_ uses gnutls. For smaller packages this
error does not appear. Although there are several workarounds like increasing
the git http buffers they don't work with software of the size of grpc and the
fix is to rebuild git from sources and make it use the openssl
version of libcurl.

A new RUN sections have to be added to the Dockerfile to handle this case.
