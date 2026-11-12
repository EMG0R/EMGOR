{
  "patcher": {
    "fileversion": 1,
    "appversion": {
      "major": 9,
      "minor": 0,
      "revision": 8,
      "architecture": "x64",
      "modernui": 1
    },
    "classnamespace": "box",
    "rect": [
      60.0,
      80.0,
      900.0,
      640.0
    ],
    "openinpresentation": 1,
    "bglocked": 0,
    "bgcolor": [
      0.043,
      0.137,
      0.102,
      1.0
    ],
    "editing_bgcolor": [
      0.043,
      0.137,
      0.102,
      1.0
    ],
    "boxes": [
      {
        "box": {
          "id": "obj-1",
          "maxclass": "panel",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            0,
            0,
            380,
            169
          ],
          "presentation": 1,
          "presentation_rect": [
            0,
            0,
            380,
            169
          ],
          "bgcolor": [
            0.043,
            0.137,
            0.102,
            1.0
          ],
          "border": 0,
          "rounded": 0,
          "background": 1
        }
      },
      {
        "box": {
          "id": "obj-2",
          "maxclass": "panel",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            4,
            28,
            372,
            137
          ],
          "presentation": 1,
          "presentation_rect": [
            4,
            28,
            372,
            137
          ],
          "bgcolor": [
            0.035,
            0.294,
            0.29,
            1.0
          ],
          "border": 0,
          "rounded": 8,
          "background": 1
        }
      },
      {
        "box": {
          "id": "obj-3",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            10,
            5,
            150,
            20
          ],
          "text": "PMOR fx",
          "presentation": 1,
          "presentation_rect": [
            10,
            5,
            150,
            20
          ],
          "textcolor": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "fontsize": 13.0,
          "fontface": 1
        }
      },
      {
        "box": {
          "id": "obj-4",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            170,
            7,
            130,
            14
          ],
          "text": "ch 4 5 -> PMOR core",
          "presentation": 1,
          "presentation_rect": [
            170,
            7,
            130,
            14
          ],
          "textcolor": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "fontsize": 10.0,
          "fontface": 0
        }
      },
      {
        "box": {
          "id": "obj-5",
          "maxclass": "textbutton",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "",
            "",
            "int"
          ],
          "patching_rect": [
            304,
            5,
            66,
            20
          ],
          "text": "editor",
          "presentation": 1,
          "presentation_rect": [
            304,
            5,
            66,
            20
          ],
          "texton": "editor",
          "textcolor": [
            0.043,
            0.137,
            0.102,
            1.0
          ],
          "bgcolor": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "bgoncolor": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "fontsize": 11.0
        }
      },
      {
        "box": {
          "id": "obj-6",
          "maxclass": "message",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            20,
            788,
            180,
            22
          ],
          "text": "open"
        }
      },
      {
        "box": {
          "id": "obj-7",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            20,
            816,
            180,
            22
          ],
          "text": "p editor",
          "patcher": {
            "fileversion": 1,
            "appversion": {
              "major": 9,
              "minor": 0,
              "revision": 8,
              "architecture": "x64",
              "modernui": 1
            },
            "classnamespace": "box",
            "rect": [
              100.0,
              100.0,
              700.0,
              410.0
            ],
            "openinpresentation": 1,
            "bglocked": 0,
            "bgcolor": [
              0.043,
              0.137,
              0.102,
              1.0
            ],
            "editing_bgcolor": [
              0.043,
              0.137,
              0.102,
              1.0
            ],
            "boxes": [
              {
                "box": {
                  "id": "obj-1",
                  "maxclass": "panel",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    0,
                    0,
                    700,
                    410
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    0,
                    0,
                    700,
                    410
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "border": 0,
                  "rounded": 0,
                  "background": 1
                }
              },
              {
                "box": {
                  "id": "obj-2",
                  "maxclass": "panel",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    6,
                    6,
                    688,
                    398
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    6,
                    6,
                    688,
                    398
                  ],
                  "bgcolor": [
                    0.035,
                    0.294,
                    0.29,
                    1.0
                  ],
                  "border": 0,
                  "rounded": 8,
                  "background": 1
                }
              },
              {
                "box": {
                  "id": "obj-3",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    14,
                    12,
                    70,
                    20
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    14,
                    12,
                    70,
                    20
                  ],
                  "items": [
                    "alg 0",
                    ",",
                    "alg 1",
                    ",",
                    "alg 2",
                    ",",
                    "alg 3",
                    ",",
                    "alg 4",
                    ",",
                    "alg 5"
                  ],
                  "textcolor": [
                    0.45,
                    0.78,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "fontsize": 11.0,
                  "varname": "p5248"
                }
              },
              {
                "box": {
                  "id": "obj-4",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    788,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5248"
                }
              },
              {
                "box": {
                  "id": "obj-5",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    20,
                    816,
                    180,
                    22
                  ],
                  "text": "send pmor_tx"
                }
              },
              {
                "box": {
                  "id": "obj-6",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    90,
                    13,
                    120,
                    14
                  ],
                  "text": "distortion algo",
                  "presentation": 1,
                  "presentation_rect": [
                    90,
                    13,
                    120,
                    14
                  ],
                  "textcolor": [
                    0.45,
                    0.78,
                    1.0,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-7",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    12,
                    46,
                    44,
                    14
                  ],
                  "text": "bank 1",
                  "presentation": 1,
                  "presentation_rect": [
                    12,
                    46,
                    44,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-8",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    58,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5120"
                }
              },
              {
                "box": {
                  "id": "obj-9",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    844,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5120"
                }
              },
              {
                "box": {
                  "id": "obj-10",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    58,
                    61,
                    38,
                    14
                  ],
                  "text": "ST4",
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-11",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    97,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5121"
                }
              },
              {
                "box": {
                  "id": "obj-12",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    872,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5121"
                }
              },
              {
                "box": {
                  "id": "obj-13",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    97,
                    61,
                    38,
                    14
                  ],
                  "text": "ST8",
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-14",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    136,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5122"
                }
              },
              {
                "box": {
                  "id": "obj-15",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    900,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5122"
                }
              },
              {
                "box": {
                  "id": "obj-16",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    136,
                    61,
                    38,
                    14
                  ],
                  "text": "ST16",
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-17",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    175,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5123"
                }
              },
              {
                "box": {
                  "id": "obj-18",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    928,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5123"
                }
              },
              {
                "box": {
                  "id": "obj-19",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    175,
                    61,
                    38,
                    14
                  ],
                  "text": "ST32",
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-20",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    214,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5124"
                }
              },
              {
                "box": {
                  "id": "obj-21",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    956,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5124"
                }
              },
              {
                "box": {
                  "id": "obj-22",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    214,
                    61,
                    38,
                    14
                  ],
                  "text": "REV",
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-23",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    253,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5125"
                }
              },
              {
                "box": {
                  "id": "obj-24",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    984,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5125"
                }
              },
              {
                "box": {
                  "id": "obj-25",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    253,
                    61,
                    38,
                    14
                  ],
                  "text": "CHOR",
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-26",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    292,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5126"
                }
              },
              {
                "box": {
                  "id": "obj-27",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1012,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5126"
                }
              },
              {
                "box": {
                  "id": "obj-28",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    292,
                    61,
                    38,
                    14
                  ],
                  "text": "PAN",
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-29",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    331,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5127"
                }
              },
              {
                "box": {
                  "id": "obj-30",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1040,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5127"
                }
              },
              {
                "box": {
                  "id": "obj-31",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    331,
                    61,
                    38,
                    14
                  ],
                  "text": "CRUSH",
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-32",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    370,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5128"
                }
              },
              {
                "box": {
                  "id": "obj-33",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1068,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5128"
                }
              },
              {
                "box": {
                  "id": "obj-34",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    370,
                    61,
                    38,
                    14
                  ],
                  "text": "HALF",
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-35",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    409,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5129"
                }
              },
              {
                "box": {
                  "id": "obj-36",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1096,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5129"
                }
              },
              {
                "box": {
                  "id": "obj-37",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    409,
                    61,
                    38,
                    14
                  ],
                  "text": "DBL",
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-38",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    448,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5130"
                }
              },
              {
                "box": {
                  "id": "obj-39",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1124,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5130"
                }
              },
              {
                "box": {
                  "id": "obj-40",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    448,
                    61,
                    38,
                    14
                  ],
                  "text": "BZZ",
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-41",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    487,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5131"
                }
              },
              {
                "box": {
                  "id": "obj-42",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1152,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5131"
                }
              },
              {
                "box": {
                  "id": "obj-43",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    487,
                    61,
                    38,
                    14
                  ],
                  "text": "MUTE",
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-44",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    526,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5132"
                }
              },
              {
                "box": {
                  "id": "obj-45",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1180,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5132"
                }
              },
              {
                "box": {
                  "id": "obj-46",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    526,
                    61,
                    38,
                    14
                  ],
                  "text": "FLT",
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-47",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    565,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5133"
                }
              },
              {
                "box": {
                  "id": "obj-48",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1208,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5133"
                }
              },
              {
                "box": {
                  "id": "obj-49",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    565,
                    61,
                    38,
                    14
                  ],
                  "text": "WASH",
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-50",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    604,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5134"
                }
              },
              {
                "box": {
                  "id": "obj-51",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1236,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5134"
                }
              },
              {
                "box": {
                  "id": "obj-52",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    604,
                    61,
                    38,
                    14
                  ],
                  "text": "DIST",
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-53",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    643,
                    42,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    42,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5135"
                }
              },
              {
                "box": {
                  "id": "obj-54",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1264,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5135"
                }
              },
              {
                "box": {
                  "id": "obj-55",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    643,
                    61,
                    38,
                    14
                  ],
                  "text": "STOP",
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    61,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-56",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    12,
                    91,
                    44,
                    14
                  ],
                  "text": "bank 2",
                  "presentation": 1,
                  "presentation_rect": [
                    12,
                    91,
                    44,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-57",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    58,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5136"
                }
              },
              {
                "box": {
                  "id": "obj-58",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1292,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5136"
                }
              },
              {
                "box": {
                  "id": "obj-59",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    58,
                    106,
                    38,
                    14
                  ],
                  "text": "DUB",
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-60",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    97,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5137"
                }
              },
              {
                "box": {
                  "id": "obj-61",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1320,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5137"
                }
              },
              {
                "box": {
                  "id": "obj-62",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    97,
                    106,
                    38,
                    14
                  ],
                  "text": "GVERB",
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-63",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    136,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5138"
                }
              },
              {
                "box": {
                  "id": "obj-64",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1348,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5138"
                }
              },
              {
                "box": {
                  "id": "obj-65",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    136,
                    106,
                    38,
                    14
                  ],
                  "text": "TGATE",
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-66",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    175,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5139"
                }
              },
              {
                "box": {
                  "id": "obj-67",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1376,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5139"
                }
              },
              {
                "box": {
                  "id": "obj-68",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    175,
                    106,
                    38,
                    14
                  ],
                  "text": "FLANG",
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-69",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    214,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5140"
                }
              },
              {
                "box": {
                  "id": "obj-70",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1404,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5140"
                }
              },
              {
                "box": {
                  "id": "obj-71",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    214,
                    106,
                    38,
                    14
                  ],
                  "text": "RING",
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-72",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    253,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5141"
                }
              },
              {
                "box": {
                  "id": "obj-73",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    760,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5141"
                }
              },
              {
                "box": {
                  "id": "obj-74",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    253,
                    106,
                    38,
                    14
                  ],
                  "text": "FOLD",
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-75",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    292,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5142"
                }
              },
              {
                "box": {
                  "id": "obj-76",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    788,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5142"
                }
              },
              {
                "box": {
                  "id": "obj-77",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    292,
                    106,
                    38,
                    14
                  ],
                  "text": "COMB",
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-78",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    331,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5143"
                }
              },
              {
                "box": {
                  "id": "obj-79",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    816,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5143"
                }
              },
              {
                "box": {
                  "id": "obj-80",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    331,
                    106,
                    38,
                    14
                  ],
                  "text": "VOWEL",
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-81",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    370,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5144"
                }
              },
              {
                "box": {
                  "id": "obj-82",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    844,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5144"
                }
              },
              {
                "box": {
                  "id": "obj-83",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    370,
                    106,
                    38,
                    14
                  ],
                  "text": "ISO-LO",
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-84",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    409,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5145"
                }
              },
              {
                "box": {
                  "id": "obj-85",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    872,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5145"
                }
              },
              {
                "box": {
                  "id": "obj-86",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    409,
                    106,
                    38,
                    14
                  ],
                  "text": "ISO-MD",
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-87",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    448,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5146"
                }
              },
              {
                "box": {
                  "id": "obj-88",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    900,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5146"
                }
              },
              {
                "box": {
                  "id": "obj-89",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    448,
                    106,
                    38,
                    14
                  ],
                  "text": "ISO-HI",
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-90",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    487,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5147"
                }
              },
              {
                "box": {
                  "id": "obj-91",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    928,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5147"
                }
              },
              {
                "box": {
                  "id": "obj-92",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    487,
                    106,
                    38,
                    14
                  ],
                  "text": "PHONE",
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-93",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    526,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5148"
                }
              },
              {
                "box": {
                  "id": "obj-94",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    956,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5148"
                }
              },
              {
                "box": {
                  "id": "obj-95",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    526,
                    106,
                    38,
                    14
                  ],
                  "text": "GRAIN",
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-96",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    565,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5149"
                }
              },
              {
                "box": {
                  "id": "obj-97",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    984,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5149"
                }
              },
              {
                "box": {
                  "id": "obj-98",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    565,
                    106,
                    38,
                    14
                  ],
                  "text": "WOW",
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-99",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    604,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5150"
                }
              },
              {
                "box": {
                  "id": "obj-100",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1012,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5150"
                }
              },
              {
                "box": {
                  "id": "obj-101",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    604,
                    106,
                    38,
                    14
                  ],
                  "text": "DOWN",
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-102",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    643,
                    87,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    87,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5151"
                }
              },
              {
                "box": {
                  "id": "obj-103",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1040,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5151"
                }
              },
              {
                "box": {
                  "id": "obj-104",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    643,
                    106,
                    38,
                    14
                  ],
                  "text": "SCAT",
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    106,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-105",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    12,
                    136,
                    44,
                    14
                  ],
                  "text": "bank 3",
                  "presentation": 1,
                  "presentation_rect": [
                    12,
                    136,
                    44,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-106",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    58,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5152"
                }
              },
              {
                "box": {
                  "id": "obj-107",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1068,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5152"
                }
              },
              {
                "box": {
                  "id": "obj-108",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    58,
                    151,
                    38,
                    14
                  ],
                  "text": "PLATE",
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-109",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    97,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5153"
                }
              },
              {
                "box": {
                  "id": "obj-110",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1096,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5153"
                }
              },
              {
                "box": {
                  "id": "obj-111",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    97,
                    151,
                    38,
                    14
                  ],
                  "text": "SHIMR",
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-112",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    136,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5154"
                }
              },
              {
                "box": {
                  "id": "obj-113",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1124,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5154"
                }
              },
              {
                "box": {
                  "id": "obj-114",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    136,
                    151,
                    38,
                    14
                  ],
                  "text": "PONG",
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-115",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    175,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5155"
                }
              },
              {
                "box": {
                  "id": "obj-116",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1152,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5155"
                }
              },
              {
                "box": {
                  "id": "obj-117",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    175,
                    151,
                    38,
                    14
                  ],
                  "text": "SLAP",
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-118",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    214,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5156"
                }
              },
              {
                "box": {
                  "id": "obj-119",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1180,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5156"
                }
              },
              {
                "box": {
                  "id": "obj-120",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    214,
                    151,
                    38,
                    14
                  ],
                  "text": "PHASE",
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-121",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    253,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5157"
                }
              },
              {
                "box": {
                  "id": "obj-122",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1208,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5157"
                }
              },
              {
                "box": {
                  "id": "obj-123",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    253,
                    151,
                    38,
                    14
                  ],
                  "text": "WAH",
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-124",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    292,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5158"
                }
              },
              {
                "box": {
                  "id": "obj-125",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1236,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5158"
                }
              },
              {
                "box": {
                  "id": "obj-126",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    292,
                    151,
                    38,
                    14
                  ],
                  "text": "TREM",
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-127",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    331,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5159"
                }
              },
              {
                "box": {
                  "id": "obj-128",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1264,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5159"
                }
              },
              {
                "box": {
                  "id": "obj-129",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    331,
                    151,
                    38,
                    14
                  ],
                  "text": "HAAS",
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-130",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    370,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5160"
                }
              },
              {
                "box": {
                  "id": "obj-131",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1292,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5160"
                }
              },
              {
                "box": {
                  "id": "obj-132",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    370,
                    151,
                    38,
                    14
                  ],
                  "text": "VIBRA",
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-133",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    409,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5161"
                }
              },
              {
                "box": {
                  "id": "obj-134",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1320,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5161"
                }
              },
              {
                "box": {
                  "id": "obj-135",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    409,
                    151,
                    38,
                    14
                  ],
                  "text": "DIMEN",
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-136",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    448,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5162"
                }
              },
              {
                "box": {
                  "id": "obj-137",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1348,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5162"
                }
              },
              {
                "box": {
                  "id": "obj-138",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    448,
                    151,
                    38,
                    14
                  ],
                  "text": "SPRNG",
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-139",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    487,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5163"
                }
              },
              {
                "box": {
                  "id": "obj-140",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1376,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5163"
                }
              },
              {
                "box": {
                  "id": "obj-141",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    487,
                    151,
                    38,
                    14
                  ],
                  "text": "SIREN",
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-142",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    526,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5164"
                }
              },
              {
                "box": {
                  "id": "obj-143",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1404,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5164"
                }
              },
              {
                "box": {
                  "id": "obj-144",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    526,
                    151,
                    38,
                    14
                  ],
                  "text": "WARM",
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-145",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    565,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5165"
                }
              },
              {
                "box": {
                  "id": "obj-146",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    760,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5165"
                }
              },
              {
                "box": {
                  "id": "obj-147",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    565,
                    151,
                    38,
                    14
                  ],
                  "text": "SUB",
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-148",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    604,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5166"
                }
              },
              {
                "box": {
                  "id": "obj-149",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    788,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5166"
                }
              },
              {
                "box": {
                  "id": "obj-150",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    604,
                    151,
                    38,
                    14
                  ],
                  "text": "AIR",
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-151",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    643,
                    132,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    132,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5167"
                }
              },
              {
                "box": {
                  "id": "obj-152",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    816,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5167"
                }
              },
              {
                "box": {
                  "id": "obj-153",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    643,
                    151,
                    38,
                    14
                  ],
                  "text": "PUMP",
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    151,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-154",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    12,
                    181,
                    44,
                    14
                  ],
                  "text": "bank 4",
                  "presentation": 1,
                  "presentation_rect": [
                    12,
                    181,
                    44,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-155",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    58,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5168"
                }
              },
              {
                "box": {
                  "id": "obj-156",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    844,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5168"
                }
              },
              {
                "box": {
                  "id": "obj-157",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    58,
                    196,
                    38,
                    14
                  ],
                  "text": "OCT+",
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-158",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    97,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5169"
                }
              },
              {
                "box": {
                  "id": "obj-159",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    872,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5169"
                }
              },
              {
                "box": {
                  "id": "obj-160",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    97,
                    196,
                    38,
                    14
                  ],
                  "text": "OCT-",
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-161",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    136,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5170"
                }
              },
              {
                "box": {
                  "id": "obj-162",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    900,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5170"
                }
              },
              {
                "box": {
                  "id": "obj-163",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    136,
                    196,
                    38,
                    14
                  ],
                  "text": "DSTRY",
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-164",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    175,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5171"
                }
              },
              {
                "box": {
                  "id": "obj-165",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    928,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5171"
                }
              },
              {
                "box": {
                  "id": "obj-166",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    175,
                    196,
                    38,
                    14
                  ],
                  "text": "REPT",
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-167",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    214,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5172"
                }
              },
              {
                "box": {
                  "id": "obj-168",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    956,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5172"
                }
              },
              {
                "box": {
                  "id": "obj-169",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    214,
                    196,
                    38,
                    14
                  ],
                  "text": "RGRN",
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-170",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    253,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5173"
                }
              },
              {
                "box": {
                  "id": "obj-171",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    984,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5173"
                }
              },
              {
                "box": {
                  "id": "obj-172",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    253,
                    196,
                    38,
                    14
                  ],
                  "text": "FREEZ",
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-173",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    292,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5174"
                }
              },
              {
                "box": {
                  "id": "obj-174",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1012,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5174"
                }
              },
              {
                "box": {
                  "id": "obj-175",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    292,
                    196,
                    38,
                    14
                  ],
                  "text": "METAL",
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-176",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    331,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5175"
                }
              },
              {
                "box": {
                  "id": "obj-177",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1040,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5175"
                }
              },
              {
                "box": {
                  "id": "obj-178",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    331,
                    196,
                    38,
                    14
                  ],
                  "text": "FRMNT",
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-179",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    370,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5176"
                }
              },
              {
                "box": {
                  "id": "obj-180",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1068,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5176"
                }
              },
              {
                "box": {
                  "id": "obj-181",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    370,
                    196,
                    38,
                    14
                  ],
                  "text": "FSHFT",
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-182",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    409,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5177"
                }
              },
              {
                "box": {
                  "id": "obj-183",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1096,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5177"
                }
              },
              {
                "box": {
                  "id": "obj-184",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    409,
                    196,
                    38,
                    14
                  ],
                  "text": "FUZZ",
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-185",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    448,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5178"
                }
              },
              {
                "box": {
                  "id": "obj-186",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1124,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5178"
                }
              },
              {
                "box": {
                  "id": "obj-187",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    448,
                    196,
                    38,
                    14
                  ],
                  "text": "GSEQ",
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-188",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    487,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5179"
                }
              },
              {
                "box": {
                  "id": "obj-189",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1152,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5179"
                }
              },
              {
                "box": {
                  "id": "obj-190",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    487,
                    196,
                    38,
                    14
                  ],
                  "text": "SRAMP",
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-191",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    526,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5180"
                }
              },
              {
                "box": {
                  "id": "obj-192",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1180,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5180"
                }
              },
              {
                "box": {
                  "id": "obj-193",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    526,
                    196,
                    38,
                    14
                  ],
                  "text": "VSTOP",
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-194",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    565,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5181"
                }
              },
              {
                "box": {
                  "id": "obj-195",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1208,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5181"
                }
              },
              {
                "box": {
                  "id": "obj-196",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    565,
                    196,
                    38,
                    14
                  ],
                  "text": "RADIO",
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-197",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    604,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5182"
                }
              },
              {
                "box": {
                  "id": "obj-198",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1236,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5182"
                }
              },
              {
                "box": {
                  "id": "obj-199",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    604,
                    196,
                    38,
                    14
                  ],
                  "text": "NGATE",
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-200",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    643,
                    177,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    177,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5183"
                }
              },
              {
                "box": {
                  "id": "obj-201",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1264,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5183"
                }
              },
              {
                "box": {
                  "id": "obj-202",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    643,
                    196,
                    38,
                    14
                  ],
                  "text": "SHMOD",
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    196,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-203",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    12,
                    226,
                    44,
                    14
                  ],
                  "text": "bank 5",
                  "presentation": 1,
                  "presentation_rect": [
                    12,
                    226,
                    44,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-204",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    58,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5184"
                }
              },
              {
                "box": {
                  "id": "obj-205",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1292,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5184"
                }
              },
              {
                "box": {
                  "id": "obj-206",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    58,
                    241,
                    38,
                    14
                  ],
                  "text": "RPT2",
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-207",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    97,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5185"
                }
              },
              {
                "box": {
                  "id": "obj-208",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1320,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5185"
                }
              },
              {
                "box": {
                  "id": "obj-209",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    97,
                    241,
                    38,
                    14
                  ],
                  "text": "RPT6",
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-210",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    136,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5186"
                }
              },
              {
                "box": {
                  "id": "obj-211",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1348,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5186"
                }
              },
              {
                "box": {
                  "id": "obj-212",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    136,
                    241,
                    38,
                    14
                  ],
                  "text": "SHUF",
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-213",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    175,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5187"
                }
              },
              {
                "box": {
                  "id": "obj-214",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1376,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5187"
                }
              },
              {
                "box": {
                  "id": "obj-215",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    175,
                    241,
                    38,
                    14
                  ],
                  "text": "Q6/8",
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-216",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    214,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5188"
                }
              },
              {
                "box": {
                  "id": "obj-217",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1404,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5188"
                }
              },
              {
                "box": {
                  "id": "obj-218",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    214,
                    241,
                    38,
                    14
                  ],
                  "text": "PUNCH",
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-219",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    253,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5189"
                }
              },
              {
                "box": {
                  "id": "obj-220",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    760,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5189"
                }
              },
              {
                "box": {
                  "id": "obj-221",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    253,
                    241,
                    38,
                    14
                  ],
                  "text": "FILL",
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-222",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    292,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5190"
                }
              },
              {
                "box": {
                  "id": "obj-223",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    788,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5190"
                }
              },
              {
                "box": {
                  "id": "obj-224",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    292,
                    241,
                    38,
                    14
                  ],
                  "text": "BUILD",
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-225",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    331,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5191"
                }
              },
              {
                "box": {
                  "id": "obj-226",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    816,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5191"
                }
              },
              {
                "box": {
                  "id": "obj-227",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    331,
                    241,
                    38,
                    14
                  ],
                  "text": "BALL",
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-228",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    370,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5192"
                }
              },
              {
                "box": {
                  "id": "obj-229",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    844,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5192"
                }
              },
              {
                "box": {
                  "id": "obj-230",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    370,
                    241,
                    38,
                    14
                  ],
                  "text": "LPF",
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-231",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    409,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5193"
                }
              },
              {
                "box": {
                  "id": "obj-232",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    872,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5193"
                }
              },
              {
                "box": {
                  "id": "obj-233",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    409,
                    241,
                    38,
                    14
                  ],
                  "text": "HPF",
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-234",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    448,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5194"
                }
              },
              {
                "box": {
                  "id": "obj-235",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    900,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5194"
                }
              },
              {
                "box": {
                  "id": "obj-236",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    448,
                    241,
                    38,
                    14
                  ],
                  "text": "SQUSH",
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-237",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    487,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5195"
                }
              },
              {
                "box": {
                  "id": "obj-238",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    928,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5195"
                }
              },
              {
                "box": {
                  "id": "obj-239",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    487,
                    241,
                    38,
                    14
                  ],
                  "text": "SCRTC",
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-240",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    526,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5196"
                }
              },
              {
                "box": {
                  "id": "obj-241",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    956,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5196"
                }
              },
              {
                "box": {
                  "id": "obj-242",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    526,
                    241,
                    38,
                    14
                  ],
                  "text": "SCRTF",
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-243",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    565,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5197"
                }
              },
              {
                "box": {
                  "id": "obj-244",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    984,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5197"
                }
              },
              {
                "box": {
                  "id": "obj-245",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    565,
                    241,
                    38,
                    14
                  ],
                  "text": "GLCH",
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-246",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    604,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5198"
                }
              },
              {
                "box": {
                  "id": "obj-247",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1012,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5198"
                }
              },
              {
                "box": {
                  "id": "obj-248",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    604,
                    241,
                    38,
                    14
                  ],
                  "text": "BLIND",
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-249",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    643,
                    222,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    222,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5199"
                }
              },
              {
                "box": {
                  "id": "obj-250",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1040,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5199"
                }
              },
              {
                "box": {
                  "id": "obj-251",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    643,
                    241,
                    38,
                    14
                  ],
                  "text": "S&H",
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    241,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-252",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    12,
                    271,
                    44,
                    14
                  ],
                  "text": "bank 6",
                  "presentation": 1,
                  "presentation_rect": [
                    12,
                    271,
                    44,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-253",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    58,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5200"
                }
              },
              {
                "box": {
                  "id": "obj-254",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1068,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5200"
                }
              },
              {
                "box": {
                  "id": "obj-255",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    58,
                    286,
                    38,
                    14
                  ],
                  "text": "DELAY",
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-256",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    97,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5201"
                }
              },
              {
                "box": {
                  "id": "obj-257",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1096,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5201"
                }
              },
              {
                "box": {
                  "id": "obj-258",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    97,
                    286,
                    38,
                    14
                  ],
                  "text": "PNGPG",
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-259",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    136,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5202"
                }
              },
              {
                "box": {
                  "id": "obj-260",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1124,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5202"
                }
              },
              {
                "box": {
                  "id": "obj-261",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    136,
                    286,
                    38,
                    14
                  ],
                  "text": "FADE",
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-262",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    175,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5203"
                }
              },
              {
                "box": {
                  "id": "obj-263",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1152,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5203"
                }
              },
              {
                "box": {
                  "id": "obj-264",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    175,
                    286,
                    38,
                    14
                  ],
                  "text": "FBACK",
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-265",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    214,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5204"
                }
              },
              {
                "box": {
                  "id": "obj-266",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1180,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5204"
                }
              },
              {
                "box": {
                  "id": "obj-267",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    214,
                    286,
                    38,
                    14
                  ],
                  "text": "DRONE",
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-268",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    253,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5205"
                }
              },
              {
                "box": {
                  "id": "obj-269",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1208,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5205"
                }
              },
              {
                "box": {
                  "id": "obj-270",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    253,
                    286,
                    38,
                    14
                  ],
                  "text": "FREEZ",
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-271",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    292,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5206"
                }
              },
              {
                "box": {
                  "id": "obj-272",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1236,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5206"
                }
              },
              {
                "box": {
                  "id": "obj-273",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    292,
                    286,
                    38,
                    14
                  ],
                  "text": "UNI",
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-274",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    331,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5207"
                }
              },
              {
                "box": {
                  "id": "obj-275",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1264,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5207"
                }
              },
              {
                "box": {
                  "id": "obj-276",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    331,
                    286,
                    38,
                    14
                  ],
                  "text": "UNILO",
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-277",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    370,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5208"
                }
              },
              {
                "box": {
                  "id": "obj-278",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1292,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5208"
                }
              },
              {
                "box": {
                  "id": "obj-279",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    370,
                    286,
                    38,
                    14
                  ],
                  "text": "DETUN",
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-280",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    409,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5209"
                }
              },
              {
                "box": {
                  "id": "obj-281",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1320,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5209"
                }
              },
              {
                "box": {
                  "id": "obj-282",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    409,
                    286,
                    38,
                    14
                  ],
                  "text": "PLFO",
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-283",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    448,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5210"
                }
              },
              {
                "box": {
                  "id": "obj-284",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1348,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5210"
                }
              },
              {
                "box": {
                  "id": "obj-285",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    448,
                    286,
                    38,
                    14
                  ],
                  "text": "BND+",
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-286",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    487,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5211"
                }
              },
              {
                "box": {
                  "id": "obj-287",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1376,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5211"
                }
              },
              {
                "box": {
                  "id": "obj-288",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    487,
                    286,
                    38,
                    14
                  ],
                  "text": "BND-",
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-289",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    526,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5212"
                }
              },
              {
                "box": {
                  "id": "obj-290",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    590,
                    1404,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5212"
                }
              },
              {
                "box": {
                  "id": "obj-291",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    526,
                    286,
                    38,
                    14
                  ],
                  "text": "WARBL",
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-292",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    565,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5213"
                }
              },
              {
                "box": {
                  "id": "obj-293",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    760,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5213"
                }
              },
              {
                "box": {
                  "id": "obj-294",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    565,
                    286,
                    38,
                    14
                  ],
                  "text": "5TH",
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-295",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    604,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5214"
                }
              },
              {
                "box": {
                  "id": "obj-296",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    788,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5214"
                }
              },
              {
                "box": {
                  "id": "obj-297",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    604,
                    286,
                    38,
                    14
                  ],
                  "text": "SUB",
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-298",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    643,
                    267,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    267,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5215"
                }
              },
              {
                "box": {
                  "id": "obj-299",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    816,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5215"
                }
              },
              {
                "box": {
                  "id": "obj-300",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    643,
                    286,
                    38,
                    14
                  ],
                  "text": "LOOP",
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    286,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-301",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    12,
                    316,
                    44,
                    14
                  ],
                  "text": "bank 7",
                  "presentation": 1,
                  "presentation_rect": [
                    12,
                    316,
                    44,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-302",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    58,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5216"
                }
              },
              {
                "box": {
                  "id": "obj-303",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    844,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5216"
                }
              },
              {
                "box": {
                  "id": "obj-304",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    58,
                    331,
                    38,
                    14
                  ],
                  "text": "GATE4",
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-305",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    97,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5217"
                }
              },
              {
                "box": {
                  "id": "obj-306",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    872,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5217"
                }
              },
              {
                "box": {
                  "id": "obj-307",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    97,
                    331,
                    38,
                    14
                  ],
                  "text": "GATE8",
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-308",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    136,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5218"
                }
              },
              {
                "box": {
                  "id": "obj-309",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    900,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5218"
                }
              },
              {
                "box": {
                  "id": "obj-310",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    136,
                    331,
                    38,
                    14
                  ],
                  "text": "GAT16",
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-311",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    175,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5219"
                }
              },
              {
                "box": {
                  "id": "obj-312",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    928,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5219"
                }
              },
              {
                "box": {
                  "id": "obj-313",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    175,
                    331,
                    38,
                    14
                  ],
                  "text": "TRNCE",
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-314",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    214,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5220"
                }
              },
              {
                "box": {
                  "id": "obj-315",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    956,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5220"
                }
              },
              {
                "box": {
                  "id": "obj-316",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    214,
                    331,
                    38,
                    14
                  ],
                  "text": "TREM",
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-317",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    253,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5221"
                }
              },
              {
                "box": {
                  "id": "obj-318",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    984,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5221"
                }
              },
              {
                "box": {
                  "id": "obj-319",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    253,
                    331,
                    38,
                    14
                  ],
                  "text": "PAN",
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-320",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    292,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5222"
                }
              },
              {
                "box": {
                  "id": "obj-321",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1012,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5222"
                }
              },
              {
                "box": {
                  "id": "obj-322",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    292,
                    331,
                    38,
                    14
                  ],
                  "text": "PUMP",
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-323",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    331,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5223"
                }
              },
              {
                "box": {
                  "id": "obj-324",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1040,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5223"
                }
              },
              {
                "box": {
                  "id": "obj-325",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    331,
                    331,
                    38,
                    14
                  ],
                  "text": "SWELL",
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-326",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    370,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5224"
                }
              },
              {
                "box": {
                  "id": "obj-327",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1068,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5224"
                }
              },
              {
                "box": {
                  "id": "obj-328",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    370,
                    331,
                    38,
                    14
                  ],
                  "text": "NOISE",
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-329",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    409,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5225"
                }
              },
              {
                "box": {
                  "id": "obj-330",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1096,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5225"
                }
              },
              {
                "box": {
                  "id": "obj-331",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    409,
                    331,
                    38,
                    14
                  ],
                  "text": "WIDE",
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-332",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    448,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5226"
                }
              },
              {
                "box": {
                  "id": "obj-333",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1124,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5226"
                }
              },
              {
                "box": {
                  "id": "obj-334",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    448,
                    331,
                    38,
                    14
                  ],
                  "text": "MONO",
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-335",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    487,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5227"
                }
              },
              {
                "box": {
                  "id": "obj-336",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1152,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5227"
                }
              },
              {
                "box": {
                  "id": "obj-337",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    487,
                    331,
                    38,
                    14
                  ],
                  "text": "BRITE",
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-338",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    526,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5228"
                }
              },
              {
                "box": {
                  "id": "obj-339",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1180,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5228"
                }
              },
              {
                "box": {
                  "id": "obj-340",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    526,
                    331,
                    38,
                    14
                  ],
                  "text": "DARK",
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-341",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    565,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5229"
                }
              },
              {
                "box": {
                  "id": "obj-342",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1208,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5229"
                }
              },
              {
                "box": {
                  "id": "obj-343",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    565,
                    331,
                    38,
                    14
                  ],
                  "text": "DRIVE",
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-344",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    604,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5230"
                }
              },
              {
                "box": {
                  "id": "obj-345",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1236,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5230"
                }
              },
              {
                "box": {
                  "id": "obj-346",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    604,
                    331,
                    38,
                    14
                  ],
                  "text": "SGATE",
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-347",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    643,
                    312,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    312,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "varname": "p5231"
                }
              },
              {
                "box": {
                  "id": "obj-348",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1264,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5231"
                }
              },
              {
                "box": {
                  "id": "obj-349",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    643,
                    331,
                    38,
                    14
                  ],
                  "text": "KILL",
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    331,
                    38,
                    14
                  ],
                  "textcolor": [
                    0.6,
                    0.965,
                    0.62,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-350",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    12,
                    361,
                    44,
                    14
                  ],
                  "text": "bank 8",
                  "presentation": 1,
                  "presentation_rect": [
                    12,
                    361,
                    44,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-351",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    58,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5232"
                }
              },
              {
                "box": {
                  "id": "obj-352",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1292,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5232"
                }
              },
              {
                "box": {
                  "id": "obj-353",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    58,
                    376,
                    38,
                    14
                  ],
                  "text": "RAT2",
                  "presentation": 1,
                  "presentation_rect": [
                    58,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-354",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    97,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5233"
                }
              },
              {
                "box": {
                  "id": "obj-355",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1320,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5233"
                }
              },
              {
                "box": {
                  "id": "obj-356",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    97,
                    376,
                    38,
                    14
                  ],
                  "text": "RAT3",
                  "presentation": 1,
                  "presentation_rect": [
                    97,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-357",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    136,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5234"
                }
              },
              {
                "box": {
                  "id": "obj-358",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1348,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5234"
                }
              },
              {
                "box": {
                  "id": "obj-359",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    136,
                    376,
                    38,
                    14
                  ],
                  "text": "RAT4",
                  "presentation": 1,
                  "presentation_rect": [
                    136,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-360",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    175,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5235"
                }
              },
              {
                "box": {
                  "id": "obj-361",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1376,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5235"
                }
              },
              {
                "box": {
                  "id": "obj-362",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    175,
                    376,
                    38,
                    14
                  ],
                  "text": "RAT8",
                  "presentation": 1,
                  "presentation_rect": [
                    175,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-363",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    214,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5236"
                }
              },
              {
                "box": {
                  "id": "obj-364",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    780,
                    1404,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5236"
                }
              },
              {
                "box": {
                  "id": "obj-365",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    214,
                    376,
                    38,
                    14
                  ],
                  "text": "STRUM",
                  "presentation": 1,
                  "presentation_rect": [
                    214,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-366",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    253,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5237"
                }
              },
              {
                "box": {
                  "id": "obj-367",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    760,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5237"
                }
              },
              {
                "box": {
                  "id": "obj-368",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    253,
                    376,
                    38,
                    14
                  ],
                  "text": "TRNC2",
                  "presentation": 1,
                  "presentation_rect": [
                    253,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-369",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    292,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5238"
                }
              },
              {
                "box": {
                  "id": "obj-370",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    788,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5238"
                }
              },
              {
                "box": {
                  "id": "obj-371",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    292,
                    376,
                    38,
                    14
                  ],
                  "text": "STAB",
                  "presentation": 1,
                  "presentation_rect": [
                    292,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-372",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    331,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5239"
                }
              },
              {
                "box": {
                  "id": "obj-373",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    816,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5239"
                }
              },
              {
                "box": {
                  "id": "obj-374",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    331,
                    376,
                    38,
                    14
                  ],
                  "text": "RINGL",
                  "presentation": 1,
                  "presentation_rect": [
                    331,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-375",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    370,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5240"
                }
              },
              {
                "box": {
                  "id": "obj-376",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    844,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5240"
                }
              },
              {
                "box": {
                  "id": "obj-377",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    370,
                    376,
                    38,
                    14
                  ],
                  "text": "RINGH",
                  "presentation": 1,
                  "presentation_rect": [
                    370,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-378",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    409,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5241"
                }
              },
              {
                "box": {
                  "id": "obj-379",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    872,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5241"
                }
              },
              {
                "box": {
                  "id": "obj-380",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    409,
                    376,
                    38,
                    14
                  ],
                  "text": "VIBR",
                  "presentation": 1,
                  "presentation_rect": [
                    409,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-381",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    448,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5242"
                }
              },
              {
                "box": {
                  "id": "obj-382",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    900,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5242"
                }
              },
              {
                "box": {
                  "id": "obj-383",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    448,
                    376,
                    38,
                    14
                  ],
                  "text": "CHOP",
                  "presentation": 1,
                  "presentation_rect": [
                    448,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-384",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    487,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5243"
                }
              },
              {
                "box": {
                  "id": "obj-385",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    928,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5243"
                }
              },
              {
                "box": {
                  "id": "obj-386",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    487,
                    376,
                    38,
                    14
                  ],
                  "text": "HALF",
                  "presentation": 1,
                  "presentation_rect": [
                    487,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-387",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    526,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5244"
                }
              },
              {
                "box": {
                  "id": "obj-388",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    956,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5244"
                }
              },
              {
                "box": {
                  "id": "obj-389",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    526,
                    376,
                    38,
                    14
                  ],
                  "text": "DBL",
                  "presentation": 1,
                  "presentation_rect": [
                    526,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-390",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    565,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5245"
                }
              },
              {
                "box": {
                  "id": "obj-391",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    984,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5245"
                }
              },
              {
                "box": {
                  "id": "obj-392",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    565,
                    376,
                    38,
                    14
                  ],
                  "text": "GLCH",
                  "presentation": 1,
                  "presentation_rect": [
                    565,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-393",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    604,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5246"
                }
              },
              {
                "box": {
                  "id": "obj-394",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    1012,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5246"
                }
              },
              {
                "box": {
                  "id": "obj-395",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    604,
                    376,
                    38,
                    14
                  ],
                  "text": "REVG",
                  "presentation": 1,
                  "presentation_rect": [
                    604,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-396",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    643,
                    357,
                    18,
                    18
                  ],
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    357,
                    18,
                    18
                  ],
                  "bgcolor": [
                    0.043,
                    0.137,
                    0.102,
                    1.0
                  ],
                  "color": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "varname": "p5247"
                }
              },
              {
                "box": {
                  "id": "obj-397",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    1040,
                    180,
                    22
                  ],
                  "text": "prepend setparam 5247"
                }
              },
              {
                "box": {
                  "id": "obj-398",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    643,
                    376,
                    38,
                    14
                  ],
                  "text": "SIL",
                  "presentation": 1,
                  "presentation_rect": [
                    643,
                    376,
                    38,
                    14
                  ],
                  "textcolor": [
                    1.0,
                    0.925,
                    0.42,
                    1.0
                  ],
                  "fontsize": 8.0,
                  "fontface": 0
                }
              },
              {
                "box": {
                  "id": "obj-399",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    1068,
                    180,
                    22
                  ],
                  "text": "receive pmor_rx"
                }
              },
              {
                "box": {
                  "id": "obj-400",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    970,
                    1096,
                    180,
                    22
                  ],
                  "text": "js pmor_ui.js"
                }
              },
              {
                "box": {
                  "id": "obj-401",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    1124,
                    180,
                    22
                  ],
                  "text": "receive pmor_ui_algo"
                }
              },
              {
                "box": {
                  "id": "obj-402",
                  "maxclass": "newobj",
                  "numinlets": 2,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    970,
                    1152,
                    180,
                    22
                  ],
                  "text": "prepend ralgo"
                }
              },
              {
                "box": {
                  "id": "obj-403",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    970,
                    1180,
                    180,
                    22
                  ],
                  "text": "send pmor_ui_algo"
                }
              }
            ],
            "lines": [
              {
                "patchline": {
                  "source": [
                    "obj-3",
                    0
                  ],
                  "destination": [
                    "obj-4",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-4",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-8",
                    0
                  ],
                  "destination": [
                    "obj-9",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-9",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-11",
                    0
                  ],
                  "destination": [
                    "obj-12",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-12",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-14",
                    0
                  ],
                  "destination": [
                    "obj-15",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-15",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-17",
                    0
                  ],
                  "destination": [
                    "obj-18",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-18",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-20",
                    0
                  ],
                  "destination": [
                    "obj-21",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-21",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-23",
                    0
                  ],
                  "destination": [
                    "obj-24",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-24",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-26",
                    0
                  ],
                  "destination": [
                    "obj-27",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-27",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-29",
                    0
                  ],
                  "destination": [
                    "obj-30",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-30",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-32",
                    0
                  ],
                  "destination": [
                    "obj-33",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-33",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-35",
                    0
                  ],
                  "destination": [
                    "obj-36",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-36",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-38",
                    0
                  ],
                  "destination": [
                    "obj-39",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-39",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-41",
                    0
                  ],
                  "destination": [
                    "obj-42",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-42",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-44",
                    0
                  ],
                  "destination": [
                    "obj-45",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-45",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-47",
                    0
                  ],
                  "destination": [
                    "obj-48",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-48",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-50",
                    0
                  ],
                  "destination": [
                    "obj-51",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-51",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-53",
                    0
                  ],
                  "destination": [
                    "obj-54",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-54",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-57",
                    0
                  ],
                  "destination": [
                    "obj-58",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-58",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-60",
                    0
                  ],
                  "destination": [
                    "obj-61",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-61",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-63",
                    0
                  ],
                  "destination": [
                    "obj-64",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-64",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-66",
                    0
                  ],
                  "destination": [
                    "obj-67",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-67",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-69",
                    0
                  ],
                  "destination": [
                    "obj-70",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-70",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-72",
                    0
                  ],
                  "destination": [
                    "obj-73",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-73",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-75",
                    0
                  ],
                  "destination": [
                    "obj-76",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-76",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-78",
                    0
                  ],
                  "destination": [
                    "obj-79",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-79",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-81",
                    0
                  ],
                  "destination": [
                    "obj-82",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-82",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-84",
                    0
                  ],
                  "destination": [
                    "obj-85",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-85",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-87",
                    0
                  ],
                  "destination": [
                    "obj-88",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-88",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-90",
                    0
                  ],
                  "destination": [
                    "obj-91",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-91",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-93",
                    0
                  ],
                  "destination": [
                    "obj-94",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-94",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-96",
                    0
                  ],
                  "destination": [
                    "obj-97",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-97",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-99",
                    0
                  ],
                  "destination": [
                    "obj-100",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-100",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-102",
                    0
                  ],
                  "destination": [
                    "obj-103",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-103",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-106",
                    0
                  ],
                  "destination": [
                    "obj-107",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-107",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-109",
                    0
                  ],
                  "destination": [
                    "obj-110",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-110",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-112",
                    0
                  ],
                  "destination": [
                    "obj-113",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-113",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-115",
                    0
                  ],
                  "destination": [
                    "obj-116",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-116",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-118",
                    0
                  ],
                  "destination": [
                    "obj-119",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-119",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-121",
                    0
                  ],
                  "destination": [
                    "obj-122",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-122",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-124",
                    0
                  ],
                  "destination": [
                    "obj-125",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-125",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-127",
                    0
                  ],
                  "destination": [
                    "obj-128",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-128",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-130",
                    0
                  ],
                  "destination": [
                    "obj-131",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-131",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-133",
                    0
                  ],
                  "destination": [
                    "obj-134",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-134",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-136",
                    0
                  ],
                  "destination": [
                    "obj-137",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-137",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-139",
                    0
                  ],
                  "destination": [
                    "obj-140",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-140",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-142",
                    0
                  ],
                  "destination": [
                    "obj-143",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-143",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-145",
                    0
                  ],
                  "destination": [
                    "obj-146",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-146",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-148",
                    0
                  ],
                  "destination": [
                    "obj-149",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-149",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-151",
                    0
                  ],
                  "destination": [
                    "obj-152",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-152",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-155",
                    0
                  ],
                  "destination": [
                    "obj-156",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-156",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-158",
                    0
                  ],
                  "destination": [
                    "obj-159",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-159",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-161",
                    0
                  ],
                  "destination": [
                    "obj-162",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-162",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-164",
                    0
                  ],
                  "destination": [
                    "obj-165",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-165",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-167",
                    0
                  ],
                  "destination": [
                    "obj-168",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-168",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-170",
                    0
                  ],
                  "destination": [
                    "obj-171",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-171",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-173",
                    0
                  ],
                  "destination": [
                    "obj-174",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-174",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-176",
                    0
                  ],
                  "destination": [
                    "obj-177",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-177",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-179",
                    0
                  ],
                  "destination": [
                    "obj-180",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-180",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-182",
                    0
                  ],
                  "destination": [
                    "obj-183",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-183",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-185",
                    0
                  ],
                  "destination": [
                    "obj-186",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-186",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-188",
                    0
                  ],
                  "destination": [
                    "obj-189",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-189",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-191",
                    0
                  ],
                  "destination": [
                    "obj-192",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-192",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-194",
                    0
                  ],
                  "destination": [
                    "obj-195",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-195",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-197",
                    0
                  ],
                  "destination": [
                    "obj-198",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-198",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-200",
                    0
                  ],
                  "destination": [
                    "obj-201",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-201",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-204",
                    0
                  ],
                  "destination": [
                    "obj-205",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-205",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-207",
                    0
                  ],
                  "destination": [
                    "obj-208",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-208",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-210",
                    0
                  ],
                  "destination": [
                    "obj-211",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-211",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-213",
                    0
                  ],
                  "destination": [
                    "obj-214",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-214",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-216",
                    0
                  ],
                  "destination": [
                    "obj-217",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-217",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-219",
                    0
                  ],
                  "destination": [
                    "obj-220",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-220",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-222",
                    0
                  ],
                  "destination": [
                    "obj-223",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-223",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-225",
                    0
                  ],
                  "destination": [
                    "obj-226",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-226",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-228",
                    0
                  ],
                  "destination": [
                    "obj-229",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-229",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-231",
                    0
                  ],
                  "destination": [
                    "obj-232",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-232",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-234",
                    0
                  ],
                  "destination": [
                    "obj-235",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-235",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-237",
                    0
                  ],
                  "destination": [
                    "obj-238",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-238",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-240",
                    0
                  ],
                  "destination": [
                    "obj-241",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-241",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-243",
                    0
                  ],
                  "destination": [
                    "obj-244",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-244",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-246",
                    0
                  ],
                  "destination": [
                    "obj-247",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-247",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-249",
                    0
                  ],
                  "destination": [
                    "obj-250",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-250",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-253",
                    0
                  ],
                  "destination": [
                    "obj-254",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-254",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-256",
                    0
                  ],
                  "destination": [
                    "obj-257",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-257",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-259",
                    0
                  ],
                  "destination": [
                    "obj-260",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-260",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-262",
                    0
                  ],
                  "destination": [
                    "obj-263",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-263",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-265",
                    0
                  ],
                  "destination": [
                    "obj-266",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-266",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-268",
                    0
                  ],
                  "destination": [
                    "obj-269",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-269",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-271",
                    0
                  ],
                  "destination": [
                    "obj-272",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-272",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-274",
                    0
                  ],
                  "destination": [
                    "obj-275",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-275",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-277",
                    0
                  ],
                  "destination": [
                    "obj-278",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-278",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-280",
                    0
                  ],
                  "destination": [
                    "obj-281",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-281",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-283",
                    0
                  ],
                  "destination": [
                    "obj-284",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-284",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-286",
                    0
                  ],
                  "destination": [
                    "obj-287",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-287",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-289",
                    0
                  ],
                  "destination": [
                    "obj-290",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-290",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-292",
                    0
                  ],
                  "destination": [
                    "obj-293",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-293",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-295",
                    0
                  ],
                  "destination": [
                    "obj-296",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-296",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-298",
                    0
                  ],
                  "destination": [
                    "obj-299",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-299",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-302",
                    0
                  ],
                  "destination": [
                    "obj-303",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-303",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-305",
                    0
                  ],
                  "destination": [
                    "obj-306",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-306",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-308",
                    0
                  ],
                  "destination": [
                    "obj-309",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-309",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-311",
                    0
                  ],
                  "destination": [
                    "obj-312",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-312",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-314",
                    0
                  ],
                  "destination": [
                    "obj-315",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-315",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-317",
                    0
                  ],
                  "destination": [
                    "obj-318",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-318",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-320",
                    0
                  ],
                  "destination": [
                    "obj-321",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-321",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-323",
                    0
                  ],
                  "destination": [
                    "obj-324",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-324",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-326",
                    0
                  ],
                  "destination": [
                    "obj-327",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-327",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-329",
                    0
                  ],
                  "destination": [
                    "obj-330",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-330",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-332",
                    0
                  ],
                  "destination": [
                    "obj-333",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-333",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-335",
                    0
                  ],
                  "destination": [
                    "obj-336",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-336",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-338",
                    0
                  ],
                  "destination": [
                    "obj-339",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-339",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-341",
                    0
                  ],
                  "destination": [
                    "obj-342",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-342",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-344",
                    0
                  ],
                  "destination": [
                    "obj-345",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-345",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-347",
                    0
                  ],
                  "destination": [
                    "obj-348",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-348",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-351",
                    0
                  ],
                  "destination": [
                    "obj-352",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-352",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-354",
                    0
                  ],
                  "destination": [
                    "obj-355",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-355",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-357",
                    0
                  ],
                  "destination": [
                    "obj-358",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-358",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-360",
                    0
                  ],
                  "destination": [
                    "obj-361",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-361",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-363",
                    0
                  ],
                  "destination": [
                    "obj-364",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-364",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-366",
                    0
                  ],
                  "destination": [
                    "obj-367",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-367",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-369",
                    0
                  ],
                  "destination": [
                    "obj-370",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-370",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-372",
                    0
                  ],
                  "destination": [
                    "obj-373",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-373",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-375",
                    0
                  ],
                  "destination": [
                    "obj-376",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-376",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-378",
                    0
                  ],
                  "destination": [
                    "obj-379",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-379",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-381",
                    0
                  ],
                  "destination": [
                    "obj-382",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-382",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-384",
                    0
                  ],
                  "destination": [
                    "obj-385",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-385",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-387",
                    0
                  ],
                  "destination": [
                    "obj-388",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-388",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-390",
                    0
                  ],
                  "destination": [
                    "obj-391",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-391",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-393",
                    0
                  ],
                  "destination": [
                    "obj-394",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-394",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-396",
                    0
                  ],
                  "destination": [
                    "obj-397",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-397",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-399",
                    0
                  ],
                  "destination": [
                    "obj-400",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-400",
                    0
                  ],
                  "destination": [
                    "obj-5",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-401",
                    0
                  ],
                  "destination": [
                    "obj-402",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-402",
                    0
                  ],
                  "destination": [
                    "obj-400",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-400",
                    2
                  ],
                  "destination": [
                    "obj-403",
                    0
                  ]
                }
              }
            ]
          }
        }
      },
      {
        "box": {
          "id": "obj-8",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            "int"
          ],
          "patching_rect": [
            20,
            844,
            180,
            22
          ],
          "text": "midiin"
        }
      },
      {
        "box": {
          "id": "obj-9",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            20,
            872,
            180,
            22
          ],
          "text": "js pmor_lane.js 4 5"
        }
      },
      {
        "box": {
          "id": "obj-10",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            20,
            900,
            180,
            22
          ],
          "text": "send pmor_midi"
        }
      },
      {
        "box": {
          "id": "obj-11",
          "maxclass": "umenu",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "int",
            "",
            ""
          ],
          "patching_rect": [
            10,
            34,
            80,
            22
          ],
          "presentation": 1,
          "presentation_rect": [
            10,
            34,
            80,
            22
          ],
          "items": [
            "alg 0",
            ",",
            "alg 1",
            ",",
            "alg 2",
            ",",
            "alg 3",
            ",",
            "alg 4",
            ",",
            "alg 5"
          ],
          "textcolor": [
            0.45,
            0.78,
            1.0,
            1.0
          ],
          "bgcolor": [
            0.043,
            0.137,
            0.102,
            1.0
          ],
          "fontsize": 11.0,
          "varname": "p5248"
        }
      },
      {
        "box": {
          "id": "obj-12",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            20,
            928,
            180,
            22
          ],
          "text": "prepend setparam 5248"
        }
      },
      {
        "box": {
          "id": "obj-13",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            20,
            956,
            180,
            22
          ],
          "text": "send pmor_tx"
        }
      },
      {
        "box": {
          "id": "obj-14",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            96,
            36,
            110,
            14
          ],
          "text": "distortion algo",
          "presentation": 1,
          "presentation_rect": [
            96,
            36,
            110,
            14
          ],
          "textcolor": [
            0.45,
            0.78,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontface": 0
        }
      },
      {
        "box": {
          "id": "obj-15",
          "maxclass": "toggle",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            "int"
          ],
          "patching_rect": [
            16,
            74,
            22,
            22
          ],
          "presentation": 1,
          "presentation_rect": [
            16,
            74,
            22,
            22
          ],
          "bgcolor": [
            0.043,
            0.137,
            0.102,
            1.0
          ],
          "color": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "varname": "p5120"
        }
      },
      {
        "box": {
          "id": "obj-16",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            20,
            984,
            180,
            22
          ],
          "text": "prepend setparam 5120"
        }
      },
      {
        "box": {
          "id": "obj-17",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            16,
            97,
            44,
            14
          ],
          "text": "ST4",
          "presentation": 1,
          "presentation_rect": [
            16,
            97,
            44,
            14
          ],
          "textcolor": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "fontsize": 9.0,
          "fontface": 0
        }
      },
      {
        "box": {
          "id": "obj-18",
          "maxclass": "toggle",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            "int"
          ],
          "patching_rect": [
            76,
            74,
            22,
            22
          ],
          "presentation": 1,
          "presentation_rect": [
            76,
            74,
            22,
            22
          ],
          "bgcolor": [
            0.043,
            0.137,
            0.102,
            1.0
          ],
          "color": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "varname": "p5121"
        }
      },
      {
        "box": {
          "id": "obj-19",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            20,
            1012,
            180,
            22
          ],
          "text": "prepend setparam 5121"
        }
      },
      {
        "box": {
          "id": "obj-20",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            76,
            97,
            44,
            14
          ],
          "text": "ST8",
          "presentation": 1,
          "presentation_rect": [
            76,
            97,
            44,
            14
          ],
          "textcolor": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "fontsize": 9.0,
          "fontface": 0
        }
      },
      {
        "box": {
          "id": "obj-21",
          "maxclass": "toggle",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            "int"
          ],
          "patching_rect": [
            136,
            74,
            22,
            22
          ],
          "presentation": 1,
          "presentation_rect": [
            136,
            74,
            22,
            22
          ],
          "bgcolor": [
            0.043,
            0.137,
            0.102,
            1.0
          ],
          "color": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "varname": "p5122"
        }
      },
      {
        "box": {
          "id": "obj-22",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            20,
            1040,
            180,
            22
          ],
          "text": "prepend setparam 5122"
        }
      },
      {
        "box": {
          "id": "obj-23",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            136,
            97,
            44,
            14
          ],
          "text": "ST16",
          "presentation": 1,
          "presentation_rect": [
            136,
            97,
            44,
            14
          ],
          "textcolor": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "fontsize": 9.0,
          "fontface": 0
        }
      },
      {
        "box": {
          "id": "obj-24",
          "maxclass": "toggle",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            "int"
          ],
          "patching_rect": [
            196,
            74,
            22,
            22
          ],
          "presentation": 1,
          "presentation_rect": [
            196,
            74,
            22,
            22
          ],
          "bgcolor": [
            0.043,
            0.137,
            0.102,
            1.0
          ],
          "color": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "varname": "p5123"
        }
      },
      {
        "box": {
          "id": "obj-25",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            20,
            1068,
            180,
            22
          ],
          "text": "prepend setparam 5123"
        }
      },
      {
        "box": {
          "id": "obj-26",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            196,
            97,
            44,
            14
          ],
          "text": "ST32",
          "presentation": 1,
          "presentation_rect": [
            196,
            97,
            44,
            14
          ],
          "textcolor": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "fontsize": 9.0,
          "fontface": 0
        }
      },
      {
        "box": {
          "id": "obj-27",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            10,
            130,
            360,
            14
          ],
          "text": "bank 1 stutters; all 128 engage toggles in the editor",
          "presentation": 1,
          "presentation_rect": [
            10,
            130,
            360,
            14
          ],
          "textcolor": [
            0.6,
            0.965,
            0.62,
            1.0
          ],
          "fontsize": 10.0,
          "fontface": 0
        }
      },
      {
        "box": {
          "id": "obj-28",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            20,
            1096,
            180,
            22
          ],
          "text": "receive pmor_rx"
        }
      },
      {
        "box": {
          "id": "obj-29",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "",
            "",
            ""
          ],
          "patching_rect": [
            20,
            1124,
            180,
            22
          ],
          "text": "js pmor_ui.js"
        }
      },
      {
        "box": {
          "id": "obj-30",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            20,
            1152,
            180,
            22
          ],
          "text": "receive pmor_ui_algo"
        }
      },
      {
        "box": {
          "id": "obj-31",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            20,
            1180,
            180,
            22
          ],
          "text": "prepend ralgo"
        }
      },
      {
        "box": {
          "id": "obj-32",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            20,
            1208,
            180,
            22
          ],
          "text": "send pmor_ui_algo"
        }
      }
    ],
    "lines": [
      {
        "patchline": {
          "source": [
            "obj-5",
            0
          ],
          "destination": [
            "obj-6",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-6",
            0
          ],
          "destination": [
            "obj-7",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-8",
            0
          ],
          "destination": [
            "obj-9",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-9",
            0
          ],
          "destination": [
            "obj-10",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-11",
            0
          ],
          "destination": [
            "obj-12",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-12",
            0
          ],
          "destination": [
            "obj-13",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-15",
            0
          ],
          "destination": [
            "obj-16",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-16",
            0
          ],
          "destination": [
            "obj-13",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-18",
            0
          ],
          "destination": [
            "obj-19",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-19",
            0
          ],
          "destination": [
            "obj-13",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-21",
            0
          ],
          "destination": [
            "obj-22",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-22",
            0
          ],
          "destination": [
            "obj-13",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-24",
            0
          ],
          "destination": [
            "obj-25",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-25",
            0
          ],
          "destination": [
            "obj-13",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-28",
            0
          ],
          "destination": [
            "obj-29",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-29",
            0
          ],
          "destination": [
            "obj-13",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-30",
            0
          ],
          "destination": [
            "obj-31",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-31",
            0
          ],
          "destination": [
            "obj-29",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-29",
            2
          ],
          "destination": [
            "obj-32",
            0
          ]
        }
      }
    ],
    "devicewidth": 380.0,
    "openrect": [
      0.0,
      0.0,
      380.0,
      169.0
    ],
    "project": {
      "version": 1,
      "creationdate": 0,
      "modificationdate": 0,
      "viewrect": [
        0.0,
        0.0,
        300.0,
        500.0
      ],
      "autoorganize": 1,
      "hideprojectwindow": 1,
      "showdependencies": 1,
      "autolocalize": 0,
      "contents": {
        "patchers": {}
      },
      "layout": {},
      "searchpath": {},
      "detailsvisible": 0,
      "amxdtype": 1835887981,
      "readonly": 0,
      "devpathtype": 0,
      "devpath": ".",
      "sortmode": 0,
      "viewmode": 0,
      "includepackages": 0
    },
    "dependency_cache": [],
    "parameters": {},
    "is_mpe": 0,
    "latency": 0,
    "minimum_live_version": "",
    "minimum_max_version": "",
    "platform_compatibility": 0,
    "autosave": 0
  }
}
