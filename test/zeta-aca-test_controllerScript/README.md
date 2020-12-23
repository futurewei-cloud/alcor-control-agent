# Pseudo Controller for Zeta/ACA Integration Tests

The pseudo controller is a python script that acts as a controller between Zeta and ACA during the integration tests, and it does the following:

- Creates Zeta resources(zgc, vpc, nodes, ports, etc) by calling the Zeta Northbound APIs.
- Sends the data needed for ACA to construct its GoalState to the ACA nodes.
- Calls different test cases, constructs the GoalStates do some validation.
- If the above steps are all successful, the psedu controller will execute a ping command from the parent node to the child node, in order to verify the results of the previous steps.

## Usage

There are now two ways to use the controller:

1. To create only two ports (by default) and run the tests DISABLED_zeta_gateway_path_CHILD and DISABLED_zeta_gateway_path_PARENT, user only need to run:

```sh
python3 run.py
```

2. To create up to 1,000,000 port and run the tests DISABLED_zeta_scale_CHILD and DISABLED_zeta_scale_PARENT, with flags to control how many ports to create in total, how many ports each /ports POST call should generate(up to 4,000 ports), and how much time the pseudo controller sleeps after each call, the user should run:

```sh
python3 run.py total_ports_to_create number_of_ports_each_call amount_of_time_to_sleep.
```

## Known Issues

1. It is suggested that the total amount of ports to create not to exceed 10,000, the reason for this is, after the data is sent to the aca nodes and the gtest are called, because of the large amount of data to process for the ACA(thus with longer time needed and more output will be produced), the pseudo controller will be 'frozen', and users may have to stop the controller by `control + c`.
Please note that, even if the pseudo controller is 'frozen', it doesn't mean the gtests failed. You can execute the gtests on the aca nodes manually, and the gtests should run and you should see the results.
