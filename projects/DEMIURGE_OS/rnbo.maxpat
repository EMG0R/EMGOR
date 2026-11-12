{
    "patcher": {
        "fileversion": 1,
        "appversion": {
            "major": 9,
            "minor": 1,
            "revision": 4,
            "architecture": "x64",
            "modernui": 1
        },
        "classnamespace": "box",
        "rect": [ 1329.0, 95.0, 149.0, 221.0 ],
        "boxes": [
            {
                "box": {
                    "id": "obj-2",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 0,
                    "patching_rect": [ 58.0, 126.0, 35.0, 22.0 ],
                    "text": "dac~"
                }
            },
            {
                "box": {
                    "autosave": 1,
                    "id": "obj-1",
                    "inletInfo": {
                        "IOInfo": [
                            {
                                "type": "midi",
                                "index": -1,
                                "tag": "",
                                "comment": ""
                            }
                        ]
                    },
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 3,
                    "outletInfo": {
                        "IOInfo": [
                            {
                                "type": "signal",
                                "index": 1,
                                "tag": "out1",
                                "comment": ""
                            },
                            {
                                "type": "signal",
                                "index": 2,
                                "tag": "out2",
                                "comment": ""
                            }
                        ]
                    },
                    "outlettype": [ "signal", "signal", "list" ],
                    "patcher": {
                        "fileversion": 1,
                        "appversion": {
                            "major": 9,
                            "minor": 1,
                            "revision": 4,
                            "architecture": "x64",
                            "modernui": 1
                        },
                        "classnamespace": "rnbo",
                        "rect": [ 34.0, 95.0, 1083.0, 797.0 ],
                        "default_fontname": "Lato",
                        "title": "untitled",
                        "boxes": [
                            {
                                "box": {
                                    "id": "obj-5",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 207.0, 172.0, 29.5, 23.0 ],
                                    "rnbo_classname": "*",
                                    "rnbo_extra_attributes": {
                                        "hot": 0
                                    },
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "*_obj-5",
                                    "text": "* -1"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-6",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 207.0, 131.0, 29.5, 23.0 ],
                                    "rnbo_classname": "-",
                                    "rnbo_extra_attributes": {
                                        "hot": 0
                                    },
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "-_obj-6",
                                    "text": "- 1."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-7",
                                    "maxclass": "newobj",
                                    "numinlets": 6,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 209.0, 87.0, 91.0, 23.0 ],
                                    "rnbo_classname": "scale",
                                    "rnbo_serial": 11,
                                    "rnbo_uniqueid": "scale_obj-7",
                                    "text": "scale 0 127 0 1."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-4",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 20.0, 177.0, 29.5, 23.0 ],
                                    "rnbo_classname": "*",
                                    "rnbo_extra_attributes": {
                                        "hot": 0
                                    },
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "*_obj-4",
                                    "text": "* -1"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-3",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 20.0, 136.0, 29.5, 23.0 ],
                                    "rnbo_classname": "-",
                                    "rnbo_extra_attributes": {
                                        "hot": 0
                                    },
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "-_obj-3",
                                    "text": "- 1."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-2",
                                    "maxclass": "newobj",
                                    "numinlets": 6,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 22.0, 92.0, 91.0, 23.0 ],
                                    "rnbo_classname": "scale",
                                    "rnbo_serial": 10,
                                    "rnbo_uniqueid": "scale_obj-2",
                                    "text": "scale 0 127 0 1."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-54",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 900.0, 177.0, 67.0, 23.0 ],
                                    "rnbo_classname": "loadmess",
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "loadmess_obj-54",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "input": {
                                                "attrOrProp": 1,
                                                "digest": "Bang to trigger message.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 0,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "inlet": 1,
                                                "type": "bang"
                                            },
                                            "message": {
                                                "attrOrProp": 1,
                                                "digest": "Message when patcher is loaded.",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "outlet": 1,
                                                "type": "list",
                                                "defaultValue": ""
                                            },
                                            "startupbang": {
                                                "attrOrProp": 1,
                                                "digest": "startupbang",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "bang"
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "input",
                                                "type": "bang",
                                                "digest": "Bang to trigger message.",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [
                                            {
                                                "name": "message",
                                                "type": "list",
                                                "digest": "Message when patcher is loaded.",
                                                "defaultarg": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "helpname": "loadmess",
                                        "aliasOf": "loadmess",
                                        "classname": "loadmess",
                                        "operator": 0,
                                        "versionId": -361086158,
                                        "changesPatcherIO": 0
                                    },
                                    "text": "loadmess 0"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-47",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [ 81.0, -72.0, 41.0, 21.0 ],
                                    "text": "attack"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-48",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 118.0, -39.0, 67.0, 23.0 ],
                                    "rnbo_classname": "loadmess",
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "loadmess_obj-48",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "input": {
                                                "attrOrProp": 1,
                                                "digest": "Bang to trigger message.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 0,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "inlet": 1,
                                                "type": "bang"
                                            },
                                            "message": {
                                                "attrOrProp": 1,
                                                "digest": "Message when patcher is loaded.",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "outlet": 1,
                                                "type": "list",
                                                "defaultValue": ""
                                            },
                                            "startupbang": {
                                                "attrOrProp": 1,
                                                "digest": "startupbang",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "bang"
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "input",
                                                "type": "bang",
                                                "digest": "Bang to trigger message.",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [
                                            {
                                                "name": "message",
                                                "type": "list",
                                                "digest": "Message when patcher is loaded.",
                                                "defaultarg": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "helpname": "loadmess",
                                        "aliasOf": "loadmess",
                                        "classname": "loadmess",
                                        "operator": 0,
                                        "versionId": -361086158,
                                        "changesPatcherIO": 0
                                    },
                                    "text": "loadmess 1"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-49",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 112.0, 22.0, 29.5, 23.0 ],
                                    "rnbo_classname": "message",
                                    "rnbo_extra_attributes": {
                                        "storeempty": 0,
                                        "text": ""
                                    },
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "message_obj-49",
                                    "text": "40"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-50",
                                    "maxclass": "newobj",
                                    "numinlets": 6,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 24.0, 214.0, 111.0, 23.0 ],
                                    "rnbo_classname": "scale",
                                    "rnbo_serial": 7,
                                    "rnbo_uniqueid": "scale_obj-50",
                                    "text": "scale 0. 1. 0.01 100"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-51",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 20.0, 30.0, 31.0, 23.0 ],
                                    "rnbo_classname": "gate",
                                    "rnbo_extra_attributes": {
                                        "outputs": 1.0
                                    },
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "gate_obj-51",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "onoff": {
                                                "attrOrProp": 1,
                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                "defaultarg": 2,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "inlet": 1,
                                                "type": "number",
                                                "defaultValue": "0"
                                            },
                                            "outputs": {
                                                "attrOrProp": 2,
                                                "digest": "Number of outlets",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "defaultValue": "1"
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "onoff",
                                                "type": "number",
                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                "defaultarg": 2,
                                                "docked": 0
                                            },
                                            {
                                                "name": "input",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "Incoming gated message",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [
                                            {
                                                "name": "out1",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "Gated Output 1",
                                                "docked": 0
                                            }
                                        ],
                                        "helpname": "gate",
                                        "aliasOf": "gate",
                                        "classname": "gate",
                                        "operator": 0,
                                        "versionId": -1253073831,
                                        "changesPatcherIO": 0
                                    },
                                    "text": "gate"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-52",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 31.0, -17.0, 39.0, 23.0 ],
                                    "rnbo_classname": "==",
                                    "rnbo_extra_attributes": {
                                        "hot": 0
                                    },
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "==_obj-52",
                                    "text": "== 27"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-53",
                                    "maxclass": "newobj",
                                    "numinlets": 3,
                                    "numoutlets": 3,
                                    "outlettype": [ "", "", "" ],
                                    "patching_rect": [ 16.0, -73.0, 40.0, 23.0 ],
                                    "rnbo_classname": "ctlin",
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "ctlin_obj-53",
                                    "text": "ctlin"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-40",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [ 270.0, -72.0, 39.0, 21.0 ],
                                    "text": "decay"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-41",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 307.0, -39.0, 67.0, 23.0 ],
                                    "rnbo_classname": "loadmess",
                                    "rnbo_serial": 3,
                                    "rnbo_uniqueid": "loadmess_obj-41",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "input": {
                                                "attrOrProp": 1,
                                                "digest": "Bang to trigger message.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 0,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "inlet": 1,
                                                "type": "bang"
                                            },
                                            "message": {
                                                "attrOrProp": 1,
                                                "digest": "Message when patcher is loaded.",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "outlet": 1,
                                                "type": "list",
                                                "defaultValue": ""
                                            },
                                            "startupbang": {
                                                "attrOrProp": 1,
                                                "digest": "startupbang",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "bang"
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "input",
                                                "type": "bang",
                                                "digest": "Bang to trigger message.",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [
                                            {
                                                "name": "message",
                                                "type": "list",
                                                "digest": "Message when patcher is loaded.",
                                                "defaultarg": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "helpname": "loadmess",
                                        "aliasOf": "loadmess",
                                        "classname": "loadmess",
                                        "operator": 0,
                                        "versionId": -361086158,
                                        "changesPatcherIO": 0
                                    },
                                    "text": "loadmess 1"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-42",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 301.0, 22.0, 29.5, 23.0 ],
                                    "rnbo_classname": "message",
                                    "rnbo_extra_attributes": {
                                        "storeempty": 0,
                                        "text": ""
                                    },
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "message_obj-42",
                                    "text": "40"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-43",
                                    "maxclass": "newobj",
                                    "numinlets": 6,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 213.0, 214.0, 104.0, 23.0 ],
                                    "rnbo_classname": "scale",
                                    "rnbo_serial": 8,
                                    "rnbo_uniqueid": "scale_obj-43",
                                    "text": "scale 0. 1. 0.1 200"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-44",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 209.0, 30.0, 31.0, 23.0 ],
                                    "rnbo_classname": "gate",
                                    "rnbo_extra_attributes": {
                                        "outputs": 1.0
                                    },
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "gate_obj-44",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "onoff": {
                                                "attrOrProp": 1,
                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                "defaultarg": 2,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "inlet": 1,
                                                "type": "number",
                                                "defaultValue": "0"
                                            },
                                            "outputs": {
                                                "attrOrProp": 2,
                                                "digest": "Number of outlets",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "defaultValue": "1"
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "onoff",
                                                "type": "number",
                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                "defaultarg": 2,
                                                "docked": 0
                                            },
                                            {
                                                "name": "input",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "Incoming gated message",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [
                                            {
                                                "name": "out1",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "Gated Output 1",
                                                "docked": 0
                                            }
                                        ],
                                        "helpname": "gate",
                                        "aliasOf": "gate",
                                        "classname": "gate",
                                        "operator": 0,
                                        "versionId": -1253073831,
                                        "changesPatcherIO": 0
                                    },
                                    "text": "gate"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-45",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 220.0, -17.0, 39.0, 23.0 ],
                                    "rnbo_classname": "==",
                                    "rnbo_extra_attributes": {
                                        "hot": 0
                                    },
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "==_obj-45",
                                    "text": "== 28"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-46",
                                    "maxclass": "newobj",
                                    "numinlets": 3,
                                    "numoutlets": 3,
                                    "outlettype": [ "", "", "" ],
                                    "patching_rect": [ 205.0, -73.0, 40.0, 23.0 ],
                                    "rnbo_classname": "ctlin",
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "ctlin_obj-46",
                                    "text": "ctlin"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-39",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [ 201.0, 293.0, 40.0, 21.0 ],
                                    "text": "cutoff"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-37",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [ 1037.0, 150.0, 43.0, 21.0 ],
                                    "text": "trigger"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-36",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [ 835.0, 98.0, 65.0, 21.0 ],
                                    "text": "probability"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-34",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 900.0, 214.0, 29.5, 23.0 ],
                                    "rnbo_classname": "message",
                                    "rnbo_extra_attributes": {
                                        "storeempty": 0,
                                        "text": ""
                                    },
                                    "rnbo_serial": 3,
                                    "rnbo_uniqueid": "message_obj-34",
                                    "text": "0.8"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-32",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 442.0, 65.0, 29.5, 23.0 ],
                                    "rnbo_classname": "message",
                                    "rnbo_extra_attributes": {
                                        "storeempty": 0,
                                        "text": ""
                                    },
                                    "rnbo_serial": 4,
                                    "rnbo_uniqueid": "message_obj-32",
                                    "text": "1"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-30",
                                    "maxclass": "button",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "bang" ],
                                    "parameter_enable": 0,
                                    "patching_rect": [ 973.0, 265.0, 24.0, 24.0 ],
                                    "rnbo_classname": "button",
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "button_obj-30"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-21",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 973.0, 130.0, 31.0, 23.0 ],
                                    "rnbo_classname": "gate",
                                    "rnbo_extra_attributes": {
                                        "outputs": 1.0
                                    },
                                    "rnbo_serial": 3,
                                    "rnbo_uniqueid": "gate_obj-21",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "onoff": {
                                                "attrOrProp": 1,
                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                "defaultarg": 2,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "inlet": 1,
                                                "type": "number",
                                                "defaultValue": "0"
                                            },
                                            "outputs": {
                                                "attrOrProp": 2,
                                                "digest": "Number of outlets",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "defaultValue": "1"
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "onoff",
                                                "type": "number",
                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                "defaultarg": 2,
                                                "docked": 0
                                            },
                                            {
                                                "name": "input",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "Incoming gated message",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [
                                            {
                                                "name": "out1",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "Gated Output 1",
                                                "docked": 0
                                            }
                                        ],
                                        "helpname": "gate",
                                        "aliasOf": "gate",
                                        "classname": "gate",
                                        "operator": 0,
                                        "versionId": -1253073831,
                                        "changesPatcherIO": 0
                                    },
                                    "text": "gate"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-22",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 1001.0, 82.0, 39.0, 23.0 ],
                                    "rnbo_classname": "==",
                                    "rnbo_extra_attributes": {
                                        "hot": 0
                                    },
                                    "rnbo_serial": 3,
                                    "rnbo_uniqueid": "==_obj-22",
                                    "text": "== 60"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-23",
                                    "maxclass": "newobj",
                                    "numinlets": 3,
                                    "numoutlets": 3,
                                    "outlettype": [ "", "", "" ],
                                    "patching_rect": [ 969.0, 42.0, 40.0, 23.0 ],
                                    "rnbo_classname": "ctlin",
                                    "rnbo_serial": 3,
                                    "rnbo_uniqueid": "ctlin_obj-23",
                                    "text": "ctlin"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-19",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 677.0, 387.0, 56.0, 23.0 ],
                                    "rnbo_classname": "loadbang",
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "loadbang_obj-19",
                                    "text": "loadbang"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-18",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 677.0, 431.0, 32.0, 23.0 ],
                                    "rnbo_classname": "message",
                                    "rnbo_extra_attributes": {
                                        "storeempty": 0,
                                        "text": ""
                                    },
                                    "rnbo_serial": 5,
                                    "rnbo_uniqueid": "message_obj-18",
                                    "text": "0.01"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-238",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "signal" ],
                                    "patching_rect": [ 434.0, 524.0, 39.0, 23.0 ],
                                    "rnbo_classname": "tanh~",
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "tanh~_obj-238",
                                    "text": "tanh~"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-237",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "signal" ],
                                    "patching_rect": [ 550.0, 524.0, 39.0, 23.0 ],
                                    "rnbo_classname": "tanh~",
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "tanh~_obj-237",
                                    "text": "tanh~"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-236",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "signal" ],
                                    "patching_rect": [ 550.0, 572.0, 39.0, 23.0 ],
                                    "rnbo_classname": "tanh~",
                                    "rnbo_serial": 3,
                                    "rnbo_uniqueid": "tanh~_obj-236",
                                    "text": "tanh~"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-235",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "signal" ],
                                    "patching_rect": [ 435.0, 572.0, 39.0, 23.0 ],
                                    "rnbo_classname": "tanh~",
                                    "rnbo_serial": 4,
                                    "rnbo_uniqueid": "tanh~_obj-235",
                                    "text": "tanh~"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-229",
                                    "maxclass": "newobj",
                                    "numinlets": 6,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 776.0, 214.0, 100.0, 23.0 ],
                                    "rnbo_classname": "scale",
                                    "rnbo_serial": 12,
                                    "rnbo_uniqueid": "scale_obj-229",
                                    "text": "scale 0. 127. 0. 1."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-230",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 776.0, 172.0, 31.0, 23.0 ],
                                    "rnbo_classname": "gate",
                                    "rnbo_extra_attributes": {
                                        "outputs": 1.0
                                    },
                                    "rnbo_serial": 4,
                                    "rnbo_uniqueid": "gate_obj-230",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "onoff": {
                                                "attrOrProp": 1,
                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                "defaultarg": 2,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "inlet": 1,
                                                "type": "number",
                                                "defaultValue": "0"
                                            },
                                            "outputs": {
                                                "attrOrProp": 2,
                                                "digest": "Number of outlets",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "defaultValue": "1"
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "onoff",
                                                "type": "number",
                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                "defaultarg": 2,
                                                "docked": 0
                                            },
                                            {
                                                "name": "input",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "Incoming gated message",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [
                                            {
                                                "name": "out1",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "Gated Output 1",
                                                "docked": 0
                                            }
                                        ],
                                        "helpname": "gate",
                                        "aliasOf": "gate",
                                        "classname": "gate",
                                        "operator": 0,
                                        "versionId": -1253073831,
                                        "changesPatcherIO": 0
                                    },
                                    "text": "gate"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-231",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 786.0, 125.0, 39.0, 23.0 ],
                                    "rnbo_classname": "==",
                                    "rnbo_extra_attributes": {
                                        "hot": 0
                                    },
                                    "rnbo_serial": 11,
                                    "rnbo_uniqueid": "==_obj-231",
                                    "text": "== 33"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-232",
                                    "maxclass": "newobj",
                                    "numinlets": 3,
                                    "numoutlets": 3,
                                    "outlettype": [ "", "", "" ],
                                    "patching_rect": [ 771.0, 69.0, 40.0, 23.0 ],
                                    "rnbo_classname": "ctlin",
                                    "rnbo_serial": 4,
                                    "rnbo_uniqueid": "ctlin_obj-232",
                                    "text": "ctlin"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-225",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 238.0, 326.0, 67.0, 23.0 ],
                                    "rnbo_classname": "loadmess",
                                    "rnbo_serial": 4,
                                    "rnbo_uniqueid": "loadmess_obj-225",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "input": {
                                                "attrOrProp": 1,
                                                "digest": "Bang to trigger message.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 0,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "inlet": 1,
                                                "type": "bang"
                                            },
                                            "message": {
                                                "attrOrProp": 1,
                                                "digest": "Message when patcher is loaded.",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "outlet": 1,
                                                "type": "list",
                                                "defaultValue": ""
                                            },
                                            "startupbang": {
                                                "attrOrProp": 1,
                                                "digest": "startupbang",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "bang"
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "input",
                                                "type": "bang",
                                                "digest": "Bang to trigger message.",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [
                                            {
                                                "name": "message",
                                                "type": "list",
                                                "digest": "Message when patcher is loaded.",
                                                "defaultarg": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "helpname": "loadmess",
                                        "aliasOf": "loadmess",
                                        "classname": "loadmess",
                                        "operator": 0,
                                        "versionId": -361086158,
                                        "changesPatcherIO": 0
                                    },
                                    "text": "loadmess 1"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-222",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 232.0, 387.0, 29.5, 23.0 ],
                                    "rnbo_classname": "message",
                                    "rnbo_extra_attributes": {
                                        "storeempty": 0,
                                        "text": ""
                                    },
                                    "rnbo_serial": 6,
                                    "rnbo_uniqueid": "message_obj-222",
                                    "text": "40"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-219",
                                    "maxclass": "newobj",
                                    "numinlets": 6,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 140.0, 437.0, 162.0, 23.0 ],
                                    "rnbo_classname": "scale",
                                    "rnbo_serial": 4,
                                    "rnbo_uniqueid": "scale_obj-219",
                                    "text": "scale 0. 127. 200. 10000. 2.3"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-217",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 140.0, 395.0, 31.0, 23.0 ],
                                    "rnbo_classname": "gate",
                                    "rnbo_extra_attributes": {
                                        "outputs": 1.0
                                    },
                                    "rnbo_serial": 5,
                                    "rnbo_uniqueid": "gate_obj-217",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "onoff": {
                                                "attrOrProp": 1,
                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                "defaultarg": 2,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "inlet": 1,
                                                "type": "number",
                                                "defaultValue": "0"
                                            },
                                            "outputs": {
                                                "attrOrProp": 2,
                                                "digest": "Number of outlets",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "defaultValue": "1"
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "onoff",
                                                "type": "number",
                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                "defaultarg": 2,
                                                "docked": 0
                                            },
                                            {
                                                "name": "input",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "Incoming gated message",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [
                                            {
                                                "name": "out1",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "Gated Output 1",
                                                "docked": 0
                                            }
                                        ],
                                        "helpname": "gate",
                                        "aliasOf": "gate",
                                        "classname": "gate",
                                        "operator": 0,
                                        "versionId": -1253073831,
                                        "changesPatcherIO": 0
                                    },
                                    "text": "gate"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-216",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 151.0, 348.0, 39.0, 23.0 ],
                                    "rnbo_classname": "==",
                                    "rnbo_extra_attributes": {
                                        "hot": 0
                                    },
                                    "rnbo_serial": 6,
                                    "rnbo_uniqueid": "==_obj-216",
                                    "text": "== 31"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-215",
                                    "maxclass": "newobj",
                                    "numinlets": 3,
                                    "numoutlets": 3,
                                    "outlettype": [ "", "", "" ],
                                    "patching_rect": [ 136.0, 292.0, 40.0, 23.0 ],
                                    "rnbo_classname": "ctlin",
                                    "rnbo_serial": 5,
                                    "rnbo_uniqueid": "ctlin_obj-215",
                                    "text": "ctlin"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-214",
                                    "maxclass": "newobj",
                                    "numinlets": 3,
                                    "numoutlets": 4,
                                    "outlettype": [ "signal", "signal", "signal", "signal" ],
                                    "patching_rect": [ 551.0, 481.0, 50.5, 23.0 ],
                                    "rnbo_classname": "svf~",
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "svf~_obj-214",
                                    "text": "svf~"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-213",
                                    "maxclass": "newobj",
                                    "numinlets": 3,
                                    "numoutlets": 4,
                                    "outlettype": [ "signal", "signal", "signal", "signal" ],
                                    "patching_rect": [ 434.0, 481.0, 50.5, 23.0 ],
                                    "rnbo_classname": "svf~",
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "svf~_obj-213",
                                    "text": "svf~"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-212",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 605.0, 116.0, 64.0, 23.0 ],
                                    "rnbo_classname": "metro",
                                    "rnbo_extra_attributes": {
                                        "lock": 0.0
                                    },
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "metro_obj-212",
                                    "text": "metro 16n"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-211",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 523.0, 116.0, 64.0, 23.0 ],
                                    "rnbo_classname": "metro",
                                    "rnbo_extra_attributes": {
                                        "lock": 0.0
                                    },
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "metro_obj-211",
                                    "text": "metro 16n"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-209",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 618.0, 166.0, 29.5, 23.0 ],
                                    "rnbo_classname": "message",
                                    "rnbo_extra_attributes": {
                                        "storeempty": 0,
                                        "text": ""
                                    },
                                    "rnbo_serial": 7,
                                    "rnbo_uniqueid": "message_obj-209",
                                    "text": "60"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-210",
                                    "maxclass": "newobj",
                                    "numinlets": 5,
                                    "numoutlets": 2,
                                    "outlettype": [ "signal", "signal" ],
                                    "patcher": {
                                        "fileversion": 1,
                                        "appversion": {
                                            "major": 9,
                                            "minor": 1,
                                            "revision": 4,
                                            "architecture": "x64",
                                            "modernui": 1
                                        },
                                        "classnamespace": "rnbo",
                                        "rect": [ 790.0, 95.0, 550.0, 779.0 ],
                                        "default_fontname": "Lato",
                                        "title": "voice",
                                        "boxes": [
                                            {
                                                "box": {
                                                    "id": "obj-5",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 109.0, 282.2518238425255, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "in_obj-5",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 5",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 5"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-4",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 68.0, 282.2518238425255, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "in_obj-4",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 4",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 4"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-6",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 229.68, 51.0, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 3,
                                                    "rnbo_uniqueid": "in_obj-6",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 3",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 3"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-3",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 228.18, 227.52, 31.0, 23.0 ],
                                                    "rnbo_classname": "gate",
                                                    "rnbo_extra_attributes": {
                                                        "outputs": 1.0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "gate_obj-3",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "onoff": {
                                                                "attrOrProp": 1,
                                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                                "defaultarg": 2,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "number",
                                                                "defaultValue": "0"
                                                            },
                                                            "outputs": {
                                                                "attrOrProp": 2,
                                                                "digest": "Number of outlets",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "1"
                                                            }
                                                        },
                                                        "inputs": [
                                                            {
                                                                "name": "onoff",
                                                                "type": "number",
                                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                                "defaultarg": 2,
                                                                "docked": 0
                                                            },
                                                            {
                                                                "name": "input",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "Incoming gated message",
                                                                "hot": 1,
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "Gated Output 1",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "gate",
                                                        "aliasOf": "gate",
                                                        "classname": "gate",
                                                        "operator": 0,
                                                        "versionId": -1253073831,
                                                        "changesPatcherIO": 0
                                                    },
                                                    "text": "gate"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-2",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 208.39415973424911, 145.0, 35.0, 23.0 ],
                                                    "rnbo_classname": ">",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": ">_obj-2",
                                                    "text": "> 0.6"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-1",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 208.97262680530548, 100.0, 59.0, 23.0 ],
                                                    "rnbo_classname": "random",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "random_obj-1",
                                                    "text": "random 1"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "format": 0,
                                                    "id": "obj-201",
                                                    "maxclass": "number",
                                                    "numinlets": 1,
                                                    "numoutlets": 2,
                                                    "outlettype": [ "", "bang" ],
                                                    "parameter_enable": 0,
                                                    "patching_rect": [ 386.4963485598564, 128.46715313196182, 50.0, 23.0 ],
                                                    "rnbo_classname": "number",
                                                    "rnbo_extra_attributes": {
                                                        "minimum": "<none>",
                                                        "order": "",
                                                        "preset": 0,
                                                        "maximum": "<none>",
                                                        "initialFormat": "integer"
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "number_obj-201"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-197",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 371.8978084921837, 164.23357629776, 29.5, 23.0 ],
                                                    "rnbo_classname": "-",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "-_obj-197",
                                                    "text": "- 4"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-196",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 358.0291954278946, 195.62043744325638, 29.5, 23.0 ],
                                                    "rnbo_classname": "*",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "*_obj-196",
                                                    "text": "* 7"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-195",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 339.05109333992004, 233.57664161920547, 29.5, 23.0 ],
                                                    "rnbo_classname": "+",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "+_obj-195",
                                                    "text": "+"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-194",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 401.09488862752914, 100.0, 59.0, 23.0 ],
                                                    "rnbo_classname": "random",
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "random_obj-194",
                                                    "text": "random 7"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-193",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "signal" ],
                                                    "patching_rect": [ 358.0291954278946, 499.27007085084915, 29.5, 23.0 ],
                                                    "rnbo_classname": "*~",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "*~_obj-193",
                                                    "text": "*~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-188",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 90.1459851861, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "message_obj-188",
                                                    "text": "0"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-186",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 182.1167876124382, 324.0875900387764, 60.0, 23.0 ],
                                                    "rnbo_classname": "delay",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "delay_obj-186",
                                                    "text": "delay 100"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-185",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "signal" ],
                                                    "patching_rect": [ 223.7226059396362, 499.27007085084915, 29.5, 23.0 ],
                                                    "rnbo_classname": "*~",
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "*~_obj-185",
                                                    "text": "*~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-184",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 245.62043690681458, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "message_obj-184",
                                                    "text": "400"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-183",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 208.39415973424911, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 3,
                                                    "rnbo_uniqueid": "message_obj-183",
                                                    "text": "0"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-182",
                                                    "linecount": 4,
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 167.51824754476547, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 4,
                                                    "rnbo_uniqueid": "message_obj-182",
                                                    "text": "629.989764"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-181",
                                                    "linecount": 4,
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 131.02189737558365, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 5,
                                                    "rnbo_uniqueid": "message_obj-181",
                                                    "text": "157.487165"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-178",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 50.0, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 6,
                                                    "rnbo_uniqueid": "message_obj-178",
                                                    "text": "1"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-176",
                                                    "maxclass": "newobj",
                                                    "numinlets": 5,
                                                    "numoutlets": 2,
                                                    "outlettype": [ "signal", "" ],
                                                    "patching_rect": [ 131.02189737558365, 437.22627556324005, 61.0, 23.0 ],
                                                    "rnbo_classname": "adsr~",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "adsr~_obj-176",
                                                    "text": "adsr~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-173",
                                                    "maxclass": "button",
                                                    "numinlets": 1,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "bang" ],
                                                    "parameter_enable": 0,
                                                    "patching_rect": [ 173.35766357183456, 281.7518238425255, 24.0, 24.0 ],
                                                    "rnbo_classname": "button",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "button_obj-173"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-168",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 369.7080274820328, 345.25547313690186, 33.0, 23.0 ],
                                                    "rnbo_classname": "-",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "-_obj-168",
                                                    "text": "- 0.2"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-167",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 301.09488916397095, 345.25547313690186, 33.0, 23.0 ],
                                                    "rnbo_classname": "-",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 3,
                                                    "rnbo_uniqueid": "-_obj-167",
                                                    "text": "- 0.2"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-166",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 321.53284525871277, 270.0729917883873, 33.0, 23.0 ],
                                                    "rnbo_classname": "mtof",
                                                    "rnbo_extra_attributes": {
                                                        "scalename": "",
                                                        "filter": 1.0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "mtof_obj-166",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "midivalue": {
                                                                "attrOrProp": 1,
                                                                "digest": "MIDI Note Number",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "list"
                                                            },
                                                            "tuning": {
                                                                "attrOrProp": 1,
                                                                "digest": "Set base frequency",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "number"
                                                            },
                                                            "scale": {
                                                                "attrOrProp": 1,
                                                                "digest": "Scala scl formatted list",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "list"
                                                            },
                                                            "map": {
                                                                "attrOrProp": 1,
                                                                "digest": "Scala kbm formatted list",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "list"
                                                            },
                                                            "ref": {
                                                                "attrOrProp": 1,
                                                                "digest": "Reference note for which the 'base' frequency is given",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "69"
                                                            },
                                                            "mid": {
                                                                "attrOrProp": 1,
                                                                "digest": "Middle note where the first scale degree is mapped",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "60"
                                                            },
                                                            "base": {
                                                                "attrOrProp": 1,
                                                                "digest": "Frequency to tune 'ref' note to",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "440"
                                                            },
                                                            "out": {
                                                                "attrOrProp": 1,
                                                                "digest": "Frequency (Hz)",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "outlet": 1,
                                                                "type": "list"
                                                            },
                                                            "scalename": {
                                                                "attrOrProp": 2,
                                                                "digest": "Scale Name",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "filter": {
                                                                "attrOrProp": 2,
                                                                "digest": "Filter unmapped values",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "1"
                                                            }
                                                        },
                                                        "inputs": [
                                                            {
                                                                "name": "midivalue",
                                                                "type": "list",
                                                                "digest": "MIDI Note Number",
                                                                "hot": 1,
                                                                "docked": 0
                                                            },
                                                            {
                                                                "name": "tuning",
                                                                "type": "number",
                                                                "digest": "Set base frequency",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "outputs": [
                                                            {
                                                                "name": "out",
                                                                "type": "list",
                                                                "digest": "Frequency (Hz)",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "mtof",
                                                        "aliasOf": "mtof",
                                                        "classname": "mtof",
                                                        "operator": 0,
                                                        "versionId": 950896033,
                                                        "changesPatcherIO": 0
                                                    },
                                                    "text": "mtof"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-163",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "signal" ],
                                                    "patching_rect": [ 369.7080274820328, 456.2043776512146, 40.0, 23.0 ],
                                                    "rnbo_classname": "*~",
                                                    "rnbo_serial": 3,
                                                    "rnbo_uniqueid": "*~_obj-163",
                                                    "text": "*~ 0.3"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-162",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "signal" ],
                                                    "patching_rect": [ 301.09488916397095, 448.0, 40.0, 23.0 ],
                                                    "rnbo_classname": "*~",
                                                    "rnbo_serial": 4,
                                                    "rnbo_uniqueid": "*~_obj-162",
                                                    "text": "*~ 0.3"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-158",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 2,
                                                    "outlettype": [ "signal", "signal" ],
                                                    "patching_rect": [ 369.7080274820328, 408.75912243127823, 36.0, 23.0 ],
                                                    "rnbo_classname": "saw~",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "saw~_obj-158",
                                                    "text": "saw~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-157",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 2,
                                                    "outlettype": [ "signal", "signal" ],
                                                    "patching_rect": [ 301.09488916397095, 408.75912243127823, 36.0, 23.0 ],
                                                    "rnbo_classname": "saw~",
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "saw~_obj-157",
                                                    "text": "saw~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-202",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 281.2262679396363, 40.00001106794355, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 4,
                                                    "rnbo_uniqueid": "in_obj-202",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 1",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 1"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-203",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 339.0510969396362, 40.00001106794355, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 5,
                                                    "rnbo_uniqueid": "in_obj-203",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 2",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 2"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-204",
                                                    "maxclass": "newobj",
                                                    "numinlets": 1,
                                                    "numoutlets": 0,
                                                    "patching_rect": [ 223.7226059396362, 582.2700920679436, 43.0, 23.0 ],
                                                    "rnbo_classname": "out~",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "out~_obj-204",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "in1": {
                                                                "attrOrProp": 1,
                                                                "digest": "signal sent to outlet with index 1",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 0,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "signal"
                                                            },
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "outlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [
                                                            {
                                                                "name": "in1",
                                                                "type": "signal",
                                                                "digest": "signal sent to outlet with index 1",
                                                                "displayName": "",
                                                                "hot": 1,
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "outputs": [],
                                                        "helpname": "out~",
                                                        "aliasOf": "out~",
                                                        "classname": "out~",
                                                        "operator": 0,
                                                        "versionId": 1989326771,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "out~ 1"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-205",
                                                    "maxclass": "newobj",
                                                    "numinlets": 1,
                                                    "numoutlets": 0,
                                                    "patching_rect": [ 358.0292459396362, 582.2700920679436, 43.0, 23.0 ],
                                                    "rnbo_classname": "out~",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "out~_obj-205",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "in1": {
                                                                "attrOrProp": 1,
                                                                "digest": "signal sent to outlet with index 2",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 0,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "signal"
                                                            },
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "outlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [
                                                            {
                                                                "name": "in1",
                                                                "type": "signal",
                                                                "digest": "signal sent to outlet with index 2",
                                                                "displayName": "",
                                                                "hot": 1,
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "outputs": [],
                                                        "helpname": "out~",
                                                        "aliasOf": "out~",
                                                        "classname": "out~",
                                                        "operator": 0,
                                                        "versionId": 1989326771,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "out~ 2"
                                                }
                                            }
                                        ],
                                        "lines": [
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-2", 0 ],
                                                    "source": [ "obj-1", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-162", 0 ],
                                                    "source": [ "obj-157", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-163", 0 ],
                                                    "source": [ "obj-158", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-185", 1 ],
                                                    "source": [ "obj-162", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-193", 1 ],
                                                    "source": [ "obj-163", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-167", 0 ],
                                                    "order": 1,
                                                    "source": [ "obj-166", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-168", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-166", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-157", 0 ],
                                                    "source": [ "obj-167", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-158", 0 ],
                                                    "source": [ "obj-168", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-178", 0 ],
                                                    "order": 1,
                                                    "source": [ "obj-173", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-186", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-173", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-185", 0 ],
                                                    "order": 1,
                                                    "source": [ "obj-176", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-193", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-176", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 0 ],
                                                    "source": [ "obj-178", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 1 ],
                                                    "source": [ "obj-181", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 2 ],
                                                    "source": [ "obj-182", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 3 ],
                                                    "source": [ "obj-183", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 4 ],
                                                    "source": [ "obj-184", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-204", 0 ],
                                                    "source": [ "obj-185", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-181", 0 ],
                                                    "order": 3,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-182", 0 ],
                                                    "order": 2,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-183", 0 ],
                                                    "order": 1,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-184", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-188", 0 ],
                                                    "order": 4,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 0 ],
                                                    "source": [ "obj-188", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-205", 0 ],
                                                    "source": [ "obj-193", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-201", 0 ],
                                                    "source": [ "obj-194", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-166", 0 ],
                                                    "source": [ "obj-195", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-195", 1 ],
                                                    "source": [ "obj-196", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-196", 0 ],
                                                    "source": [ "obj-197", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-3", 0 ],
                                                    "source": [ "obj-2", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-197", 0 ],
                                                    "source": [ "obj-201", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-1", 0 ],
                                                    "order": 2,
                                                    "source": [ "obj-202", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-194", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-202", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-3", 1 ],
                                                    "order": 1,
                                                    "source": [ "obj-202", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-195", 0 ],
                                                    "source": [ "obj-203", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-173", 0 ],
                                                    "source": [ "obj-3", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-181", 1 ],
                                                    "source": [ "obj-4", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-182", 1 ],
                                                    "source": [ "obj-5", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-2", 1 ],
                                                    "source": [ "obj-6", 0 ]
                                                }
                                            }
                                        ]
                                    },
                                    "patching_rect": [ 618.0, 259.0, 46.0, 23.0 ],
                                    "rnbo_classname": "p",
                                    "rnbo_extra_attributes": {
                                        "exposevoiceparams": 0,
                                        "args": [],
                                        "uidstyle": "auto",
                                        "polyphony": -1.0,
                                        "notecontroller": 0,
                                        "voicecontrol": "simple",
                                        "receivemode": "local"
                                    },
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "voice",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "target": {
                                                "attrOrProp": 1,
                                                "digest": "target",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 0,
                                                "attachable": 1,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "defaultValue": "0"
                                            },
                                            "mute": {
                                                "attrOrProp": 1,
                                                "digest": "mute",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 1,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number"
                                            },
                                            "__probingout1": {
                                                "attrOrProp": 1,
                                                "digest": "__probingout1",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "signal"
                                            },
                                            "out1": {
                                                "attrOrProp": 1,
                                                "digest": "out1",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "outlet": 1,
                                                "type": "signal"
                                            },
                                            "out2": {
                                                "attrOrProp": 1,
                                                "digest": "out2",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "outlet": 1,
                                                "type": "signal"
                                            },
                                            "polyphony": {
                                                "attrOrProp": 2,
                                                "digest": "Polyphony of the subpatcher.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "defaultValue": "-1"
                                            },
                                            "exposevoiceparams": {
                                                "attrOrProp": 2,
                                                "digest": "Expose per voice versions of the contained parameters (only valid in polyphonic subpatchers).",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "bool",
                                                "defaultValue": "false"
                                            },
                                            "title": {
                                                "attrOrProp": 2,
                                                "digest": "Title of the subpatcher",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "doNotShowInMaxInspector": 1
                                            },
                                            "file": {
                                                "attrOrProp": 2,
                                                "digest": "rnbo file to load",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "doNotShowInMaxInspector": 1
                                            },
                                            "voicecontrol": {
                                                "attrOrProp": 2,
                                                "digest": "Chooses the way that polyphonic voices are controlled. 'simple' (or 'midi') will automatically allocate voices for \tincoming MIDI notes. Setting it to 'user' (or 'none') will switch off MIDI \tvoice allocation and start with all voices unmuted.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "enum": [ "simple", "user" ],
                                                "type": "enum",
                                                "defaultValue": "simple"
                                            },
                                            "notecontroller": {
                                                "attrOrProp": 2,
                                                "digest": "DEPRECATED. Use voicecontrol instead.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "doNotShowInMaxInspector": 1
                                            },
                                            "receivemode": {
                                                "attrOrProp": 2,
                                                "digest": "Do receive~ objects get the signal from a send~ inside the patcher directly (without latency), or compensated (with latency, aligned with all other voices).",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "enum": [ "local", "compensated" ],
                                                "type": "enum",
                                                "defaultValue": "local"
                                            },
                                            "args": {
                                                "attrOrProp": 2,
                                                "digest": "Replacement args for the subpatcher, everything named #1, #2 etc. will be replaced with the according argument.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "doNotShowInMaxInspector": 1
                                            },
                                            "uidstyle": {
                                                "attrOrProp": 2,
                                                "digest": "Behavior of #0 unique ID. auto (default) means abstractions get a local UID, local: start a new local UID, parent: use the one from the parent patcher",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "enum": [ "auto", "local", "parent", "global" ],
                                                "type": "enum",
                                                "defaultValue": "auto"
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "in1",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in1",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            },
                                            {
                                                "name": "in2",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in2",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            },
                                            {
                                                "name": "in3",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in3",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            },
                                            {
                                                "name": "in4",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in4",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            },
                                            {
                                                "name": "in5",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in5",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [
                                            {
                                                "name": "out1",
                                                "type": "signal",
                                                "digest": "out1",
                                                "displayName": "",
                                                "docked": 0
                                            },
                                            {
                                                "name": "out2",
                                                "type": "signal",
                                                "digest": "out2",
                                                "displayName": "",
                                                "docked": 0
                                            }
                                        ],
                                        "helpname": "patcher",
                                        "aliasOf": "rnbo",
                                        "classname": "p",
                                        "operator": 0,
                                        "versionId": 426236520,
                                        "changesPatcherIO": 0
                                    },
                                    "text": "p voice",
                                    "varname": "voice"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-208",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 532.0, 167.0, 29.5, 23.0 ],
                                    "rnbo_classname": "message",
                                    "rnbo_extra_attributes": {
                                        "storeempty": 0,
                                        "text": ""
                                    },
                                    "rnbo_serial": 8,
                                    "rnbo_uniqueid": "message_obj-208",
                                    "text": "55"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-207",
                                    "maxclass": "newobj",
                                    "numinlets": 5,
                                    "numoutlets": 2,
                                    "outlettype": [ "signal", "signal" ],
                                    "patcher": {
                                        "fileversion": 1,
                                        "appversion": {
                                            "major": 9,
                                            "minor": 1,
                                            "revision": 4,
                                            "architecture": "x64",
                                            "modernui": 1
                                        },
                                        "classnamespace": "rnbo",
                                        "rect": [ 790.0, 95.0, 550.0, 779.0 ],
                                        "default_fontname": "Lato",
                                        "title": "voice",
                                        "boxes": [
                                            {
                                                "box": {
                                                    "id": "obj-5",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 109.0, 282.2518238425255, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "in_obj-5",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 5",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 5"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-4",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 68.0, 282.2518238425255, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "in_obj-4",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 4",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 4"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-6",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 229.68, 51.0, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 3,
                                                    "rnbo_uniqueid": "in_obj-6",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 3",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 3"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-3",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 228.18, 227.52, 31.0, 23.0 ],
                                                    "rnbo_classname": "gate",
                                                    "rnbo_extra_attributes": {
                                                        "outputs": 1.0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "gate_obj-3",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "onoff": {
                                                                "attrOrProp": 1,
                                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                                "defaultarg": 2,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "number",
                                                                "defaultValue": "0"
                                                            },
                                                            "outputs": {
                                                                "attrOrProp": 2,
                                                                "digest": "Number of outlets",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "1"
                                                            }
                                                        },
                                                        "inputs": [
                                                            {
                                                                "name": "onoff",
                                                                "type": "number",
                                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                                "defaultarg": 2,
                                                                "docked": 0
                                                            },
                                                            {
                                                                "name": "input",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "Incoming gated message",
                                                                "hot": 1,
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "Gated Output 1",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "gate",
                                                        "aliasOf": "gate",
                                                        "classname": "gate",
                                                        "operator": 0,
                                                        "versionId": -1253073831,
                                                        "changesPatcherIO": 0
                                                    },
                                                    "text": "gate"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-2",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 208.39415973424911, 145.0, 35.0, 23.0 ],
                                                    "rnbo_classname": ">",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": ">_obj-2",
                                                    "text": "> 0.6"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-1",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 208.97262680530548, 100.0, 59.0, 23.0 ],
                                                    "rnbo_classname": "random",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "random_obj-1",
                                                    "text": "random 1"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "format": 0,
                                                    "id": "obj-201",
                                                    "maxclass": "number",
                                                    "numinlets": 1,
                                                    "numoutlets": 2,
                                                    "outlettype": [ "", "bang" ],
                                                    "parameter_enable": 0,
                                                    "patching_rect": [ 386.4963485598564, 128.46715313196182, 50.0, 23.0 ],
                                                    "rnbo_classname": "number",
                                                    "rnbo_extra_attributes": {
                                                        "minimum": "<none>",
                                                        "order": "",
                                                        "preset": 0,
                                                        "maximum": "<none>",
                                                        "initialFormat": "integer"
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "number_obj-201"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-197",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 371.8978084921837, 164.23357629776, 29.5, 23.0 ],
                                                    "rnbo_classname": "-",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "-_obj-197",
                                                    "text": "- 4"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-196",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 358.0291954278946, 195.62043744325638, 29.5, 23.0 ],
                                                    "rnbo_classname": "*",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "*_obj-196",
                                                    "text": "* 7"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-195",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 339.05109333992004, 233.57664161920547, 29.5, 23.0 ],
                                                    "rnbo_classname": "+",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "+_obj-195",
                                                    "text": "+"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-194",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 401.09488862752914, 100.0, 59.0, 23.0 ],
                                                    "rnbo_classname": "random",
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "random_obj-194",
                                                    "text": "random 7"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-193",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "signal" ],
                                                    "patching_rect": [ 358.0291954278946, 499.27007085084915, 29.5, 23.0 ],
                                                    "rnbo_classname": "*~",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "*~_obj-193",
                                                    "text": "*~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-188",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 90.1459851861, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "message_obj-188",
                                                    "text": "0"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-186",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 182.1167876124382, 324.0875900387764, 60.0, 23.0 ],
                                                    "rnbo_classname": "delay",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "delay_obj-186",
                                                    "text": "delay 100"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-185",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "signal" ],
                                                    "patching_rect": [ 223.7226059396362, 499.27007085084915, 29.5, 23.0 ],
                                                    "rnbo_classname": "*~",
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "*~_obj-185",
                                                    "text": "*~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-184",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 245.62043690681458, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "message_obj-184",
                                                    "text": "400"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-183",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 208.39415973424911, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 3,
                                                    "rnbo_uniqueid": "message_obj-183",
                                                    "text": "0"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-182",
                                                    "linecount": 4,
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 167.51824754476547, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 4,
                                                    "rnbo_uniqueid": "message_obj-182",
                                                    "text": "629.989764"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-181",
                                                    "linecount": 4,
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 131.02189737558365, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 5,
                                                    "rnbo_uniqueid": "message_obj-181",
                                                    "text": "157.487165"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-178",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 50.0, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 6,
                                                    "rnbo_uniqueid": "message_obj-178",
                                                    "text": "1"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-176",
                                                    "maxclass": "newobj",
                                                    "numinlets": 5,
                                                    "numoutlets": 2,
                                                    "outlettype": [ "signal", "" ],
                                                    "patching_rect": [ 131.02189737558365, 437.22627556324005, 61.0, 23.0 ],
                                                    "rnbo_classname": "adsr~",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "adsr~_obj-176",
                                                    "text": "adsr~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-173",
                                                    "maxclass": "button",
                                                    "numinlets": 1,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "bang" ],
                                                    "parameter_enable": 0,
                                                    "patching_rect": [ 173.35766357183456, 281.7518238425255, 24.0, 24.0 ],
                                                    "rnbo_classname": "button",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "button_obj-173"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-168",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 369.7080274820328, 345.25547313690186, 33.0, 23.0 ],
                                                    "rnbo_classname": "-",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "-_obj-168",
                                                    "text": "- 0.2"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-167",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 301.09488916397095, 345.25547313690186, 33.0, 23.0 ],
                                                    "rnbo_classname": "-",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 3,
                                                    "rnbo_uniqueid": "-_obj-167",
                                                    "text": "- 0.2"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-166",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 321.53284525871277, 270.0729917883873, 33.0, 23.0 ],
                                                    "rnbo_classname": "mtof",
                                                    "rnbo_extra_attributes": {
                                                        "scalename": "",
                                                        "filter": 1.0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "mtof_obj-166",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "midivalue": {
                                                                "attrOrProp": 1,
                                                                "digest": "MIDI Note Number",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "list"
                                                            },
                                                            "tuning": {
                                                                "attrOrProp": 1,
                                                                "digest": "Set base frequency",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "number"
                                                            },
                                                            "scale": {
                                                                "attrOrProp": 1,
                                                                "digest": "Scala scl formatted list",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "list"
                                                            },
                                                            "map": {
                                                                "attrOrProp": 1,
                                                                "digest": "Scala kbm formatted list",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "list"
                                                            },
                                                            "ref": {
                                                                "attrOrProp": 1,
                                                                "digest": "Reference note for which the 'base' frequency is given",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "69"
                                                            },
                                                            "mid": {
                                                                "attrOrProp": 1,
                                                                "digest": "Middle note where the first scale degree is mapped",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "60"
                                                            },
                                                            "base": {
                                                                "attrOrProp": 1,
                                                                "digest": "Frequency to tune 'ref' note to",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "440"
                                                            },
                                                            "out": {
                                                                "attrOrProp": 1,
                                                                "digest": "Frequency (Hz)",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "outlet": 1,
                                                                "type": "list"
                                                            },
                                                            "scalename": {
                                                                "attrOrProp": 2,
                                                                "digest": "Scale Name",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "filter": {
                                                                "attrOrProp": 2,
                                                                "digest": "Filter unmapped values",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "1"
                                                            }
                                                        },
                                                        "inputs": [
                                                            {
                                                                "name": "midivalue",
                                                                "type": "list",
                                                                "digest": "MIDI Note Number",
                                                                "hot": 1,
                                                                "docked": 0
                                                            },
                                                            {
                                                                "name": "tuning",
                                                                "type": "number",
                                                                "digest": "Set base frequency",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "outputs": [
                                                            {
                                                                "name": "out",
                                                                "type": "list",
                                                                "digest": "Frequency (Hz)",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "mtof",
                                                        "aliasOf": "mtof",
                                                        "classname": "mtof",
                                                        "operator": 0,
                                                        "versionId": 950896033,
                                                        "changesPatcherIO": 0
                                                    },
                                                    "text": "mtof"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-163",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "signal" ],
                                                    "patching_rect": [ 369.7080274820328, 456.2043776512146, 40.0, 23.0 ],
                                                    "rnbo_classname": "*~",
                                                    "rnbo_serial": 3,
                                                    "rnbo_uniqueid": "*~_obj-163",
                                                    "text": "*~ 0.3"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-162",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "signal" ],
                                                    "patching_rect": [ 301.09488916397095, 448.0, 40.0, 23.0 ],
                                                    "rnbo_classname": "*~",
                                                    "rnbo_serial": 4,
                                                    "rnbo_uniqueid": "*~_obj-162",
                                                    "text": "*~ 0.3"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-158",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 2,
                                                    "outlettype": [ "signal", "signal" ],
                                                    "patching_rect": [ 369.7080274820328, 408.75912243127823, 36.0, 23.0 ],
                                                    "rnbo_classname": "saw~",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "saw~_obj-158",
                                                    "text": "saw~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-157",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 2,
                                                    "outlettype": [ "signal", "signal" ],
                                                    "patching_rect": [ 301.09488916397095, 408.75912243127823, 36.0, 23.0 ],
                                                    "rnbo_classname": "saw~",
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "saw~_obj-157",
                                                    "text": "saw~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-202",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 281.2262679396363, 40.00001106794355, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 4,
                                                    "rnbo_uniqueid": "in_obj-202",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 1",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 1"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-203",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 339.0510969396362, 40.00001106794355, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 5,
                                                    "rnbo_uniqueid": "in_obj-203",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 2",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 2"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-204",
                                                    "maxclass": "newobj",
                                                    "numinlets": 1,
                                                    "numoutlets": 0,
                                                    "patching_rect": [ 223.7226059396362, 582.2700920679436, 43.0, 23.0 ],
                                                    "rnbo_classname": "out~",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "out~_obj-204",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "in1": {
                                                                "attrOrProp": 1,
                                                                "digest": "signal sent to outlet with index 1",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 0,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "signal"
                                                            },
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "outlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [
                                                            {
                                                                "name": "in1",
                                                                "type": "signal",
                                                                "digest": "signal sent to outlet with index 1",
                                                                "displayName": "",
                                                                "hot": 1,
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "outputs": [],
                                                        "helpname": "out~",
                                                        "aliasOf": "out~",
                                                        "classname": "out~",
                                                        "operator": 0,
                                                        "versionId": 1989326771,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "out~ 1"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-205",
                                                    "maxclass": "newobj",
                                                    "numinlets": 1,
                                                    "numoutlets": 0,
                                                    "patching_rect": [ 358.0292459396362, 582.2700920679436, 43.0, 23.0 ],
                                                    "rnbo_classname": "out~",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "out~_obj-205",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "in1": {
                                                                "attrOrProp": 1,
                                                                "digest": "signal sent to outlet with index 2",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 0,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "signal"
                                                            },
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "outlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [
                                                            {
                                                                "name": "in1",
                                                                "type": "signal",
                                                                "digest": "signal sent to outlet with index 2",
                                                                "displayName": "",
                                                                "hot": 1,
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "outputs": [],
                                                        "helpname": "out~",
                                                        "aliasOf": "out~",
                                                        "classname": "out~",
                                                        "operator": 0,
                                                        "versionId": 1989326771,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "out~ 2"
                                                }
                                            }
                                        ],
                                        "lines": [
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-2", 0 ],
                                                    "source": [ "obj-1", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-162", 0 ],
                                                    "source": [ "obj-157", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-163", 0 ],
                                                    "source": [ "obj-158", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-185", 1 ],
                                                    "source": [ "obj-162", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-193", 1 ],
                                                    "source": [ "obj-163", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-167", 0 ],
                                                    "order": 1,
                                                    "source": [ "obj-166", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-168", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-166", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-157", 0 ],
                                                    "source": [ "obj-167", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-158", 0 ],
                                                    "source": [ "obj-168", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-178", 0 ],
                                                    "order": 1,
                                                    "source": [ "obj-173", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-186", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-173", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-185", 0 ],
                                                    "order": 1,
                                                    "source": [ "obj-176", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-193", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-176", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 0 ],
                                                    "source": [ "obj-178", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 1 ],
                                                    "source": [ "obj-181", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 2 ],
                                                    "source": [ "obj-182", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 3 ],
                                                    "source": [ "obj-183", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 4 ],
                                                    "source": [ "obj-184", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-204", 0 ],
                                                    "source": [ "obj-185", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-181", 0 ],
                                                    "order": 3,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-182", 0 ],
                                                    "order": 2,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-183", 0 ],
                                                    "order": 1,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-184", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-188", 0 ],
                                                    "order": 4,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 0 ],
                                                    "source": [ "obj-188", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-205", 0 ],
                                                    "source": [ "obj-193", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-201", 0 ],
                                                    "source": [ "obj-194", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-166", 0 ],
                                                    "source": [ "obj-195", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-195", 1 ],
                                                    "source": [ "obj-196", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-196", 0 ],
                                                    "source": [ "obj-197", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-3", 0 ],
                                                    "source": [ "obj-2", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-197", 0 ],
                                                    "source": [ "obj-201", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-1", 0 ],
                                                    "order": 2,
                                                    "source": [ "obj-202", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-194", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-202", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-3", 1 ],
                                                    "order": 1,
                                                    "source": [ "obj-202", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-195", 0 ],
                                                    "source": [ "obj-203", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-173", 0 ],
                                                    "source": [ "obj-3", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-181", 1 ],
                                                    "source": [ "obj-4", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-182", 1 ],
                                                    "source": [ "obj-5", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-2", 1 ],
                                                    "source": [ "obj-6", 0 ]
                                                }
                                            }
                                        ]
                                    },
                                    "patching_rect": [ 532.0, 259.0, 46.0, 23.0 ],
                                    "rnbo_classname": "p",
                                    "rnbo_extra_attributes": {
                                        "exposevoiceparams": 0,
                                        "args": [],
                                        "uidstyle": "auto",
                                        "polyphony": -1.0,
                                        "notecontroller": 0,
                                        "voicecontrol": "simple",
                                        "receivemode": "local"
                                    },
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "voice[1]",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "target": {
                                                "attrOrProp": 1,
                                                "digest": "target",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 0,
                                                "attachable": 1,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "defaultValue": "0"
                                            },
                                            "mute": {
                                                "attrOrProp": 1,
                                                "digest": "mute",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 1,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number"
                                            },
                                            "__probingout1": {
                                                "attrOrProp": 1,
                                                "digest": "__probingout1",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "signal"
                                            },
                                            "out1": {
                                                "attrOrProp": 1,
                                                "digest": "out1",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "outlet": 1,
                                                "type": "signal"
                                            },
                                            "out2": {
                                                "attrOrProp": 1,
                                                "digest": "out2",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "outlet": 1,
                                                "type": "signal"
                                            },
                                            "polyphony": {
                                                "attrOrProp": 2,
                                                "digest": "Polyphony of the subpatcher.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "defaultValue": "-1"
                                            },
                                            "exposevoiceparams": {
                                                "attrOrProp": 2,
                                                "digest": "Expose per voice versions of the contained parameters (only valid in polyphonic subpatchers).",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "bool",
                                                "defaultValue": "false"
                                            },
                                            "title": {
                                                "attrOrProp": 2,
                                                "digest": "Title of the subpatcher",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "doNotShowInMaxInspector": 1
                                            },
                                            "file": {
                                                "attrOrProp": 2,
                                                "digest": "rnbo file to load",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "doNotShowInMaxInspector": 1
                                            },
                                            "voicecontrol": {
                                                "attrOrProp": 2,
                                                "digest": "Chooses the way that polyphonic voices are controlled. 'simple' (or 'midi') will automatically allocate voices for \tincoming MIDI notes. Setting it to 'user' (or 'none') will switch off MIDI \tvoice allocation and start with all voices unmuted.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "enum": [ "simple", "user" ],
                                                "type": "enum",
                                                "defaultValue": "simple"
                                            },
                                            "notecontroller": {
                                                "attrOrProp": 2,
                                                "digest": "DEPRECATED. Use voicecontrol instead.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "doNotShowInMaxInspector": 1
                                            },
                                            "receivemode": {
                                                "attrOrProp": 2,
                                                "digest": "Do receive~ objects get the signal from a send~ inside the patcher directly (without latency), or compensated (with latency, aligned with all other voices).",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "enum": [ "local", "compensated" ],
                                                "type": "enum",
                                                "defaultValue": "local"
                                            },
                                            "args": {
                                                "attrOrProp": 2,
                                                "digest": "Replacement args for the subpatcher, everything named #1, #2 etc. will be replaced with the according argument.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "doNotShowInMaxInspector": 1
                                            },
                                            "uidstyle": {
                                                "attrOrProp": 2,
                                                "digest": "Behavior of #0 unique ID. auto (default) means abstractions get a local UID, local: start a new local UID, parent: use the one from the parent patcher",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "enum": [ "auto", "local", "parent", "global" ],
                                                "type": "enum",
                                                "defaultValue": "auto"
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "in1",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in1",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            },
                                            {
                                                "name": "in2",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in2",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            },
                                            {
                                                "name": "in3",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in3",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            },
                                            {
                                                "name": "in4",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in4",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            },
                                            {
                                                "name": "in5",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in5",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [
                                            {
                                                "name": "out1",
                                                "type": "signal",
                                                "digest": "out1",
                                                "displayName": "",
                                                "docked": 0
                                            },
                                            {
                                                "name": "out2",
                                                "type": "signal",
                                                "digest": "out2",
                                                "displayName": "",
                                                "docked": 0
                                            }
                                        ],
                                        "helpname": "patcher",
                                        "aliasOf": "rnbo",
                                        "classname": "p",
                                        "operator": 0,
                                        "versionId": 426236520,
                                        "changesPatcherIO": 0
                                    },
                                    "text": "p voice",
                                    "varname": "voice[1]"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-206",
                                    "maxclass": "newobj",
                                    "numinlets": 5,
                                    "numoutlets": 2,
                                    "outlettype": [ "signal", "signal" ],
                                    "patcher": {
                                        "fileversion": 1,
                                        "appversion": {
                                            "major": 9,
                                            "minor": 1,
                                            "revision": 4,
                                            "architecture": "x64",
                                            "modernui": 1
                                        },
                                        "classnamespace": "rnbo",
                                        "rect": [ 790.0, 95.0, 550.0, 779.0 ],
                                        "default_fontname": "Lato",
                                        "title": "voice",
                                        "boxes": [
                                            {
                                                "box": {
                                                    "id": "obj-5",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 109.0, 282.2518238425255, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "in_obj-5",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 5",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 5"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-4",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 68.0, 282.2518238425255, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "in_obj-4",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 4",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 4"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-6",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 229.68, 51.0, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 3,
                                                    "rnbo_uniqueid": "in_obj-6",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 3",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 3"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-3",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 228.18, 227.52, 31.0, 23.0 ],
                                                    "rnbo_classname": "gate",
                                                    "rnbo_extra_attributes": {
                                                        "outputs": 1.0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "gate_obj-3",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "onoff": {
                                                                "attrOrProp": 1,
                                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                                "defaultarg": 2,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "number",
                                                                "defaultValue": "0"
                                                            },
                                                            "outputs": {
                                                                "attrOrProp": 2,
                                                                "digest": "Number of outlets",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "1"
                                                            }
                                                        },
                                                        "inputs": [
                                                            {
                                                                "name": "onoff",
                                                                "type": "number",
                                                                "digest": "0 Closes gate, non-zero opens the corresponding gate outlet",
                                                                "defaultarg": 2,
                                                                "docked": 0
                                                            },
                                                            {
                                                                "name": "input",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "Incoming gated message",
                                                                "hot": 1,
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "Gated Output 1",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "gate",
                                                        "aliasOf": "gate",
                                                        "classname": "gate",
                                                        "operator": 0,
                                                        "versionId": -1253073831,
                                                        "changesPatcherIO": 0
                                                    },
                                                    "text": "gate"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-2",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 208.39415973424911, 145.0, 35.0, 23.0 ],
                                                    "rnbo_classname": ">",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": ">_obj-2",
                                                    "text": "> 0.6"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-1",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 208.97262680530548, 100.0, 59.0, 23.0 ],
                                                    "rnbo_classname": "random",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "random_obj-1",
                                                    "text": "random 1"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "format": 0,
                                                    "id": "obj-201",
                                                    "maxclass": "number",
                                                    "numinlets": 1,
                                                    "numoutlets": 2,
                                                    "outlettype": [ "", "bang" ],
                                                    "parameter_enable": 0,
                                                    "patching_rect": [ 386.4963485598564, 128.46715313196182, 50.0, 23.0 ],
                                                    "rnbo_classname": "number",
                                                    "rnbo_extra_attributes": {
                                                        "minimum": "<none>",
                                                        "order": "",
                                                        "preset": 0,
                                                        "maximum": "<none>",
                                                        "initialFormat": "integer"
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "number_obj-201"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-197",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 371.8978084921837, 164.23357629776, 29.5, 23.0 ],
                                                    "rnbo_classname": "-",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "-_obj-197",
                                                    "text": "- 4"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-196",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 358.0291954278946, 195.62043744325638, 29.5, 23.0 ],
                                                    "rnbo_classname": "*",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "*_obj-196",
                                                    "text": "* 7"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-195",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 339.05109333992004, 233.57664161920547, 29.5, 23.0 ],
                                                    "rnbo_classname": "+",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "+_obj-195",
                                                    "text": "+"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-194",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 401.09488862752914, 100.0, 59.0, 23.0 ],
                                                    "rnbo_classname": "random",
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "random_obj-194",
                                                    "text": "random 7"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-193",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "signal" ],
                                                    "patching_rect": [ 358.0291954278946, 499.27007085084915, 29.5, 23.0 ],
                                                    "rnbo_classname": "*~",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "*~_obj-193",
                                                    "text": "*~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-188",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 90.1459851861, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "message_obj-188",
                                                    "text": "0"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-186",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 182.1167876124382, 324.0875900387764, 60.0, 23.0 ],
                                                    "rnbo_classname": "delay",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "delay_obj-186",
                                                    "text": "delay 100"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-185",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "signal" ],
                                                    "patching_rect": [ 223.7226059396362, 499.27007085084915, 29.5, 23.0 ],
                                                    "rnbo_classname": "*~",
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "*~_obj-185",
                                                    "text": "*~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-184",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 245.62043690681458, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "message_obj-184",
                                                    "text": "400"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-183",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 208.39415973424911, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 3,
                                                    "rnbo_uniqueid": "message_obj-183",
                                                    "text": "0"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-182",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 68.0, 339.0, 85.2226059396362, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 4,
                                                    "rnbo_uniqueid": "message_obj-182",
                                                    "text": "629.989764"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-181",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ -28.0, 345.25547313690186, 92.0, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 5,
                                                    "rnbo_uniqueid": "message_obj-181",
                                                    "text": "157.487165"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-178",
                                                    "maxclass": "message",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 50.0, 389.05109333992004, 29.5, 23.0 ],
                                                    "rnbo_classname": "message",
                                                    "rnbo_extra_attributes": {
                                                        "storeempty": 0,
                                                        "text": ""
                                                    },
                                                    "rnbo_serial": 6,
                                                    "rnbo_uniqueid": "message_obj-178",
                                                    "text": "1"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-176",
                                                    "maxclass": "newobj",
                                                    "numinlets": 5,
                                                    "numoutlets": 2,
                                                    "outlettype": [ "signal", "" ],
                                                    "patching_rect": [ 131.02189737558365, 437.22627556324005, 61.0, 23.0 ],
                                                    "rnbo_classname": "adsr~",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "adsr~_obj-176",
                                                    "text": "adsr~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-173",
                                                    "maxclass": "button",
                                                    "numinlets": 1,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "bang" ],
                                                    "parameter_enable": 0,
                                                    "patching_rect": [ 173.35766357183456, 281.7518238425255, 24.0, 24.0 ],
                                                    "rnbo_classname": "button",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "button_obj-173"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-168",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 369.7080274820328, 345.25547313690186, 33.0, 23.0 ],
                                                    "rnbo_classname": "-",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "-_obj-168",
                                                    "text": "- 0.2"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-167",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 301.09488916397095, 345.25547313690186, 33.0, 23.0 ],
                                                    "rnbo_classname": "-",
                                                    "rnbo_extra_attributes": {
                                                        "hot": 0
                                                    },
                                                    "rnbo_serial": 3,
                                                    "rnbo_uniqueid": "-_obj-167",
                                                    "text": "- 0.2"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-166",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 321.53284525871277, 270.0729917883873, 33.0, 23.0 ],
                                                    "rnbo_classname": "mtof",
                                                    "rnbo_extra_attributes": {
                                                        "scalename": "",
                                                        "filter": 1.0
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "mtof_obj-166",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "midivalue": {
                                                                "attrOrProp": 1,
                                                                "digest": "MIDI Note Number",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "list"
                                                            },
                                                            "tuning": {
                                                                "attrOrProp": 1,
                                                                "digest": "Set base frequency",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "number"
                                                            },
                                                            "scale": {
                                                                "attrOrProp": 1,
                                                                "digest": "Scala scl formatted list",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "list"
                                                            },
                                                            "map": {
                                                                "attrOrProp": 1,
                                                                "digest": "Scala kbm formatted list",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "list"
                                                            },
                                                            "ref": {
                                                                "attrOrProp": 1,
                                                                "digest": "Reference note for which the 'base' frequency is given",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "69"
                                                            },
                                                            "mid": {
                                                                "attrOrProp": 1,
                                                                "digest": "Middle note where the first scale degree is mapped",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "60"
                                                            },
                                                            "base": {
                                                                "attrOrProp": 1,
                                                                "digest": "Frequency to tune 'ref' note to",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 1,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "440"
                                                            },
                                                            "out": {
                                                                "attrOrProp": 1,
                                                                "digest": "Frequency (Hz)",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "outlet": 1,
                                                                "type": "list"
                                                            },
                                                            "scalename": {
                                                                "attrOrProp": 2,
                                                                "digest": "Scale Name",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "filter": {
                                                                "attrOrProp": 2,
                                                                "digest": "Filter unmapped values",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "defaultValue": "1"
                                                            }
                                                        },
                                                        "inputs": [
                                                            {
                                                                "name": "midivalue",
                                                                "type": "list",
                                                                "digest": "MIDI Note Number",
                                                                "hot": 1,
                                                                "docked": 0
                                                            },
                                                            {
                                                                "name": "tuning",
                                                                "type": "number",
                                                                "digest": "Set base frequency",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "outputs": [
                                                            {
                                                                "name": "out",
                                                                "type": "list",
                                                                "digest": "Frequency (Hz)",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "mtof",
                                                        "aliasOf": "mtof",
                                                        "classname": "mtof",
                                                        "operator": 0,
                                                        "versionId": 950896033,
                                                        "changesPatcherIO": 0
                                                    },
                                                    "text": "mtof"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-163",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "signal" ],
                                                    "patching_rect": [ 369.7080274820328, 456.2043776512146, 40.0, 23.0 ],
                                                    "rnbo_classname": "*~",
                                                    "rnbo_serial": 3,
                                                    "rnbo_uniqueid": "*~_obj-163",
                                                    "text": "*~ 0.3"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-162",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "signal" ],
                                                    "patching_rect": [ 301.09488916397095, 448.0, 40.0, 23.0 ],
                                                    "rnbo_classname": "*~",
                                                    "rnbo_serial": 4,
                                                    "rnbo_uniqueid": "*~_obj-162",
                                                    "text": "*~ 0.3"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-158",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 2,
                                                    "outlettype": [ "signal", "signal" ],
                                                    "patching_rect": [ 369.7080274820328, 408.75912243127823, 36.0, 23.0 ],
                                                    "rnbo_classname": "saw~",
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "saw~_obj-158",
                                                    "text": "saw~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-157",
                                                    "maxclass": "newobj",
                                                    "numinlets": 2,
                                                    "numoutlets": 2,
                                                    "outlettype": [ "signal", "signal" ],
                                                    "patching_rect": [ 301.09488916397095, 408.75912243127823, 36.0, 23.0 ],
                                                    "rnbo_classname": "saw~",
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "saw~_obj-157",
                                                    "text": "saw~"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-202",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 281.2262679396363, 40.00001106794355, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 4,
                                                    "rnbo_uniqueid": "in_obj-202",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 1",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 1"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-203",
                                                    "maxclass": "newobj",
                                                    "numinlets": 0,
                                                    "numoutlets": 1,
                                                    "outlettype": [ "" ],
                                                    "patching_rect": [ 339.0510969396362, 40.00001106794355, 28.0, 23.0 ],
                                                    "rnbo_classname": "in",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 5,
                                                    "rnbo_uniqueid": "in_obj-203",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "inlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [],
                                                        "outputs": [
                                                            {
                                                                "name": "out1",
                                                                "type": [ "bang", "number", "list" ],
                                                                "digest": "value from inlet with index 2",
                                                                "displayName": "",
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "helpname": "in",
                                                        "aliasOf": "in",
                                                        "classname": "in",
                                                        "operator": 0,
                                                        "versionId": 475235762,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "in 2"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-204",
                                                    "maxclass": "newobj",
                                                    "numinlets": 1,
                                                    "numoutlets": 0,
                                                    "patching_rect": [ 223.7226059396362, 582.2700920679436, 43.0, 23.0 ],
                                                    "rnbo_classname": "out~",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 1,
                                                    "rnbo_uniqueid": "out~_obj-204",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "in1": {
                                                                "attrOrProp": 1,
                                                                "digest": "signal sent to outlet with index 1",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 0,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "signal"
                                                            },
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "outlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [
                                                            {
                                                                "name": "in1",
                                                                "type": "signal",
                                                                "digest": "signal sent to outlet with index 1",
                                                                "displayName": "",
                                                                "hot": 1,
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "outputs": [],
                                                        "helpname": "out~",
                                                        "aliasOf": "out~",
                                                        "classname": "out~",
                                                        "operator": 0,
                                                        "versionId": 1989326771,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "out~ 1"
                                                }
                                            },
                                            {
                                                "box": {
                                                    "id": "obj-205",
                                                    "maxclass": "newobj",
                                                    "numinlets": 1,
                                                    "numoutlets": 0,
                                                    "patching_rect": [ 358.0292459396362, 582.2700920679436, 43.0, 23.0 ],
                                                    "rnbo_classname": "out~",
                                                    "rnbo_extra_attributes": {
                                                        "comment": "",
                                                        "meta": ""
                                                    },
                                                    "rnbo_serial": 2,
                                                    "rnbo_uniqueid": "out~_obj-205",
                                                    "rnboinfo": {
                                                        "needsInstanceInfo": 1,
                                                        "argnames": {
                                                            "in1": {
                                                                "attrOrProp": 1,
                                                                "digest": "signal sent to outlet with index 2",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 0,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "inlet": 1,
                                                                "type": "signal"
                                                            },
                                                            "index": {
                                                                "attrOrProp": 2,
                                                                "digest": "outlet number",
                                                                "defaultarg": 1,
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "number",
                                                                "mandatory": 1
                                                            },
                                                            "comment": {
                                                                "attrOrProp": 2,
                                                                "digest": "mouse over comment",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol"
                                                            },
                                                            "meta": {
                                                                "attrOrProp": 2,
                                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                                "isalias": 0,
                                                                "aliases": [],
                                                                "settable": 1,
                                                                "attachable": 0,
                                                                "isparam": 0,
                                                                "deprecated": 0,
                                                                "touched": 0,
                                                                "type": "symbol",
                                                                "defaultValue": "",
                                                                "label": "Metadata",
                                                                "displayorder": 3
                                                            }
                                                        },
                                                        "inputs": [
                                                            {
                                                                "name": "in1",
                                                                "type": "signal",
                                                                "digest": "signal sent to outlet with index 2",
                                                                "displayName": "",
                                                                "hot": 1,
                                                                "docked": 0
                                                            }
                                                        ],
                                                        "outputs": [],
                                                        "helpname": "out~",
                                                        "aliasOf": "out~",
                                                        "classname": "out~",
                                                        "operator": 0,
                                                        "versionId": 1989326771,
                                                        "changesPatcherIO": 1
                                                    },
                                                    "text": "out~ 2"
                                                }
                                            }
                                        ],
                                        "lines": [
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-2", 0 ],
                                                    "source": [ "obj-1", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-162", 0 ],
                                                    "source": [ "obj-157", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-163", 0 ],
                                                    "source": [ "obj-158", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-185", 1 ],
                                                    "source": [ "obj-162", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-193", 1 ],
                                                    "source": [ "obj-163", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-167", 0 ],
                                                    "order": 1,
                                                    "source": [ "obj-166", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-168", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-166", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-157", 0 ],
                                                    "source": [ "obj-167", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-158", 0 ],
                                                    "source": [ "obj-168", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-178", 0 ],
                                                    "order": 1,
                                                    "source": [ "obj-173", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-186", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-173", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-185", 0 ],
                                                    "order": 1,
                                                    "source": [ "obj-176", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-193", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-176", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 0 ],
                                                    "source": [ "obj-178", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 1 ],
                                                    "source": [ "obj-181", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 2 ],
                                                    "source": [ "obj-182", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 3 ],
                                                    "source": [ "obj-183", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 4 ],
                                                    "source": [ "obj-184", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-204", 0 ],
                                                    "source": [ "obj-185", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-181", 0 ],
                                                    "order": 4,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-182", 0 ],
                                                    "order": 3,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-183", 0 ],
                                                    "order": 1,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-184", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-188", 0 ],
                                                    "order": 2,
                                                    "source": [ "obj-186", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-176", 0 ],
                                                    "source": [ "obj-188", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-205", 0 ],
                                                    "source": [ "obj-193", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-201", 0 ],
                                                    "source": [ "obj-194", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-166", 0 ],
                                                    "source": [ "obj-195", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-195", 1 ],
                                                    "source": [ "obj-196", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-196", 0 ],
                                                    "source": [ "obj-197", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-3", 0 ],
                                                    "source": [ "obj-2", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-197", 0 ],
                                                    "source": [ "obj-201", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-1", 0 ],
                                                    "order": 2,
                                                    "source": [ "obj-202", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-194", 0 ],
                                                    "order": 0,
                                                    "source": [ "obj-202", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-3", 1 ],
                                                    "order": 1,
                                                    "source": [ "obj-202", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-195", 0 ],
                                                    "source": [ "obj-203", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-173", 0 ],
                                                    "source": [ "obj-3", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-181", 1 ],
                                                    "source": [ "obj-4", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-182", 1 ],
                                                    "source": [ "obj-5", 0 ]
                                                }
                                            },
                                            {
                                                "patchline": {
                                                    "destination": [ "obj-2", 1 ],
                                                    "source": [ "obj-6", 0 ]
                                                }
                                            }
                                        ]
                                    },
                                    "patching_rect": [ 421.0, 255.0, 46.0, 23.0 ],
                                    "rnbo_classname": "p",
                                    "rnbo_extra_attributes": {
                                        "exposevoiceparams": 0,
                                        "args": [],
                                        "uidstyle": "auto",
                                        "polyphony": -1.0,
                                        "notecontroller": 0,
                                        "voicecontrol": "simple",
                                        "receivemode": "local"
                                    },
                                    "rnbo_serial": 3,
                                    "rnbo_uniqueid": "voice[2]",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "target": {
                                                "attrOrProp": 1,
                                                "digest": "target",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 0,
                                                "attachable": 1,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "defaultValue": "0"
                                            },
                                            "mute": {
                                                "attrOrProp": 1,
                                                "digest": "mute",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 1,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number"
                                            },
                                            "__probingout1": {
                                                "attrOrProp": 1,
                                                "digest": "__probingout1",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "signal"
                                            },
                                            "out1": {
                                                "attrOrProp": 1,
                                                "digest": "out1",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "outlet": 1,
                                                "type": "signal"
                                            },
                                            "out2": {
                                                "attrOrProp": 1,
                                                "digest": "out2",
                                                "isalias": 0,
                                                "aliases": [],
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "outlet": 1,
                                                "type": "signal"
                                            },
                                            "polyphony": {
                                                "attrOrProp": 2,
                                                "digest": "Polyphony of the subpatcher.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "defaultValue": "-1"
                                            },
                                            "exposevoiceparams": {
                                                "attrOrProp": 2,
                                                "digest": "Expose per voice versions of the contained parameters (only valid in polyphonic subpatchers).",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "bool",
                                                "defaultValue": "false"
                                            },
                                            "title": {
                                                "attrOrProp": 2,
                                                "digest": "Title of the subpatcher",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "doNotShowInMaxInspector": 1
                                            },
                                            "file": {
                                                "attrOrProp": 2,
                                                "digest": "rnbo file to load",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "doNotShowInMaxInspector": 1
                                            },
                                            "voicecontrol": {
                                                "attrOrProp": 2,
                                                "digest": "Chooses the way that polyphonic voices are controlled. 'simple' (or 'midi') will automatically allocate voices for \tincoming MIDI notes. Setting it to 'user' (or 'none') will switch off MIDI \tvoice allocation and start with all voices unmuted.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "enum": [ "simple", "user" ],
                                                "type": "enum",
                                                "defaultValue": "simple"
                                            },
                                            "notecontroller": {
                                                "attrOrProp": 2,
                                                "digest": "DEPRECATED. Use voicecontrol instead.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "doNotShowInMaxInspector": 1
                                            },
                                            "receivemode": {
                                                "attrOrProp": 2,
                                                "digest": "Do receive~ objects get the signal from a send~ inside the patcher directly (without latency), or compensated (with latency, aligned with all other voices).",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "enum": [ "local", "compensated" ],
                                                "type": "enum",
                                                "defaultValue": "local"
                                            },
                                            "args": {
                                                "attrOrProp": 2,
                                                "digest": "Replacement args for the subpatcher, everything named #1, #2 etc. will be replaced with the according argument.",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "doNotShowInMaxInspector": 1
                                            },
                                            "uidstyle": {
                                                "attrOrProp": 2,
                                                "digest": "Behavior of #0 unique ID. auto (default) means abstractions get a local UID, local: start a new local UID, parent: use the one from the parent patcher",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "enum": [ "auto", "local", "parent", "global" ],
                                                "type": "enum",
                                                "defaultValue": "auto"
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "in1",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in1",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            },
                                            {
                                                "name": "in2",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in2",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            },
                                            {
                                                "name": "in3",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in3",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            },
                                            {
                                                "name": "in4",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in4",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            },
                                            {
                                                "name": "in5",
                                                "type": [ "bang", "number", "list" ],
                                                "digest": "in5",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [
                                            {
                                                "name": "out1",
                                                "type": "signal",
                                                "digest": "out1",
                                                "displayName": "",
                                                "docked": 0
                                            },
                                            {
                                                "name": "out2",
                                                "type": "signal",
                                                "digest": "out2",
                                                "displayName": "",
                                                "docked": 0
                                            }
                                        ],
                                        "helpname": "patcher",
                                        "aliasOf": "rnbo",
                                        "classname": "p",
                                        "operator": 0,
                                        "versionId": 426236520,
                                        "changesPatcherIO": 0
                                    },
                                    "text": "p voice",
                                    "varname": "voice[2]"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-175",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 444.0, 167.0, 29.5, 23.0 ],
                                    "rnbo_classname": "message",
                                    "rnbo_extra_attributes": {
                                        "storeempty": 0,
                                        "text": ""
                                    },
                                    "rnbo_serial": 9,
                                    "rnbo_uniqueid": "message_obj-175",
                                    "text": "48"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-170",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 442.0, 33.0, 56.0, 23.0 ],
                                    "rnbo_classname": "loadbang",
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "loadbang_obj-170",
                                    "text": "loadbang"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-169",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 443.0, 116.0, 64.0, 23.0 ],
                                    "rnbo_classname": "metro",
                                    "rnbo_extra_attributes": {
                                        "lock": 0.0
                                    },
                                    "rnbo_serial": 3,
                                    "rnbo_uniqueid": "metro_obj-169",
                                    "text": "metro 16n"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-160",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [ 550.0, 621.0, 43.0, 23.0 ],
                                    "rnbo_classname": "out~",
                                    "rnbo_extra_attributes": {
                                        "comment": "",
                                        "meta": ""
                                    },
                                    "rnbo_serial": 1,
                                    "rnbo_uniqueid": "out~_obj-160",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "in1": {
                                                "attrOrProp": 1,
                                                "digest": "signal sent to outlet with index 2",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 0,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "inlet": 1,
                                                "type": "signal"
                                            },
                                            "index": {
                                                "attrOrProp": 2,
                                                "digest": "outlet number",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "mandatory": 1
                                            },
                                            "comment": {
                                                "attrOrProp": 2,
                                                "digest": "mouse over comment",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol"
                                            },
                                            "meta": {
                                                "attrOrProp": 2,
                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "defaultValue": "",
                                                "label": "Metadata",
                                                "displayorder": 3
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "in1",
                                                "type": "signal",
                                                "digest": "signal sent to outlet with index 2",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [],
                                        "helpname": "out~",
                                        "aliasOf": "out~",
                                        "classname": "out~",
                                        "operator": 0,
                                        "versionId": 1989326771,
                                        "changesPatcherIO": 1
                                    },
                                    "text": "out~ 2"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-159",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [ 435.0, 621.0, 43.0, 23.0 ],
                                    "rnbo_classname": "out~",
                                    "rnbo_extra_attributes": {
                                        "comment": "",
                                        "meta": ""
                                    },
                                    "rnbo_serial": 2,
                                    "rnbo_uniqueid": "out~_obj-159",
                                    "rnboinfo": {
                                        "needsInstanceInfo": 1,
                                        "argnames": {
                                            "in1": {
                                                "attrOrProp": 1,
                                                "digest": "signal sent to outlet with index 1",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 0,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "inlet": 1,
                                                "type": "signal"
                                            },
                                            "index": {
                                                "attrOrProp": 2,
                                                "digest": "outlet number",
                                                "defaultarg": 1,
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "number",
                                                "mandatory": 1
                                            },
                                            "comment": {
                                                "attrOrProp": 2,
                                                "digest": "mouse over comment",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol"
                                            },
                                            "meta": {
                                                "attrOrProp": 2,
                                                "digest": "A JSON formatted string containing metadata for use by the exported code",
                                                "isalias": 0,
                                                "aliases": [],
                                                "settable": 1,
                                                "attachable": 0,
                                                "isparam": 0,
                                                "deprecated": 0,
                                                "touched": 0,
                                                "type": "symbol",
                                                "defaultValue": "",
                                                "label": "Metadata",
                                                "displayorder": 3
                                            }
                                        },
                                        "inputs": [
                                            {
                                                "name": "in1",
                                                "type": "signal",
                                                "digest": "signal sent to outlet with index 1",
                                                "displayName": "",
                                                "hot": 1,
                                                "docked": 0
                                            }
                                        ],
                                        "outputs": [],
                                        "helpname": "out~",
                                        "aliasOf": "out~",
                                        "classname": "out~",
                                        "operator": 0,
                                        "versionId": 1989326771,
                                        "changesPatcherIO": 1
                                    },
                                    "text": "out~ 1"
                                }
                            }
                        ],
                        "lines": [
                            {
                                "patchline": {
                                    "destination": [ "obj-175", 0 ],
                                    "order": 1,
                                    "source": [ "obj-169", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-18", 0 ],
                                    "order": 0,
                                    "source": [ "obj-169", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-206", 0 ],
                                    "order": 2,
                                    "source": [ "obj-169", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-32", 0 ],
                                    "source": [ "obj-170", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-206", 1 ],
                                    "source": [ "obj-175", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-213", 2 ],
                                    "order": 1,
                                    "source": [ "obj-18", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-214", 2 ],
                                    "order": 0,
                                    "source": [ "obj-18", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-18", 0 ],
                                    "source": [ "obj-19", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-3", 0 ],
                                    "source": [ "obj-2", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-213", 0 ],
                                    "source": [ "obj-206", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-214", 0 ],
                                    "source": [ "obj-206", 1 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-213", 0 ],
                                    "source": [ "obj-207", 1 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-213", 0 ],
                                    "source": [ "obj-207", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-207", 1 ],
                                    "source": [ "obj-208", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-210", 1 ],
                                    "source": [ "obj-209", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-30", 0 ],
                                    "source": [ "obj-21", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-214", 0 ],
                                    "source": [ "obj-210", 1 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-214", 0 ],
                                    "source": [ "obj-210", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-207", 0 ],
                                    "order": 0,
                                    "source": [ "obj-211", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-208", 0 ],
                                    "order": 1,
                                    "source": [ "obj-211", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-209", 0 ],
                                    "order": 1,
                                    "source": [ "obj-212", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-210", 0 ],
                                    "order": 0,
                                    "source": [ "obj-212", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-238", 0 ],
                                    "source": [ "obj-213", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-237", 0 ],
                                    "source": [ "obj-214", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-216", 0 ],
                                    "source": [ "obj-215", 1 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-217", 1 ],
                                    "source": [ "obj-215", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-217", 0 ],
                                    "source": [ "obj-216", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-219", 0 ],
                                    "source": [ "obj-217", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-213", 1 ],
                                    "order": 1,
                                    "source": [ "obj-219", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-214", 1 ],
                                    "order": 0,
                                    "source": [ "obj-219", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-21", 0 ],
                                    "source": [ "obj-22", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-219", 0 ],
                                    "source": [ "obj-222", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-222", 0 ],
                                    "source": [ "obj-225", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-206", 2 ],
                                    "order": 2,
                                    "source": [ "obj-229", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-207", 2 ],
                                    "order": 1,
                                    "source": [ "obj-229", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-210", 2 ],
                                    "order": 0,
                                    "source": [ "obj-229", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-21", 1 ],
                                    "source": [ "obj-23", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-22", 0 ],
                                    "source": [ "obj-23", 1 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-229", 0 ],
                                    "source": [ "obj-230", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-230", 0 ],
                                    "source": [ "obj-231", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-230", 1 ],
                                    "source": [ "obj-232", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-231", 0 ],
                                    "source": [ "obj-232", 1 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-159", 0 ],
                                    "source": [ "obj-235", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-160", 0 ],
                                    "source": [ "obj-236", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-236", 0 ],
                                    "source": [ "obj-237", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-235", 0 ],
                                    "source": [ "obj-238", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-4", 0 ],
                                    "source": [ "obj-3", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-175", 0 ],
                                    "order": 2,
                                    "source": [ "obj-30", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-208", 0 ],
                                    "order": 1,
                                    "source": [ "obj-30", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-209", 0 ],
                                    "order": 0,
                                    "source": [ "obj-30", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-169", 0 ],
                                    "order": 2,
                                    "source": [ "obj-32", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-211", 0 ],
                                    "order": 1,
                                    "source": [ "obj-32", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-212", 0 ],
                                    "order": 0,
                                    "source": [ "obj-32", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-206", 2 ],
                                    "order": 2,
                                    "source": [ "obj-34", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-207", 2 ],
                                    "order": 1,
                                    "source": [ "obj-34", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-210", 2 ],
                                    "order": 0,
                                    "source": [ "obj-34", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-50", 0 ],
                                    "source": [ "obj-4", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-42", 0 ],
                                    "source": [ "obj-41", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-43", 0 ],
                                    "source": [ "obj-42", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-206", 4 ],
                                    "order": 2,
                                    "source": [ "obj-43", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-207", 4 ],
                                    "order": 1,
                                    "source": [ "obj-43", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-210", 4 ],
                                    "order": 0,
                                    "source": [ "obj-43", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-7", 0 ],
                                    "source": [ "obj-44", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-44", 0 ],
                                    "source": [ "obj-45", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-44", 1 ],
                                    "source": [ "obj-46", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-45", 0 ],
                                    "source": [ "obj-46", 1 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-49", 0 ],
                                    "source": [ "obj-48", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-50", 0 ],
                                    "source": [ "obj-49", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-43", 0 ],
                                    "source": [ "obj-5", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-206", 3 ],
                                    "order": 2,
                                    "source": [ "obj-50", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-207", 3 ],
                                    "order": 1,
                                    "source": [ "obj-50", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-210", 3 ],
                                    "order": 0,
                                    "source": [ "obj-50", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-2", 0 ],
                                    "source": [ "obj-51", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-51", 0 ],
                                    "source": [ "obj-52", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-51", 1 ],
                                    "source": [ "obj-53", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-52", 0 ],
                                    "source": [ "obj-53", 1 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-5", 0 ],
                                    "source": [ "obj-6", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-6", 0 ],
                                    "source": [ "obj-7", 0 ]
                                }
                            }
                        ]
                    },
                    "patching_rect": [ 52.0, 85.0, 63.0, 22.0 ],
                    "rnboattrcache": {                    },
                    "rnboversion": "1.4.4",
                    "saved_attribute_attributes": {
                        "valueof": {
                            "parameter_invisible": 1,
                            "parameter_longname": "rnbo~",
                            "parameter_modmode": 0,
                            "parameter_shortname": "rnbo~",
                            "parameter_type": 3
                        }
                    },
                    "saved_object_attributes": {
                        "optimization": "O1",
                        "parameter_enable": 1,
                        "uuid": "0e29398d-613b-11f1-b98d-da548cc1b95f"
                    },
                    "snapshot": {
                        "filetype": "C74Snapshot",
                        "version": 2,
                        "minorversion": 0,
                        "name": "snapshotlist",
                        "origin": "rnbo~",
                        "type": "list",
                        "subtype": "Undefined",
                        "embed": 1,
                        "snapshot": {
                            "__sps": {
                                "voice": {                                },
                                "voice[1]": {                                },
                                "voice[2]": {                                }
                            },
                            "__presetid": "0e29398d-613b-11f1-b98d-da548cc1b95f"
                        },
                        "snapshotlist": {
                            "current_snapshot": 0,
                            "entries": [
                                {
                                    "filetype": "C74Snapshot",
                                    "version": 2,
                                    "minorversion": 0,
                                    "name": "untitled",
                                    "origin": "0e29398d-613b-11f1-b98d-da548cc1b95f",
                                    "type": "rnbo",
                                    "subtype": "",
                                    "embed": 1,
                                    "snapshot": {
                                        "__sps": {
                                            "voice": {                                            },
                                            "voice[1]": {                                            },
                                            "voice[2]": {                                            }
                                        },
                                        "__presetid": "0e29398d-613b-11f1-b98d-da548cc1b95f"
                                    },
                                    "fileref": {
                                        "name": "untitled",
                                        "filename": "untitled_20260605_1.maxsnap",
                                        "filepath": "~/Documents/Max 9/Snapshots",
                                        "filepos": -1,
                                        "snapshotfileid": "97dc6b49bafd1aeebdbed98d6ad26bf6"
                                    }
                                }
                            ]
                        }
                    },
                    "text": "rnbo~ go1",
                    "varname": "rnbo~"
                }
            }
        ],
        "lines": [
            {
                "patchline": {
                    "destination": [ "obj-2", 1 ],
                    "source": [ "obj-1", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-2", 0 ],
                    "source": [ "obj-1", 0 ]
                }
            }
        ],
        "parameters": {
            "obj-1": [ "rnbo~", "rnbo~", 0 ],
            "parameterbanks": {
                "0": {
                    "index": 0,
                    "name": "",
                    "parameters": [ "-", "-", "-", "-", "-", "-", "-", "-" ],
                    "buttons": [ "-", "-", "-", "-", "-", "-", "-", "-" ]
                }
            },
            "inherited_shortname": 1
        },
        "autosave": 0
    }
}