# Pseudo Controller for Zeta/ACA Integration Tests

The pseudo controller is a python script that acts as a controller between Zeta and ACA during the integration tests, and it does the following:

- Creates Zeta resources(zgc, vpc, nodes, ports, etc) by calling the Zeta Northbound APIs.
- Sends the data needed for ACA to construct its GoalState to the ACA nodes.
- Calls test case DISABLED_zeta_scale_container, constructs the GoalStates do some validation, and start Busybox container(s), each of which has one zeta port.
- If the above steps are all successful, the pseudo controller will execute 3 ping commands from the containers on the parent node to the containers on the child node, before it executes another 3 ping commands from containers on the child node to containers on the parent node, in order to verify the results of the previous steps.

## Usage

There are now two ways to use the controller:

1. To create up to 1,000,000 port and run the tests DISABLED_zeta_scale_container, with flags to control how many ports to create in total, how many ports each /ports POST call should generate(up to 4,000 ports), how much time the pseudo controller sleeps after each call, and how many ports to send to the aca nodes(this also equals the total # of containers that the aca nodes will create) the user should run:

```sh
python3 run.py total_ports_to_create number_of_ports_each_call amount_of_time_to_sleep amount_of_ports_to_send_to_aca
```

2. If no parameters are specified, 2 ports will be created and be sent to the aca nodes, after which the ping will be performed between these two ports.

## Known Issues

1. It is suggested that the total amount of ports to send to aca not to exceed 10, the reason for this is, after the data is sent to the aca nodes and the gtest are called, because of the large amount of data to process for the ACA(thus with longer time needed and more output will be produced), the pseudo controller will be 'frozen', and users may have to stop the controller by `control + c`.
Please note that, even if the pseudo controller is 'frozen', it doesn't mean the gtests failed. You can execute the gtests on the aca nodes manually, and the gtests should run and you should see the results.
