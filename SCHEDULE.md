- Basic Functionality (P0 7/31)
    - Able to attach transit agent to the corresponding vNIC(veth)
    - Able to attach transit switch/router on the physical NIC
    - Able to create VPC/Subnet/Endpoint and send traffic between the endpoints on the new dataplane
    - listen to messages from network controller through Kafka
    - deserialize protobuf messages
    - program the transit switch/router/endpoint through transit daemon
    - functional test, UT, E2E
  
- Scale out/in

- Transit Placement

- Disaster recovery (P0 8/30)
    - NCA gets error message and do local recovery
    - VPC-level recovery
