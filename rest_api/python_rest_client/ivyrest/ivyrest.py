import json
import requests
import uuid
import sys
import jsonschema
from jsonschema import validate


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
                 user_agent="Mozilla/5.0", request_kwargs=None):

        #if not api_token and not (username and password):
        #    raise ValueError(
        #        "Must specify API token or both username and password.")
        #elif api_token and (username or password):
        #    raise ValueError(
        #        "Specify only API token or both username and password.")

        self._cookies = {}
        self._target = target
        self._port = port
        self._last_result = True
        self._json_schema = {
    "$schema": "http://json-schema.org/draft-04/schema#",

    "definitions": {
        "percentageschema": {
            "anyof": [
                {  "type":     "number",
                  "minimum":  0,
                  "maximum":  100
                },
                {  "type":     "number",
                  "minimum":  0,
                  "maximum":  1
                }]
        },
        "binarysizeschema" : {
            "type": "object", 
	    "properties": {
                "value" : { "type":     "number" },
                "suffix" : { "type": { "enum": [ "KiB", "MiB", "GiB", "TiB" ] } }
            }
        },
        "hostlistschema": {
            "anyof" : [
                {"type" : "string"},
                {
                "type": "array",
                "items":{
                    "type": "string"
                }
                } ]
        },
        "selectschema": {
            "anyof" : [
                {"type" : "string"},
                {
                "type": "array",
                "items":{
                    "type": "object", 
                    "properties": {
                        "key" : { "type": "string" },
                        "val" : {
                            "anyof": [
                                { "type": "array",
                                "items":{
                                    "type": "string"
                                }},
                                {"type": "string"}
                             ]
                        }
                    }
                }
                } ]
        },
        "ioparametersschema":  {
            "anyof": [ 
            {"type" : "string"},
            {
            "type": "object", 
	    "properties": {
                "skew": { "$ref": "#/definitions/percentageschema" },
                "hot_zone_size_bytes" : {
                    "anyof": [
                        { "type": "string" },
                        { "$ref": "#/definitions/binarysizeschema" }
                    ]
                },
                "hot_zone_read_fraction" : { "type" : "boolean" },
                "hot_zone_write_fraction" : { "type" : "boolean" },
                "hot_zone_IOPS_fraction": { "$ref": "#/definitions/percentageschema" },
                "fractionRead": { "$ref": "#/definitions/percentageschema" },
                "VolumeCoverageFractionStart": { "$ref": "#/definitions/percentageschema" },
                "VolumeCoverageFractionEnd": { "$ref": "#/definitions/percentageschema" },
                "blocksize": { "type" : "integer" },
                "IOPS": {
                    "anyof": [
                          { "type": {
                              "enum": [ "max", "min" ] }
                          },
                          { "type": "object",
	                      "properties": {
                                  "allof": [
                                      { "anyof": [
                                            {  "incr+": { "type": "string" } },
                                            {  "decr-": { "type": "string" } }
                                       ] }, 
                                      { "amount": { "type": "integer" } }
                                  ] 
	                      }
	                  }
                    ] 
                },
                "pattern": {
                    "anyof": [
                          { "type": {
                              "enum": [ "random", "ascii", "gobbledgook" ] }
                          },
                          { "type": "object",
	                      "properties": {
                                  "variable_type": { "type": { 
                                      "enum": [ "trailing_zeros" ] }
                                  },
                                  "compressibility": { "$ref": "#/definitions/percentageschema" }
                              }
	                  }
                    ] 
                },
                "maxTags": {"type": "integer" }
            }
            } ]
        },
        "iosequencertemplateschema":  {
            "type": "object", 
	    "properties": {
                "iosequencer": { "type": {
                            "enum": [ "random_steady", "random_independent", "sequential" ] }},
                "parameters" : { "$ref": "#/definitions/ioparametersschema" }
	    }
        },
        "outputfolderschema":  {
            "outputfolder": { "type": "string" }
        },
        "testnameschema":  {
            "testname": { "type": "string" }
        },
        "hostsstatementschema":  {
            "type": "object", 
	    "properties": {
                "Hosts" : { "$ref": "#/definitions/hostlistschema" },
                "Select" : { "$ref": "#/definitions/selectschema" },
	    "required": ["Hosts", "Select"]
            }
        },
        "setiosequencertemplateschema": {
            "iosequencer" : { "$ref": "#/definitions/iosequencertemplateschema" }
        },
        "createworkloadschema":  {
            "type": "object", 
	    "properties": {
                "workload": { "type": "string" },
                "select" : { "$ref": "#/definitions/selectschema" },
                "iosequencer": { "type": {
                                    "enum": [ "random_steady", "random_independent", "sequential"]}},
                "parameters" : { "$ref": "#/definitions/ioparametersschema" }
	    },
	    "required": ["workload"]
        },
        "deleteworkloadschema":  {
            "type": "object", 
	    "properties": {
                "workload": { "type": "string" },
                "select" : { "$ref": "#/definitions/selectschema" }
	    },
	    "required": ["workload"]
        },
        "createrollupschema":  {
            "type": "object", 
	    "properties": {
                "name": { "type": "string" },
                "nocsv": { "type": "boolean" },
                "maxDroopMaxtoMinIOPSSection": { "type": "boolean" },
                "quantity": { "type": "integer" },
                "maxDroop": { "type": "number" }
	    },
	    "required": ["name"]
        },
        "editrollupschema":  {
            "type": "object", 
	    "properties": {
                "name": { "type": "string" },
                "parameters" : {
                    "anyof": [
                        { "type": "string" },
                        { "$ref": "#/definitions/ioparametersschema" }
                    ]
                }
	    },
	    "required": ["name", "parameters"]
        },
        "deleterollupschema":  {
            "type": "object", 
	    "properties": {
                "name": { "type": "string" }
	    },
	    "required": ["name"]
        },
        "csvqueryschema":  {
            "type": "object", 
	    "properties": {
                "filename": { "type": "string" },
                "row": { "type": "integer" },
                "col": { 
                    "anyof": [
                        { "type": "integer" },
                        { "type": "string" }
                    ]
	        }
	    },
	    "required": ["filename", "row", "col"]
        },
        "configschema":  {
            "type": "object", 
	    "properties": {
                "Hosts": {
                    "type": "array",
                    "items":{
                        "type": "string"
                    }
                },
                "allDiscoveredLUNs": {
                    "type": "array",
                    "items":{
                        "type": "string"
                    }
                },
                "availableTestLUNs": {
                    "type": "array",
                    "items":{
                        "type": "string"
                    }
                },
                "commandDeviceLUNs": {
                    "type": "array",
                    "items":{
                        "type": "string"
                    }
                },
                "Subsystems": {
                    "type": "array",
                    "items":{
                        "type": "string"
                    }
                }
	    }
        },
        "sessionsschema":  {
            "type": "object", 
	    "properties": {
                "api_token": { "type": "string" }
	    },
	    "required": ["api_token"]
        },
        "gostatementschema":  {
            "type": "object", 
	    "properties": {
                "stepname": { "type": "string" },
                "subinterval_seconds" : { "type": "integer" },
                "warmup_seconds" : { "type": "integer" },
                "measure_seconds" : { "type": "integer" },
                "cooldown_by_wp": { "type": "boolean" },
                "measure": { "type": {
                            "enum": ["MP_core_busy_percent", "PG_busy_percent",
                                     "CLPR_WP_percent", "MB_per_second", "IOPS",
                             "service_time_seconds", "response_time_seconds"]
                }},
                "accuracy_plus_minus": { "type": "string" },
                "timeout_seconds": { "type": "string" },
                "max_wp": { "$ref": "#/definitions/percentageschema" },
                "min_wp": { "$ref": "#/definitions/percentageschema" },
                "max_wp_change": { "$ref": "#/definitions/percentageschema" },
                "dfc": { "type": {
                            "enum": ["PID"]
                }},
                "low_IOPS": { "type": "string" },
                "low_target": { "type": "string" },
                "high_IOPS": { "type": "string" },
                "high_target": { "type": "string" },
                "target_value": { "type": "string" },
                "focus_rollup": { "type": {
                            "enum": ["all"]
                }},
                "source": { "type": {
                            "enum": ["workload", "RAID_subsystem"] }
                 },
                "subsystem_element": { "type": "string" },
                "element_metric": { "type": "string" },
                "category": { "type": {
                            "enum": ["overall", "read", "write", "random", "sequential",
                                     "random_read", "random_write", "sequential_read",
                                     "sequential_write"] }
                 },
                "accumulator_type": { "type": {
                            "enum": ["bytes_transferred", "service_time", "response_time"] }
                 },
                "accessor": { "type": {
                            "enum": ["avg", "count", "min","max", "sum", "variance",
                                     "standardDeviation"] }
                 }
	    },
	    "required": ["stepname"]
        },
        "responseschema":  {
            "type": "object", 
	    "properties": {
                "retval": { "type": "string" },
                "status": { "type": "string" },
                "result": { "type": "string" }
	    },
	    "required": ["retval", "status", "result"]
        }
    },

    "anyof": [
        { "$ref": "#/definitions/responseschema" },
        { "$ref": "#/definitions/percentageschema" },
        { "$ref": "#/definitions/binarysizeschema" },
        { "$ref": "#/definitions/hostlistschema" },
        { "$ref": "#/definitions/selectschema" },
        { "$ref": "#/definitions/ioparametersschema" },
        { "$ref": "#/definitions/iosequencertemplateschema" },
        { "$ref": "#/definitions/hostsstatementschema" },
        { "$ref": "#/definitions/setiosequencertemplateschema" },
        { "$ref": "#/definitions/createworkloadschema" },
        { "$ref": "#/definitions/deleteworkloadschema" },
        { "$ref": "#/definitions/createrollupschema" },
        { "$ref": "#/definitions/editrollupschema" },
        { "$ref": "#/definitions/deleterollupschema" },
        { "$ref": "#/definitions/sessionsschema" },
        { "$ref": "#/definitions/gostatementschema" },
        { "$ref": "#/definitions/configschema" },
        { "$ref": "#/definitions/csvquerychema" }
    ]
}
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

    def _end_session(self):
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

    def hosts_luns(self, **kwargs):
        payload = dict(kwargs)
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
        #validate(body, self._json_schema)
        ret = self._request("POST", "/ivy_engine", data=payload)
        self._last_result = ret['retval']
        return ret

    def set_output_folder_root(self, output_folder_root):
        ret = self._request("PUT", "/ivy_engine?output_folder_root=" + output_folder_root)
        self._last_result = ret['retval']
        return ret

    def set_test_name(self, test_name):
        ret = self._request("PUT", "/ivy_engine?test_name=" + test_name)
        self._last_result = ret['retval']
        return ret

    def set_io_sequencer_template(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        ret = self._request("PUT", "/ivy_engine/workloads", data=payload)
        self._last_result = ret['retval']
        return ret

    def create_workload(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        ret = self._request("POST", "/ivy_engine/workloads/1/a/2/b/3", data=payload)
        self._last_result = ret['retval']
        return ret

    def delete_workload(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        ret = self._request("DELETE", "/ivy_engine/workloads", data=payload)
        self._last_result = ret['retval']
        return ret

    def create_rollup(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        ret = self._request("POST", "/ivy_engine/rollups", data=payload)
        self._last_result = ret['retval']
        return ret

    def delete_rollup(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        ret = self._request("DELETE", "/ivy_engine/rollups", data=payload)
        self._last_result = ret['retval']
        return ret

    def edit_rollup(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        ret = self._request("PUT", "/ivy_engine/rollups", data=payload)
        self._last_result = ret['retval']
        return ret

    def exit(self):
            self._end_session()
            sys.exit()

    def last_result(self):
        return self._last_result

    def test_folder(self):
        ret = self._request("GET", "/ivy_engine?test_folder=true")
        print(ret)
        return ret['status']

    def test_name(self):
        ret = self._request("GET", "/ivy_engine?test_name=true")
        print(ret)
        return ret['status']

    def csv_rows(self, filename):
        ret = self._request("GET", "/ivy_engine/csvquery?file_name={0}&rows=true".format(filename))
        print(ret)
        return ret['status']

    def csv_header_columns(self, filename):
        ret = self._request("GET", "/ivy_engine/csvquery?file_name={0}&header_columns=true".format(filename))
        print(ret)
        return ret['status']

    def csv_columns_in_row(self, filename, row):
        ret = self._request("GET", "/ivy_engine/csvquery?file_name={0}&columns_in_row=true,row={1}".format(filename, row))
        print(ret)
        return ret['status']

    def csv_column_header(self, filename):
        ret = self._request("GET", "/ivy_engine/csvquery?file_name={0}&column_header=true".format(filename))
        print(ret)
        return ret['status']

    def csv_column(self, filename, col):
        ret = self._request("GET", "/ivy_engine/csvquery?file_name={0}&column={1}".format(filename), col)
        print(ret)
        return ret['status']

    def csv_lookup_column(self, filename, col_header_name):
        ret = self._request("GET", "/ivy_engine/csvquery?file_name={0}&column_lookup={1}".format(filename), col_header_name)
        print(ret)
        return ret['status']

    def csv_cell_value(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        ret = self._request("GET", "/ivy_engine/csvquery", data=payload)
        print(ret)
        return ret['status']

    def csv_raw_cell_value(self, **kwargs):
        payload = dict(kwargs)
        print(payload)
        ret = self._request("GET", "/ivy_engine/csvquery?raw=true", data=payload)
        print(ret)
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
