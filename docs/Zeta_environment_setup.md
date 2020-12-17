## Zeta+ACA environment setup and test cases

### 1. Experimental topology

![](images/Zeta_environment_setup.JPG)

<p>Figure 1. Experimental topology</p>

#### 1.1 Gateway node

-   ZGC 1:
    -   gateway node1 &nbsp;&nbsp; ip: 172.16.62.247
    -   gateway node2 &nbsp;&nbsp; ip: 172.16.62.248

#### 1.2 Computer node

-   computer node1 &nbsp;&nbsp; ip: 172.16.62.249
-   computer node2 &nbsp;&nbsp; ip: 172.16.62.250
-   ACA on these two computer nodes has been configured.


### 2. Setup workflow

#### Step 1: REST API - ZGC information and gateway nodes initialize

-   call REST API to create ZGC 

-   call REST API to add VPC and get zgc entry points info

-   call REST API to notify ZGC the ports created on each ACA

#### Step 2: gtest - ACA test cases initialize

-   Use gtest to configure gateway path, and add a openflow rule on both computer nodes to receive oam packets
-   The steps of gtest about Zeta mainly have the following steps:
    -   delete the old br-int and br-tun, and create new br-int and br-tun, and patch ports between br-int and br-tun;
    -   create a port and the information of this port should be the same as that posted to ZGC by REST API;
    -   add the OAM punt rule to receive oam packets from gateway;
    -   add group entry configure gateway path;
    -   the ports on the two computing nodes begin to communicate. 
- Observe whether the gateway path is successful through "n_packet";

#### Step 3: First packet upload to gateways by default

#### Step 4: First packet forwarded to destination by gateawy

#### Step 5: Gateways reply OAM packet

#### Step 6: Create direct path

### Test Cases

#### Test Case 1: Create ZGC

- The controller uses ZGC REST APIs to create a zeta gateway cluster.
- 
    Request: http://172.16.62.247/zgcs

        data:
        {
            "name": "ZGC_test",
            "description": "ZGC_test",
            "cidr": "192.168.0.0/28",
            "port_ibo": "8300",
            "overlay_type": "vxlan"
        }

    Response:

        data:
        {
            "id": "f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
            "name": "ZGC_test",
            "description": "ZGC 1",
            "ip_start": "192.168.0.2",
            "ip_end": "192.168.0.4",
            "port_ibo": "8300",
            "overlay_type": "vxlan",
            "vpcs": []
        }

- The controller receives the response from Zeta management and determine whether the response code is 200. Then the controller gets information about the ZGC just created by zgc id to determine whether the ZGC is created successfully.
  
    Request: http://172.16.62.247/zgcs/f81d4fae-7dec-11d0-a765-00a0c91e6bf6
    Response:

        data:
        {
            "zgc_id": "f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
            "name": "ZGC_test1",
            "description": "ZGC 1",
            "ip_start": "192.168.0.2",
            "ip_end": "192.168.0.11",
            "port_ibo": "8300",
            "overlay_type": "vxlan",
            "nodes": [
                "111d4fae-7dec-11d0-a765-00a0c9345612",
                "111d4fae-7dec-11d0-a765-00a0c9345999",
                "111d4fae-7dec-11d0-a765-00a0c9345777"
            ],
            "vpcs": [
                "3dda2801-d675-4688-a63f-dcda8d327f50",
                "3ddffee1-cf55-7788-a63f-dcda8d582f45"
            ]
        }

#### Test Case 2: Add VPC

- The controller adds a VPC to the Zeta management.
  
    Method: POST
    Request: http://172.16.62.247/vpcs

        data:
        {
            "vpc_id": "3dda2801-d675-4688-a63f-dcda8d327f50",
            "vni": "1"
        }

    Reponse:

        data:
        {
            "vpc_id": "3dda2801-d675-4688-a63f-dcda8d327f50",
            "vni": "1",
            "zgc_id": "f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
            "name": "ZGC_test",
            "gws": 
            [
            {
                "ip": "192.168.0.2",
                "mac": "37.02.ff.cc.65.87"
            },
            {
                "ip": "192.168.0.3",
                "mac": "37.02.ff.cc.65.88"
            }
            ],
            "port_ibo": "8300"
        }

- The controller receive the response code and determine whether the response code is 201.

#### Test Case 3: Add Compute Instance Port

- The controller add the information of computer instance ports which will be created on computer nodes to the Zeta management plane.

    Method: POST
    Request: http://172.16.62.247:8080/ports

        data:
        [
            {
                "port_id": "333d4fae-7dec-11d0-a765-00a0c9342222",
                "vpc_id": "3dda2801-d675-4688-a63f-dcda8d327f50",
                "ips_port": 
                [
                    {
                        "ip": "10.10.0.3",
                        "vip": ""
                    }
                ],
                "mac_port": "cc:dd:ee:ff:11:22",
                "ip_node": "172.16.62.249",
                "mac_node": "00:1b:21:26:0a:37",
            },
            {
                "port_id": "99976feae-7dec-11d0-a765-00a0c9341111",
                "vpc_id": "3dda2801-d675-4688-a63f-dcda8d327f50",
                "ips_port": 
                [
                    {
                        "ip": "10.10.0.4",
                        "vip": ""
                    }
                ],
                "mac_port": "6c:dd:ee:ff:11:32",
                "ip_node": "172.16.62.250",
                "mac_node": "36:31:03:3f:62:74",
            }
        ]

- The controller receive the response code and determine whether the response code is 201. If it's not 201, this operation fails.

#### Test Case 4: Gateway Path Connectivity

- Add a TEST function about Zeta, and recompile ACA on both computer nodes.
- The controller sends the port information that needs to be created to ACA on computer nodes by transferring files.
- Execute gtest command on both computer nodes. 
  
    On computer node 172.16.62.250, run

        ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_2_ports_ZETA_test_traffic_CHILD -c 172.16.62.249
  
    On computer node 172.16.62.249, run

        ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_2_ports_ZETA_test_traffic_PARENT -p 172.16.62.250

- Observe whether the gateway path is successful through "n_packet"


#### Test Case 5: Direct Path Connectivity

TBD

