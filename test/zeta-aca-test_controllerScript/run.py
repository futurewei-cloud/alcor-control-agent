import requests
import json
import paramiko
from collections import defaultdict, OrderedDict
import time
import sys
import itertools
from math import ceil

# zeta_data = None
server_aca_repo_path = ''
aca_data_destination_path = '/test/gtest/aca_data.json'
aca_data_local_path = './aca_data.json'

ips_ports_ip_prefix = "10."
mac_port_prefix = "6c:dd:ee:"
port_api_upper_limit = 4000

# Transfer the file locally to aca nodes


def upload_file_aca(host, user, password, server_path, local_path, timeout=10):
    """
    :param host
    :param user
    :param password
    :param server_path: /root/alcor-control-agent/test/gtest
    :param local_path: ./text.txt
    :param timeout
    :return: bool
    """
    try:
        for host_ip in host:
            t = paramiko.Transport((host_ip, 22))
            t.banner_timeout = timeout
            t.connect(username=user, password=password)
            sftp = paramiko.SFTPClient.from_transport(t)
            sftp.put(local_path, server_path)
            t.close()
        return True
    except Exception as e:
        print(e)
        return False


# Execute remote SSH commands
def exec_sshCommand_aca(host, user, password, cmd, timeout=60):
    """
    :param host
    :param user
    :param password
    :param cmd
    :param seconds
    :return: dict
    """
    result = {'status': [], 'data': [], 'error': False}  # Record return result
    try:
        # Create a SSHClient instance
        ssh = paramiko.SSHClient()
        ssh.banner_timeout = timeout
        # set host key
        ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        # Connect to remote server
        ssh.connect(host, 22, user, password, timeout=timeout)
        for command in cmd:
            # execute command
            print(f'executing command: {command}')
            stdin, stdout, stderr = ssh.exec_command(
                command, get_pty=True, timeout=timeout)
            # If need password
            if 'sudo' in command:
                stdin.write(password + '\n')
            # result of execution,return a list
            # out1 = stdout.readlines()
            out2 = stdout.read()
            # execution state:0 means success,1 means failure
            channel = stdout.channel
            status = channel.recv_exit_status()
            result['status'].append(status)
            # result['data'].append(out1)
            print(f'Output: {out2.decode()}')
        ssh.close()  # close ssh connection
        return result
    except Exception as e:
        print(f'Exception when executing command:{e}')
        result['error'] = True
        return result


def talk_to_zeta(file_path, zgc_api_url, zeta_data):
    headers = {'Content-type': 'application/json'}
    # create ZGC
    ZGC_data = zeta_data["ZGC_data"]
    print(f'ZGC_data: \n{ZGC_data}')
    zgc_response = requests.post(
        zgc_api_url + "/zgcs", data=json.dumps(ZGC_data), headers=headers)
    print(f'zgc creation response: \n{zgc_response}')
    zgc_id = zgc_response.json()['zgc_id']

    # add Nodes
    for node in zeta_data["NODE_data"]:
        node_data = node
        node_data['zgc_id'] = zgc_id
        print(f'node_data: \n{node_data}')
        node_response_data = requests.post(
            zgc_api_url + "/nodes", data=json.dumps(node_data), headers=headers)
        print(f'Response for adding node: {node_response_data.text}')

    json_content_for_aca = dict()
    json_content_for_aca['vpc_response'] = {}
    json_content_for_aca['port_response'] = {}

    # first delay
    print('Sleep 10 seconds after the Nodes call')
    time.sleep(10)

    # add VPC
    for item in zeta_data["VPC_data"]:
        VPC_data = item
        print(f'VPC_data: \n{VPC_data}')
        vpc_response_data = requests.post(
            zgc_api_url + "/vpcs", data=json.dumps(VPC_data), headers=headers).json()
        print(f'Response for adding VPC: {vpc_response_data}')
        json_content_for_aca['vpc_response'] = (vpc_response_data)

    # second delay
    print('Sleep 60 seconds after the VPC call')
    time.sleep(60)
    print('Start calling /ports API')

    # notify ZGC the ports created on each ACA
    PORT_data = zeta_data["PORT_data"]
    # for port in PORT_data:
    #     print(f'Port data: \n{port}')
    amount_of_ports = len(PORT_data)
    all_post_responses = []
    all_ports_start_time = time.time()
    print(f'Port_data length: \n{amount_of_ports}')
    for i in range(ceil(len(PORT_data) / port_api_upper_limit)):
        # print("Hello")
        start_idx = i * port_api_upper_limit
        end_idx = start_idx
        if end_idx + port_api_upper_limit >= amount_of_ports:
            end_idx = amount_of_ports
        else:
            end_idx = end_idx + port_api_upper_limit

        if start_idx == end_idx:
            end_idx = end_idx + 1
        print(f'In this /ports POST call, we are calling with port from {start_idx} to {end_idx}')
        one_call_start_time = time.time()
        port_response = requests.post(
        zgc_api_url + "/ports", data=json.dumps(PORT_data[start_idx: end_idx]), headers=headers)
        if port_response.status_code >= 300:
            print(f'Call failed for index {start_idx} to {end_idx}, \nstatus code: {port_response.status_code}, \ncontent: {port_response.content}\nExiting')
            return
        one_call_end_time = time.time()
        print(f'ONE PORT post call ended, for {end_idx - start_idx} ports creation it took: {one_call_end_time - one_call_start_time} seconds')
        all_post_responses.append(port_response.json())
    all_ports_end_time = time.time()
    print(
        f'ALL PORT post call ended, for {amount_of_ports} ports creation it took: {all_ports_end_time - all_ports_start_time} seconds')
    json_content_for_aca['port_response'] = list(itertools.chain.from_iterable(all_post_responses))
    print(f'Length of all ports response: {len(json_content_for_aca["port_response"])}')
    with open('aca_data.json', 'w') as outfile:
        json.dump(json_content_for_aca, outfile)
        print(f'The aca data is exported to {aca_data_local_path}')

