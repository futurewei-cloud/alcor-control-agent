import requests
import json
import paramiko
from collections import defaultdict, OrderedDict
import time

video = defaultdict(list)

# zeta_data = None

server_path = '/home/user/src/Github.com/zzxgzgz/alcor-control-agent/test/gtest/aca_data.json'
local_path = './aca_data.json'

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
def exec_sshCommand_aca(host, user, password, cmd, timeout=10):
    """
    :param host
    :param user
    :param password
    :param cmd
    :param seconds
    :return: dict
    """
    result = {'status': [], 'data': []}  # Record return result
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
            stdin, stdout, stderr = ssh.exec_command(command, get_pty=True, timeout=timeout)
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
        print(e)
        return False


def talk_to_zeta(file_path, zgc_api_url, zeta_data):
    headers = {'Content-type': 'application/json'}
    # create ZGC
    ZGC_data = zeta_data["ZGC_data"]
    print(f'ZGC_data: \n{ZGC_data}')
    zgc_response = requests.post(zgc_api_url + "/zgcs", data=json.dumps(ZGC_data), headers=headers)
    print(f'zgc creation response: \n{zgc_response}')
    zgc_id = zgc_response.json()['zgc_id']


    # add Nodes
    for node in zeta_data["NODE_data"]:
        node_data = node
        node_data['zgc_id'] = zgc_id
        print(f'node_data: \n{node_data}')
        node_response_data = requests.post(zgc_api_url + "/nodes", data=json.dumps(node_data), headers=headers)
        print(f'Response for adding node: {node_response_data.text}')
    
    json_content_for_aca = dict()
    json_content_for_aca['vpc_response'] = []
    json_content_for_aca['port_response'] = []

    # first delay
    print('Sleep 10 seconds after the Nodes call')
    time.sleep(10)   

    # add VPC
    for tem in zeta_data["VPC_data"]:
        VPC_data = tem
        print(f'VPC_data: \n{VPC_data}')
        vpc_response_data = requests.post(zgc_api_url + "/vpcs", data=json.dumps(VPC_data), headers=headers).json()
        print(f'Response for adding VPC: {vpc_response_data}')
        json_content_for_aca['vpc_response'].append(vpc_response_data)
        video["zgc_id"] = vpc_response_data["zgc_id"]

    # second delay
    print('Sleep 60 seconds after the VPC call')
    time.sleep(60)   

    # notify ZGC the ports created on each ACA
    PORT_data = zeta_data["PORT_data"]
    print(f'Port_data: \n{PORT_data}')
    port_response_data = requests.post(zgc_api_url + "/ports", data=json.dumps(PORT_data), headers=headers).json()
    print(f'Response for adding port: {port_response_data}')
    json_content_for_aca['port_response'].append(port_response_data)
    # TODO: 分别生成CHILD和PARENT的配置文件
    with open('aca_data.json', 'w') as outfile:
        json.dump(json_content_for_aca, outfile)
        print(f'The aca data is exported to {local_path}')


def run():
    # # Call zeta API to create ZGC,vpc etc.and generate the information ACA need, and save it in zetaToAca_data.json
    file_path = './data/zeta_data.json'
    zeta_data = {}
    with open(file_path, 'r', encoding='utf8')as fp:
        zeta_data = json.loads(fp.read())
        print(f'zeta_data: {zeta_data}')
        print(f'zeta_data type: {type(zeta_data)}')
    
    zgc_api_url = zeta_data["zeta_api_ip"]
    talk_to_zeta(file_path, zgc_api_url, zeta_data)

    aca_nodes_data = zeta_data["aca_nodes"]
    aca_nodes_ip = aca_nodes_data['ip']

    res = upload_file_aca(aca_nodes_data['ip'], aca_nodes_data['username'], aca_nodes_data['password'], server_path, local_path)
    if not res:
        print("upload file %s failed" % local_path)
    else:
        print("upload file %s successfully" % local_path)

    # Execute remote command, use the transferred file to change the information in aca_test_ovs_util.cpp,recompile using 'make',perform aca_test
    aca_nodes = aca_nodes_ip
    cmd_list1 = ['cd ~/src/Github.com/zzxgzgz/alcor-control-agent;sudo make',
                 'cd ~/src/Github.com/zzxgzgz/alcor-control-agent;./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_2_ports_CREATE_test_traffic_PARENT -c 10.213.43.93']
    result1 = exec_sshCommand_aca(host=aca_nodes[0], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=cmd_list1, timeout=10)
    cmd_list2 = ['cd ~/src/Github.com/zzxgzgz/alcor-control-agent;sudo make',
                 'cd ~/src/Github.com/zzxgzgz/alcor-control-agent;./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_2_ports_CREATE_test_traffic_CHILD -p 10.213.43.92']
    result2 = exec_sshCommand_aca(host=aca_nodes[1], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=cmd_list2, timeout=10)
    print(result1["status"])
    print(result1["data"])
    print(result2["status"])
    print(result2["data"])


if __name__ == '__main__':
    run()
