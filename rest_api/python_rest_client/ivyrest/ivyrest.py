import json
import requests
import uuid
import sys
#import jsonschema
#from jsonschema import validate
#import dnspython


class IvyRestClient(object):
    """ 
    :param target: IP address or domain name of the target arrays management interface.
    :type target: str
    :param username: Username of the user with which to log in.
    :type username: str, optional
    :param password: Password of the user with which to log in.
    :type password: str, optional
    :param api_token: API token of the user with which to log in.
    :type api_token: str, optional
    :param rest_version: REST API version to use when communicating with
                         target array.
    :type rest_version: str, optional
    :param verify_https: Enable SSL certificate verification for HTTPS requests.
    :type verify_https: bool, optional
    :param ssl_cert: Path to SSL certificate or CA Bundle file. Ignored if
                     verify_https=False.
    :type ssl_cert: str, optional
    :param user_agent: String to be used as the HTTP User-Agent for requests.
    :type user_agent: str, optional
    :param request_kwargs: Keyword arguments that we will pass into the the call
                           to requests.request.
    :type request_kwargs: dict, optional
    """ 

    def __init__(self, target, port=9000, username=None, password=None, api_token=None,
                 rest_version=None, verify_https=False, ssl_cert=None,
                 user_agent=None, request_kwargs=None):

        #if not api_token and not (username and password):
        #    raise ValueError(
        #        "Must specify API token or both username and password.")
        #elif api_token and (username or password):
        #    raise ValueError(
        #        "Specify only API token or both username and password.")

        self._cookies = {}
        self._target = target
        self._port = port

        self._renegotiate_rest_version = False if rest_version else True

        self._request_kwargs = dict(request_kwargs or {})
        if not "verify" in self._request_kwargs:
            if ssl_cert and verify_https:
                self._request_kwargs["verify"] = ssl_cert
            else:
                self._request_kwargs["verify"] = verify_https
        self._user_agent = user_agent

        #self._api_token = (api_token or self._obtain_api_token(username, password))
        #self._api_token = api_token
        self._api_token = str(uuid.uuid4())
        self._start_session()

    def _format_path(self, path):
        return "http://{0}:{1}{2}".format(
                self._target, self._port, path)

    def _request(self, method, path, data=None, reestablish_session=True):
        """Perform HTTP request for REST API."""
        if path.startswith("http"):
            url = path  # For cases where URL of different form is needed.
        else:
            url = self._format_path(path)

        print(url)

        headers = {"Content-Type": "application/json;charset=utf-8"}
        if self._user_agent:
            headers['User-Agent'] = self._user_agent

        body = json.dumps(data).encode("utf-8")
        response = requests.request(method, url, data=body, headers=headers,
                                        cookies=self._cookies, **self._request_kwargs)
        if response.status_code == 200:
            print(response.json())
            return response.json()
        else:        
            print("Ivy server is Busy")
            print(response)
            sys.exit()

    def _start_session(self):
        """Start a REST API session."""
        payload = {"api_token": self._api_token}
        response = self._request("POST", "/ivy_engine/sessions", data=payload,
                      reestablish_session=False)
        self._cookies = { 'session_cookie' : response['status'] }

    def end_session(self):
        """End a REST API session."""
        response = self._request("DELETE", "/ivy_engine/sessions", data={},
                      reestablish_session=False)

    def host_range(self, prefix, start, end, step):
        host_list = []
        for i in range(start, end, step):
            host_list.append(prefix + str(i))  #prefix + $i

        return host_list

    def port_range(self, prefix, start, end):
        port_list = []
        for c in range(ord(start), ord(end)+1):
            port_list.append(prefix + chr(c))

        return port_list

    #def Hosts(self, outfolder, testname, hosts, serial_number):
    def hosts_luns(self, **kwargs):
        payload = dict(kwargs)
#ivy.hosts(outputfolder = ".", testname = "MP_50", host = "sun159", serial_number = "83011441")
        print(payload)
        hostschema =  {
            "type": "object",
            "properties": {
                "outputfolder": { "type": "string" },
                "testname": { "type": "string" },
                "host": { "type": "string" },
                "serial_number": { "type": "string" },
                "LDEV": { "type": "string" }
            },
            "required": ["host", "serial_number"]
        }
        body = json.dumps(payload).encode("utf-8")
        print(body)
        #validate(body, hostschema)
        return self._request("POST", "/ivy_engine", data=payload)

    def set_output_folder_root(self, output_folder_root):
        return self._request("PUT", "/ivy_engine?output_folder_root=" + output_folder_root)

    def set_test_name(self, test_name):
        return self._request("PUT", "/ivy_engine?test_name=" + test_name)

    def set_io_sequencer_template(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        return self._request("PUT", "/ivy_engine/workloads", data=payload)

    def create_workload(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        return self._request("POST", "/ivy_engine/workloads", data=payload)

    def delete_workload(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        return self._request("DELETE", "/ivy_engine/workloads", data=payload)

    def create_rollup(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        return self._request("POST", "/ivy_engine/rollups", data=payload)

    def delete_rollup(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        return self._request("DELETE", "/ivy_engine/rollups", data=payload)

    def edit_rollup(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        return self._request("PUT", "/ivy_engine/rollups", data=payload)

    def csv_cell_value(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        ret = self._request("GET", "/ivy_engine/csvquery", data=payload)
        print(ret)
        #resp_dict = json.loads(ret)
        # convert or extract value
        #return resp_dict['status']
        return ret['status']

    def go(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        return self._request("PUT", "/ivy_engine", data=payload)

    def get_lun_list(self):
        ret=self._request("GET", "/ivy_engine?luns=true")
        print(ret)

    def get_host_list(self):
        return self._request("GET", "/ivy_engine?hosts=true")

    def get_subsystem_list(self):
        return self._request("GET", "/ivy_engine?subsystems=true")