def get_port_template(i):
    if i %2 == 0:
        return {
        "port_id": "333d4fae-7dec-11d0-a765-00a0c9341120",
        "vpc_id": "3dda2801-d675-4688-a63f-dcda8d327f61",
        "ips_port": [
            {
                "ip": "10.10.0.92",
                "vip": ""
            }
        ],
        "mac_port": "cc:dd:ee:ff:11:22",
        "ip_node": "192.168.20.92",
        "mac_node": "e8:bd:d1:01:77:ec"
    }
    return     {
        "port_id": "99976feae-7dec-11d0-a765-00a0c9342230",
        "vpc_id": "3dda2801-d675-4688-a63f-dcda8d327f61",
        "ips_port": [
            {
                "ip": "10.10.0.93",
                "vip": ""
            }
        ],
        "mac_port": "6c:dd:ee:ff:11:32",
        "ip_node": "192.168.20.93",
        "mac_node": "e8:bd:d1:01:72:c8"
    }


def generate_ports(ports_to_create):
    print(f'Need to generate {ports_to_create} ports')
    node_data = {}
    all_ports_generated = []
    unique_ips = set()
    unique_macs = set()
    unique_port_ids = set()
    for i in range(ports_to_create):
        port_template_to_use = get_port_template(i)
        port_id = '99976feae-7dec-11d0-a765-00a0c{0:07d}'.format(i)
        unique_port_ids.add(port_id)
        # print(f'port_id: {port_id}')
        ip_2nd_octet = '{0:02d}'.format((i // 10000))
        ip_3rd_octet = '{0:02d}'.format((i % 10000 // 100))
        ip_4th_octet = '{0:02d}'.format((i % 100))
        ip = ips_ports_ip_prefix + ip_2nd_octet + \
            "." + ip_3rd_octet + "." + ip_4th_octet
        # print(f'Generated IP: {ip}')
        unique_ips.add(ip)
        mac = mac_port_prefix + ip_2nd_octet + ":" + ip_3rd_octet + ":" + ip_4th_octet
        # print(f'Generated MAC: {mac}')
        unique_macs.add(mac)
        port_template_to_use['port_id'] = port_id
        port_template_to_use['ips_port'][0]['ip'] = ip
        port_template_to_use['mac_port'] = mac
        # print(f'Generated Port info: {port_template_to_use}')
        all_ports_generated.append(port_template_to_use)
    # print(f'Ports generated: {ports_to_create}, \nunique port_id: {len(unique_port_ids)}, \nunique IPs: {len(unique_ips)},\nunique MACs: {len(unique_macs)}')
    return all_ports_generated


def run():
    # right now the only argument should be how many ports to be generated.
    arguments = sys.argv
    print(f'Arguments: {arguments}')
    file_path = './data/zeta_data.json'
    zeta_data = {}
    with open(file_path, 'r', encoding='utf8')as fp:
        zeta_data = json.loads(fp.read())

    server_aca_repo_path = zeta_data['server_aca_repo_path']
    print(f'Server aca repo path: {server_aca_repo_path}')
    zgc_api_url = zeta_data["zeta_api_ip"]
    # second argument should be amount of ports to be generated
    if len(arguments) > 1:
        ports_to_create = int(arguments[1])
        if ports_to_create > 1000000:
            print(
                f'You tried to create {ports_to_create} ports, but the pseudo controller only supports up to 1,000,000 ports, sorry.')
            return
        print("Has arguments, need to generate some ports!")
        zeta_data['PORT_data'] = generate_ports(ports_to_create)
        print(f'After generating ports, we now have {len(zeta_data["PORT_data"])} entries in the PORT_data')

    talk_to_zeta(file_path, zgc_api_url, zeta_data)

    if len(arguments) == 1:
        print("Doesn't have arguments, just run the two node test.")
        aca_nodes_data = zeta_data["aca_nodes"]
        aca_nodes_ip = aca_nodes_data['ip']

        res = upload_file_aca(aca_nodes_data['ip'], aca_nodes_data['username'], aca_nodes_data['password'],
                              server_aca_repo_path + aca_data_destination_path, aca_data_local_path)
        if not res:
            print("upload file %s failed" % aca_data_local_path)
        else:
            print("upload file %s successfully" % aca_data_local_path)

        # Execute remote command, use the transferred file to change the information in aca_test_ovs_util.cpp,recompile using 'make',perform aca_test
        aca_nodes = aca_nodes_ip
        cmd_list2 = [
            f'cd {server_aca_repo_path};sudo ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_zeta_gateway_path_CHILD']
        result2 = exec_sshCommand_aca(
            host=aca_nodes[1], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=cmd_list2, timeout=60)

        cmd_list1 = [
            f'cd {server_aca_repo_path};sudo ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_zeta_gateway_path_PARENT']
        result1 = exec_sshCommand_aca(
            host=aca_nodes[0], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=cmd_list1, timeout=60)
        print(f'Status from node [{aca_nodes[0]}]: {result1["status"]}')
        print(f'Data from node [{aca_nodes[0]}]: {result1["data"]}')
        print(f'Error from node [{aca_nodes[0]}]: {result1["error"]}')
        print(f'Status from node [{aca_nodes[1]}]: {result2["status"]}')
        print(f'Data from node [{aca_nodes[1]}]: {result2["data"]}')
        print(f'Error from node [{aca_nodes[1]}]: {result2["error"]}')


if __name__ == '__main__':
    run()
