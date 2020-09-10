# AlcorControlAgent
Next Generation Cloud Network Control Agent

# Summary
Source code folder:

- comm: Library for communication with Alcor controllers, includes gRPC and MQ implementation
- dhcp: DHCP server implementation
- dp_abstraction: Data plane abstraction layer implementation
- grpc: Auto generated source codes and library for gRPC interface with Alcor Controllers
- net_config: Library for configurating host networking components
- ovs: OVS data plane implementation
- proto3: Auto generated source codes and library for proto3 scheme for communication with Alcor Controllers
- test: Unit and integration test code

# Build and Execution Instructions using Dockerfile
Since the Alcor Control Agent relies on a few external dependencies, Dockerfile was used for fast build and test environment setup.

## Setting up a Development Environment
The Alcor Control Agent project currently uses cmake for building. It includes the Alcor controller submodule to consume the needed proto3 schemas and gRPC definitions.

To set up your local development environment, we recommend to use fork-and-branch git workflow.

1. Fork Alcor Control Agent Github repository by clicking the Fork button on the upper right-hand side of Alcor Control Agent home page.
2. Make a local clone:
    ```
    cd ~/dev
    $ git clone --recurse-submodules https://github.com/<your_github_username>/alcor-control-agent.git ~/alcor-control-agent
    $ cd ~/dev/alcor-control-agent
    $ git submodule update --init --recursive
    ```
3. Add a remote pointing back to the Alcor Official repository
    ```
    $ git remote add upstream https://github.com/futurewei-cloud/alcor-control-agent.git 
    ```
4. Always keep your forked repo (both local and remote) in sync with upstream. Try to run the following commands daily:
    ```
    $ git checkout master
    $ git pull upstream master
    $ git push origin master
    ```
5. Work in a feature branch
    ```
    $ git checkout -b <new_branch_name>
    $ ... (make changes in your branch)
    $ git add .
    & git commit -m "commit message"
    ```
6. Push changes to your remote fork
    ```
    $ git push origin <new_branch_name>
    ```
7. Open a Pull Request on Alcor home page, notify community on [Alcor Slack](https://alcor-networking.slack.com/) channels.
You will need approval from at least one maintainer, who will merge your codes to Alcor master.
8. Clean up after a merged Pull Request
    ```
    $ git checkout master
    $ git pull upstream master
    $ git push origin master
    $ git branch -d <branch_name>
    $ git push --delete origin <branch_name>
    ```

## Run the build script to set up the build container and compile the alcor-control-agent
Assuming alcor-control-agent was cloned into ~/dev/alcor-control-agent directory:
```Shell
cd ~/dev/alcor-control-agent
./build/build.sh
```

## You can also setup a physical machine or VM to compile the alcor-control-agent
Assuming alcor-control-agent was cloned into ~/dev/alcor-control-agent directory:
```Shell
cd ~/dev/alcor-control-agent
./build/aca-machine-init.sh
```

## Running alcor-control-agent and tests
You can run the test (optional):
```Shell
root@ca62b6feec63:/mnt/host/code/alcor-control-agent# ./build/tests/aca_tests
```

You should be ready to run the executable:
```Shell
root@ca62b6feec63:/mnt/host/code/alcor-control-agent# ./build/bin/AlcorControlAgent
```

# Build the container while behind a proxy

If the docker installation environment is behind proxy the Dockerfile.proxy file needs
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
