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
  
    Method: POST
    Request: http://172.16.62.247/zgcs

        data:
        {
            "name": "ZGC_test",
            "description": "ZGC 1",
            "cidr": "192.168.0.0/28",
            "port_ibo": "8300",
            "overlay_type": "vxlan"
        }

    Response:

        data:
        {
            "id": "zgc1zgc1-xxxx-xxxx-xxxx-xxxx",
            "name": "ZGC_test",
            "description": "ZGC 1",
            "ip_start": "192.168.0.2",
            "ip_end": "192.168.0.4",
            "port_ibo": "8300",
            "overlay_type": "vxlan",
            "vpcs": []
        }

-   call REST API to add VPC and get zgc entry points info

    Method: POST
    Request: http://172.16.62.247/vpcs

        data:
        {
            "vpc_id": "vpc1vpc1-xxxx-xxxx-xxxx-xxxx",
            "vni": "1"
        }

    Reponse:

        data:
        {
            "vpc_id": "vpc1vpc1-xxxx-xxxx-xxxx-xxxx",
            "vni": "1",
            "zgc_id": "zgc1zgc1-xxxx-xxxx-xxxx-xxxx",
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
            },
            {
                "ip": "192.168.0.4",
                "mac": "37.02.ff.cc.65.89"
             }
            ],
            "port_ibo": "8300"
        }

-   call REST API to notify ZGC the ports created on each ACA

    Method: POST
    Request: http://172.16.62.247:8080/ports

        data:
        [
            {
                "port_id": "333d4fae-7dec-11d0-a765-00a0c9342222",
                "vpc_id": "vpc1vpc1-xxxx-xxxx-xxxx-xxxx",
                "ips_port": 
                [
                    {
                        "ip": "172.16.62.247",
                        "vip": "192.168.1.2"
                    }
                ],
                "mac_port": "cc:dd:ee:ff:11:22",
                "ip_node": "192.168.10.27",
                "mac_node": "ee:dd:ee:ff:22:11",
            },
            {
                "port_id": "99976feae-7dec-11d0-a765-00a0c9341111",
                "vpc_id": vpc1vpc1-xxxx-xxxx-xxxx-xxxx",
                "ips_port": 
                [
                    {
                        "ip": "10.10.0.3",
                        "vip": "192.168.1.3"
                    }
                ],
                "mac_port": "6c:dd:ee:ff:11:32",
                "ip_node": "192.168.10.33",
                "mac_node": "ee:dd:ee:ff:33:11",
            }
        ]

#### Step 2: gtest - ACA test cases initialize

-   Use gtest create br-int, br-tun, patch ports, create ports filled with vpc-id, port-name and configure gateway path
-   The steps of gtest about Zeta mainly have the following steps:
    -   delete the old br-int and br-tun, and create new br-int and br-tun, and patch ports between br-int and br-tun
    -   create a port and the information of this port should be the same as that posted to ZGC by REST API
    -   add a rule in table 0, its function is to jump to table 2 when the packet comes from "patch-int"
    -   add a rule in table 2, its function is to jump to table 22  
    -   create group entries according to ZGC info received from ZGC
    -   add a rule in table 22, its function is to jump to the corresponding group table entry according to vlan id


#### Step 3: Firest packet upload to gateways by default

#### Step 4: Firest packet forwarded to destination by gateawy

#### Step 5: Gateways reply OAM packet

#### Step 6: Create direct path