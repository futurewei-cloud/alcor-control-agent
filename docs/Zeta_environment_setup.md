## Zeta+ACA environment setup and test cases

### 1. Experimental topology

![](images/Zeta_environment_setup.JPG)

<p>Figure 1. Experimental topology</p>

#### 1.1 Gateway node

-   ZGC 1:
    -   gateway node1 &nbsp;&nbsp; ip: 172.16.62.247
    -   gateway node2 &nbsp;&nbsp; ip: 172.16.62.248

#### 1.2 Computer node

-   computer node1 &nbsp;&nbsp; ip: 172.16.62.240
-   computer node2 &nbsp;&nbsp; ip: 172.16.62.250
-   ACA on these two computer nodes has been configured.


### 2. Setup workflow

#### Step 1: REST API - ZGC information and gateway nodes initialize

-   call REST API to create ZGC 
-   call REST API to add VPC and get zgc entry points info
-   call REST API to notify ZGC the ports created on each ACA

#### Step 2: gtest - ACA test cases initialize

-   use gtest create br-int, br-tun, patch ports and create ports filled with vpc-id, port-name and other information

#### Step 3: Firest packet upload to gateways by default

#### Step 4: Firest packet forwarded to destination by gateawy

#### Step 5: Gateways reply OAM packet

#### Step 6: Create direct path