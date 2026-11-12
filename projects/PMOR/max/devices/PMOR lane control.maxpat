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
      0.051,
      0.0353,
      0.098,
      1.0
    ],
    "editing_bgcolor": [
      0.051,
      0.0353,
      0.098,
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
            748,
            169
          ],
          "bgfillcolor": {
            "angle": 270.0,
            "autogradient": 0,
            "color": [
              0.051,
              0.0353,
              0.098,
              1.0
            ],
            "color1": [
              0.051,
              0.0353,
              0.098,
              1.0
            ],
            "color2": [
              0.051,
              0.0353,
              0.098,
              1.0
            ],
            "proportion": 0.39,
            "type": "color"
          },
          "border": 0,
          "rounded": 0,
          "background": 1,
          "presentation": 1,
          "presentation_rect": [
            0,
            0,
            748,
            169
          ]
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
            8,
            32,
            732,
            128
          ],
          "bgfillcolor": {
            "angle": 270.0,
            "autogradient": 0,
            "color": [
              0.1333,
              0.102,
              0.2471,
              1.0
            ],
            "color1": [
              0.1333,
              0.102,
              0.2471,
              1.0
            ],
            "color2": [
              0.1333,
              0.102,
              0.2471,
              1.0
            ],
            "proportion": 0.39,
            "type": "color"
          },
          "border": 0,
          "rounded": 8,
          "background": 1,
          "presentation": 1,
          "presentation_rect": [
            8,
            32,
            732,
            128
          ]
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
            272,
            4,
            204,
            24
          ],
          "text": "PMOR control",
          "textcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fontsize": 14.0,
          "fontname": "Arial",
          "fontface": 1,
          "textjustification": 1,
          "varname": "title",
          "presentation": 1,
          "presentation_rect": [
            272,
            4,
            204,
            24
          ]
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
            16,
            8,
            200,
            16
          ],
          "text": "ch 4 5 6 -> PMOR core",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 0,
          "presentation": 1,
          "presentation_rect": [
            16,
            8,
            200,
            16
          ]
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
            636,
            40,
            96,
            112
          ],
          "text": "EXPAND",
          "texton": "MINIMIZE",
          "textcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "textoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "bgoncolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "usebgoncolor": 1,
          "fontsize": 14.0,
          "fontname": "Arial",
          "fontface": 1,
          "rounded": 4,
          "mode": 1,
          "varname": "expand",
          "presentation": 1,
          "presentation_rect": [
            636,
            40,
            96,
            112
          ]
        }
      },
      {
        "box": {
          "id": "obj-6",
          "maxclass": "newobj",
          "numinlets": 0,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            20,
            788,
            180,
            22
          ],
          "text": "p editor",
          "varname": "editor",
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
              720.0,
              316.0
            ],
            "openinpresentation": 1,
            "bglocked": 0,
            "bgcolor": [
              0.051,
              0.0353,
              0.098,
              1.0
            ],
            "editing_bgcolor": [
              0.051,
              0.0353,
              0.098,
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
                    720,
                    316
                  ],
                  "bgfillcolor": {
                    "angle": 270.0,
                    "autogradient": 0,
                    "color": [
                      0.051,
                      0.0353,
                      0.098,
                      1.0
                    ],
                    "color1": [
                      0.051,
                      0.0353,
                      0.098,
                      1.0
                    ],
                    "color2": [
                      0.051,
                      0.0353,
                      0.098,
                      1.0
                    ],
                    "proportion": 0.39,
                    "type": "color"
                  },
                  "border": 0,
                  "rounded": 0,
                  "background": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    0,
                    0,
                    720,
                    316
                  ]
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
                    8,
                    72,
                    704,
                    236
                  ],
                  "bgfillcolor": {
                    "angle": 270.0,
                    "autogradient": 0,
                    "color": [
                      0.1333,
                      0.102,
                      0.2471,
                      1.0
                    ],
                    "color1": [
                      0.1333,
                      0.102,
                      0.2471,
                      1.0
                    ],
                    "color2": [
                      0.1333,
                      0.102,
                      0.2471,
                      1.0
                    ],
                    "proportion": 0.39,
                    "type": "color"
                  },
                  "border": 0,
                  "rounded": 8,
                  "background": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    8,
                    72,
                    704,
                    236
                  ]
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
                    260,
                    4,
                    200,
                    24
                  ],
                  "text": "PMOR control",
                  "textcolor": [
                    0.3098,
                    0.851,
                    0.7686,
                    1.0
                  ],
                  "fontsize": 14.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 1,
                  "varname": "title",
                  "presentation": 1,
                  "presentation_rect": [
                    260,
                    4,
                    200,
                    24
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-4",
                  "maxclass": "tab",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    16,
                    36,
                    688,
                    28
                  ],
                  "tabs": [
                    "METERS",
                    "FX"
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "tabcolor": [
                    0.1333,
                    0.102,
                    0.2471,
                    1.0
                  ],
                  "htabcolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "textcolor": [
                    0.7882,
                    0.7373,
                    0.9412,
                    1.0
                  ],
                  "htextcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "rounded": 4,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "varname": "tabs",
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    36,
                    688,
                    28
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-5",
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
                  "text": "prepend page"
                }
              },
              {
                "box": {
                  "id": "obj-6",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    ""
                  ],
                  "patching_rect": [
                    20,
                    816,
                    180,
                    22
                  ],
                  "text": "receive pmor_rx"
                }
              },
              {
                "box": {
                  "id": "obj-7",
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
                    844,
                    180,
                    22
                  ],
                  "text": "js pmor_bank.js gated control",
                  "varname": "bank"
                }
              },
              {
                "box": {
                  "id": "obj-8",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    872,
                    180,
                    22
                  ],
                  "text": "js pmor_guard.js",
                  "varname": "guard"
                }
              },
              {
                "box": {
                  "id": "obj-9",
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
                  "text": "send pmor_tx"
                }
              },
              {
                "box": {
                  "id": "obj-10",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_rx"
                }
              },
              {
                "box": {
                  "id": "obj-11",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    956,
                    180,
                    22
                  ],
                  "text": "route param"
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
                    984,
                    180,
                    22
                  ],
                  "text": "prepend param"
                }
              },
              {
                "box": {
                  "id": "obj-13",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_ui_status"
                }
              },
              {
                "box": {
                  "id": "obj-14",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1040,
                    180,
                    22
                  ],
                  "text": "route st"
                }
              },
              {
                "box": {
                  "id": "obj-15",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_ui_sel"
                }
              },
              {
                "box": {
                  "id": "obj-16",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    20,
                    1096,
                    180,
                    22
                  ],
                  "text": "send pmor_ui_sel"
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
                    40,
                    400,
                    16
                  ],
                  "text": "control CCs (ch4)",
                  "textcolor": [
                    0.3098,
                    0.851,
                    0.7686,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh0_19",
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    80,
                    400,
                    16
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-18",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    68,
                    80,
                    16
                  ],
                  "text": "FSR",
                  "textcolor": [
                    0.7882,
                    0.7373,
                    0.9412,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    108,
                    80,
                    16
                  ],
                  "varname": "bh0_x1"
                }
              },
              {
                "box": {
                  "id": "obj-19",
                  "maxclass": "slider",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    104,
                    72,
                    320,
                    8
                  ],
                  "size": 128.0,
                  "min": 0.0,
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "knobcolor": [
                    0.3098,
                    0.851,
                    0.7686,
                    1.0
                  ],
                  "elementcolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "bordercolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "m1",
                  "presentation": 1,
                  "presentation_rect": [
                    104,
                    112,
                    320,
                    8
                  ]
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
                    16,
                    96,
                    80,
                    16
                  ],
                  "text": "LIDAR",
                  "textcolor": [
                    0.7882,
                    0.7373,
                    0.9412,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    136,
                    80,
                    16
                  ],
                  "varname": "bh0_x2"
                }
              },
              {
                "box": {
                  "id": "obj-21",
                  "maxclass": "slider",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    104,
                    100,
                    320,
                    8
                  ],
                  "size": 128.0,
                  "min": 0.0,
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "knobcolor": [
                    0.3098,
                    0.851,
                    0.7686,
                    1.0
                  ],
                  "elementcolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "bordercolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "m2",
                  "presentation": 1,
                  "presentation_rect": [
                    104,
                    140,
                    320,
                    8
                  ]
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
                    16,
                    124,
                    80,
                    16
                  ],
                  "text": "ACCEL X",
                  "textcolor": [
                    0.7882,
                    0.7373,
                    0.9412,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    164,
                    80,
                    16
                  ],
                  "varname": "bh0_x3"
                }
              },
              {
                "box": {
                  "id": "obj-23",
                  "maxclass": "slider",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    104,
                    128,
                    320,
                    8
                  ],
                  "size": 128.0,
                  "min": 0.0,
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "knobcolor": [
                    0.3098,
                    0.851,
                    0.7686,
                    1.0
                  ],
                  "elementcolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "bordercolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "m3",
                  "presentation": 1,
                  "presentation_rect": [
                    104,
                    168,
                    320,
                    8
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-24",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    152,
                    80,
                    16
                  ],
                  "text": "ACCEL Y",
                  "textcolor": [
                    0.7882,
                    0.7373,
                    0.9412,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    192,
                    80,
                    16
                  ],
                  "varname": "bh0_x4"
                }
              },
              {
                "box": {
                  "id": "obj-25",
                  "maxclass": "slider",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    104,
                    156,
                    320,
                    8
                  ],
                  "size": 128.0,
                  "min": 0.0,
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "knobcolor": [
                    0.3098,
                    0.851,
                    0.7686,
                    1.0
                  ],
                  "elementcolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "bordercolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "m4",
                  "presentation": 1,
                  "presentation_rect": [
                    104,
                    196,
                    320,
                    8
                  ]
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
                    16,
                    180,
                    80,
                    16
                  ],
                  "text": "JOY X",
                  "textcolor": [
                    0.7882,
                    0.7373,
                    0.9412,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    220,
                    80,
                    16
                  ],
                  "varname": "bh0_x5"
                }
              },
              {
                "box": {
                  "id": "obj-27",
                  "maxclass": "slider",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    104,
                    184,
                    320,
                    8
                  ],
                  "size": 128.0,
                  "min": 0.0,
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "knobcolor": [
                    0.3098,
                    0.851,
                    0.7686,
                    1.0
                  ],
                  "elementcolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "bordercolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "m5",
                  "presentation": 1,
                  "presentation_rect": [
                    104,
                    224,
                    320,
                    8
                  ]
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
                    16,
                    208,
                    80,
                    16
                  ],
                  "text": "JOY Y",
                  "textcolor": [
                    0.7882,
                    0.7373,
                    0.9412,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    248,
                    80,
                    16
                  ],
                  "varname": "bh0_x6"
                }
              },
              {
                "box": {
                  "id": "obj-29",
                  "maxclass": "slider",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    104,
                    212,
                    320,
                    8
                  ],
                  "size": 128.0,
                  "min": 0.0,
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "knobcolor": [
                    0.3098,
                    0.851,
                    0.7686,
                    1.0
                  ],
                  "elementcolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "bordercolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "m6",
                  "presentation": 1,
                  "presentation_rect": [
                    104,
                    252,
                    320,
                    8
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-30",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    236,
                    80,
                    16
                  ],
                  "text": "FADER",
                  "textcolor": [
                    0.7882,
                    0.7373,
                    0.9412,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    276,
                    80,
                    16
                  ],
                  "varname": "bh0_x7"
                }
              },
              {
                "box": {
                  "id": "obj-31",
                  "maxclass": "slider",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    104,
                    240,
                    320,
                    8
                  ],
                  "size": 128.0,
                  "min": 0.0,
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "knobcolor": [
                    0.3098,
                    0.851,
                    0.7686,
                    1.0
                  ],
                  "elementcolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "bordercolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "m7",
                  "presentation": 1,
                  "presentation_rect": [
                    104,
                    280,
                    320,
                    8
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-32",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    464,
                    40,
                    240,
                    16
                  ],
                  "text": "random-FX seed",
                  "textcolor": [
                    0.3098,
                    0.851,
                    0.7686,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh0_20",
                  "presentation": 1,
                  "presentation_rect": [
                    464,
                    80,
                    240,
                    16
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-33",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    464,
                    68,
                    240,
                    16
                  ],
                  "text": "seed -",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "textjustification": 0,
                  "varname": "seed",
                  "presentation": 1,
                  "presentation_rect": [
                    464,
                    108,
                    240,
                    16
                  ]
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
                    464,
                    92,
                    240,
                    16
                  ],
                  "text": "live CC stream (ch4 / ch5 recorded as MIDI)",
                  "textcolor": [
                    0.7882,
                    0.7373,
                    0.9412,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    464,
                    132,
                    240,
                    16
                  ],
                  "varname": "bh0_x8"
                }
              },
              {
                "box": {
                  "id": "obj-35",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    40,
                    688,
                    16
                  ],
                  "text": "FX controls",
                  "textcolor": [
                    0.3098,
                    0.851,
                    0.7686,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh1_21",
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    80,
                    688,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-36",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    72,
                    688,
                    16
                  ],
                  "text": "FX controls are in the lane: bank / effect browser, Live-automatable.",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    112,
                    688,
                    16
                  ],
                  "varname": "bh1_x9",
                  "hidden": 1
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
                    16,
                    96,
                    688,
                    16
                  ],
                  "text": "Record them in a clip (automation lane) and edit there; PMOR stays the source of truth.",
                  "textcolor": [
                    0.7882,
                    0.7373,
                    0.9412,
                    1.0
                  ],
                  "fontsize": 10.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    136,
                    688,
                    16
                  ],
                  "varname": "bh1_x10",
                  "hidden": 1
                }
              }
            ],
            "lines": [
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
                    "obj-10",
                    0
                  ],
                  "destination": [
                    "obj-11",
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
                    "obj-8",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-7",
                    0
                  ],
                  "destination": [
                    "obj-8",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-13",
                    0
                  ],
                  "destination": [
                    "obj-14",
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
                    "obj-7",
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
                    "obj-7",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-7",
                    2
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
                    "obj-5",
                    0
                  ],
                  "destination": [
                    "obj-7",
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
          "id": "obj-7",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            "int"
          ],
          "patching_rect": [
            20,
            816,
            180,
            22
          ],
          "text": "midiin"
        }
      },
      {
        "box": {
          "id": "obj-8",
          "maxclass": "newobj",
          "numinlets": 1,
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
          "text": "js pmor_lane.js 4 5 6 meters",
          "varname": "lanejs"
        }
      },
      {
        "box": {
          "id": "obj-9",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            20,
            872,
            180,
            22
          ],
          "text": "send pmor_midi"
        }
      },
      {
        "box": {
          "id": "obj-10",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "bang",
            "bang",
            ""
          ],
          "patching_rect": [
            20,
            900,
            180,
            22
          ],
          "text": "sel 1 0"
        }
      },
      {
        "box": {
          "id": "obj-11",
          "maxclass": "message",
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
          "text": "show"
        }
      },
      {
        "box": {
          "id": "obj-12",
          "maxclass": "message",
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
          "text": "hide"
        }
      },
      {
        "box": {
          "id": "obj-13",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 4,
          "outlettype": [
            "",
            "",
            "",
            ""
          ],
          "patching_rect": [
            20,
            984,
            180,
            22
          ],
          "text": "js pmor_edit.js",
          "varname": "edit"
        }
      },
      {
        "box": {
          "id": "obj-14",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            20,
            1012,
            180,
            22
          ],
          "text": "js pmor_guard.js",
          "varname": "guard"
        }
      },
      {
        "box": {
          "id": "obj-15",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            20,
            1040,
            180,
            22
          ],
          "text": "send pmor_tx"
        }
      },
      {
        "box": {
          "id": "obj-16",
          "maxclass": "newobj",
          "numinlets": 1,
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
          "text": "receive pmor_rx"
        }
      },
      {
        "box": {
          "id": "obj-17",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            20,
            1096,
            180,
            22
          ],
          "text": "route param"
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
            1124,
            180,
            22
          ],
          "text": "prepend param"
        }
      },
      {
        "box": {
          "id": "obj-19",
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
            1152,
            180,
            22
          ],
          "text": "pattr pmor_winrect @parameter_enable 1",
          "varname": "winrect",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "PMOR editor window",
              "parameter_shortname": "PMOR win",
              "parameter_type": 3,
              "parameter_invisible": 1
            }
          }
        }
      },
      {
        "box": {
          "id": "obj-20",
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
          "text": "prepend rectload"
        }
      },
      {
        "box": {
          "id": "obj-21",
          "maxclass": "panel",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            404,
            40,
            1,
            112
          ],
          "bgfillcolor": {
            "angle": 270.0,
            "autogradient": 0,
            "color": [
              0.3569,
              0.2941,
              0.6196,
              1.0
            ],
            "color1": [
              0.3569,
              0.2941,
              0.6196,
              1.0
            ],
            "color2": [
              0.3569,
              0.2941,
              0.6196,
              1.0
            ],
            "proportion": 0.39,
            "type": "color"
          },
          "border": 0,
          "rounded": 0,
          "background": 1,
          "presentation": 1,
          "presentation_rect": [
            404,
            40,
            1,
            112
          ]
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
            16,
            44,
            28,
            16
          ],
          "text": "FX",
          "textcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fontsize": 12.0,
          "fontname": "Arial",
          "fontface": 1,
          "textjustification": 0,
          "presentation": 1,
          "presentation_rect": [
            16,
            44,
            28,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-23",
          "maxclass": "umenu",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "int",
            "",
            ""
          ],
          "patching_rect": [
            48,
            40,
            72,
            24
          ],
          "items": [
            "bank 1",
            ",",
            "bank 2",
            ",",
            "bank 3",
            ",",
            "bank 4",
            ",",
            "bank 5",
            ",",
            "bank 6",
            ",",
            "bank 7",
            ",",
            "bank 8"
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bgfillcolor": {
            "angle": 270.0,
            "autogradient": 0,
            "color": [
              0.051,
              0.0353,
              0.098,
              1.0
            ],
            "color1": [
              0.051,
              0.0353,
              0.098,
              1.0
            ],
            "color2": [
              0.051,
              0.0353,
              0.098,
              1.0
            ],
            "proportion": 0.39,
            "type": "color"
          },
          "fontsize": 11.0,
          "fontname": "Arial",
          "rounded": 4,
          "varname": "fxbank",
          "presentation": 1,
          "presentation_rect": [
            48,
            40,
            72,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-24",
          "maxclass": "umenu",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "int",
            "",
            ""
          ],
          "patching_rect": [
            128,
            40,
            80,
            24
          ],
          "items": [
            "ST4",
            ",",
            "ST8",
            ",",
            "ST16",
            ",",
            "ST32",
            ",",
            "REV",
            ",",
            "CHOR",
            ",",
            "PAN",
            ",",
            "CRUSH",
            ",",
            "HALF",
            ",",
            "DBL",
            ",",
            "BZZ",
            ",",
            "MUTE",
            ",",
            "FLT",
            ",",
            "WASH",
            ",",
            "DIST",
            ",",
            "STOP"
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bgfillcolor": {
            "angle": 270.0,
            "autogradient": 0,
            "color": [
              0.051,
              0.0353,
              0.098,
              1.0
            ],
            "color1": [
              0.051,
              0.0353,
              0.098,
              1.0
            ],
            "color2": [
              0.051,
              0.0353,
              0.098,
              1.0
            ],
            "proportion": 0.39,
            "type": "color"
          },
          "fontsize": 11.0,
          "fontname": "Arial",
          "rounded": 4,
          "varname": "fxeff",
          "presentation": 1,
          "presentation_rect": [
            128,
            40,
            80,
            24
          ]
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
            216,
            44,
            36,
            16
          ],
          "text": "dist",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 2,
          "presentation": 1,
          "presentation_rect": [
            216,
            44,
            36,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-26",
          "maxclass": "live.menu",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "",
            "",
            "float"
          ],
          "patching_rect": [
            256,
            40,
            140,
            24
          ],
          "parameter_enable": 1,
          "varname": "p5248",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Distortion algo",
              "parameter_shortname": "Dist algo",
              "parameter_type": 2,
              "parameter_mmax": 5.0,
              "parameter_enum": [
                "TANH",
                "FOLD",
                "FUZZ",
                "WARM",
                "FL",
                "CRUSH"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "lcdbgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "lcdcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "inactivelcdcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "hltcolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "hlttextcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "tricolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 11.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            40,
            140,
            24
          ]
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
            1208,
            180,
            22
          ],
          "text": "prepend setparam 5248"
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
            16,
            64,
            380,
            16
          ],
          "text": "FX1.01 ...",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "fxsel",
          "presentation": 1,
          "presentation_rect": [
            16,
            64,
            380,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-29",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            84,
            44,
            24
          ],
          "text": "ST4",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "ST4",
          "varname": "p5120",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.01 ST4",
              "parameter_shortname": "ST4",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            84,
            44,
            24
          ]
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
            1236,
            180,
            22
          ],
          "text": "prepend setparam 5120"
        }
      },
      {
        "box": {
          "id": "obj-31",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            84,
            44,
            24
          ],
          "text": "DUB",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "DUB",
          "varname": "p5136",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.01 DUB",
              "parameter_shortname": "DUB",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-32",
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
          "text": "prepend setparam 5136"
        }
      },
      {
        "box": {
          "id": "obj-33",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            84,
            44,
            24
          ],
          "text": "PLATE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "PLATE",
          "varname": "p5152",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.01 PLATE",
              "parameter_shortname": "PLATE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-34",
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
          "text": "prepend setparam 5152"
        }
      },
      {
        "box": {
          "id": "obj-35",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            84,
            44,
            24
          ],
          "text": "OCT+",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "OCT+",
          "varname": "p5168",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.01 OCT+",
              "parameter_shortname": "OCT+",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            84,
            44,
            24
          ]
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
            1320,
            180,
            22
          ],
          "text": "prepend setparam 5168"
        }
      },
      {
        "box": {
          "id": "obj-37",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            84,
            44,
            24
          ],
          "text": "RPT2",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "RPT2",
          "varname": "p5184",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.01 RPT2",
              "parameter_shortname": "RPT2",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-38",
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
          "text": "prepend setparam 5184"
        }
      },
      {
        "box": {
          "id": "obj-39",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            84,
            44,
            24
          ],
          "text": "DELAY",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "DELAY",
          "varname": "p5200",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.01 DELAY",
              "parameter_shortname": "DELAY",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-40",
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
          "text": "prepend setparam 5200"
        }
      },
      {
        "box": {
          "id": "obj-41",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            84,
            44,
            24
          ],
          "text": "GATE4",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "GATE4",
          "varname": "p5216",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.01 GATE4",
              "parameter_shortname": "GATE4",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            84,
            44,
            24
          ]
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
            1404,
            180,
            22
          ],
          "text": "prepend setparam 5216"
        }
      },
      {
        "box": {
          "id": "obj-43",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            84,
            44,
            24
          ],
          "text": "RAT2",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "RAT2",
          "varname": "p5232",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.01 RAT2",
              "parameter_shortname": "RAT2",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-44",
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
          "text": "prepend setparam 5232"
        }
      },
      {
        "box": {
          "id": "obj-45",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            84,
            44,
            24
          ],
          "text": "ST8",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "ST8",
          "varname": "p5121",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.02 ST8",
              "parameter_shortname": "ST8",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-46",
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
          "text": "prepend setparam 5121"
        }
      },
      {
        "box": {
          "id": "obj-47",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            84,
            44,
            24
          ],
          "text": "GVERB",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "GVERB",
          "varname": "p5137",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.02 GVERB",
              "parameter_shortname": "GVERB",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            84,
            44,
            24
          ]
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
            210,
            816,
            180,
            22
          ],
          "text": "prepend setparam 5137"
        }
      },
      {
        "box": {
          "id": "obj-49",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            84,
            44,
            24
          ],
          "text": "SHIMR",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SHIMR",
          "varname": "p5153",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.02 SHIMR",
              "parameter_shortname": "SHIMR",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-50",
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
          "text": "prepend setparam 5153"
        }
      },
      {
        "box": {
          "id": "obj-51",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            84,
            44,
            24
          ],
          "text": "OCT-",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "OCT-",
          "varname": "p5169",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.02 OCT-",
              "parameter_shortname": "OCT-",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-52",
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
          "text": "prepend setparam 5169"
        }
      },
      {
        "box": {
          "id": "obj-53",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            84,
            44,
            24
          ],
          "text": "RPT6",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "RPT6",
          "varname": "p5185",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.02 RPT6",
              "parameter_shortname": "RPT6",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            84,
            44,
            24
          ]
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
            210,
            900,
            180,
            22
          ],
          "text": "prepend setparam 5185"
        }
      },
      {
        "box": {
          "id": "obj-55",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            84,
            44,
            24
          ],
          "text": "PNGPG",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "PNGPG",
          "varname": "p5201",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.02 PNGPG",
              "parameter_shortname": "PNGPG",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-56",
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
          "text": "prepend setparam 5201"
        }
      },
      {
        "box": {
          "id": "obj-57",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            84,
            44,
            24
          ],
          "text": "GATE8",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "GATE8",
          "varname": "p5217",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.02 GATE8",
              "parameter_shortname": "GATE8",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            84,
            44,
            24
          ]
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
            210,
            956,
            180,
            22
          ],
          "text": "prepend setparam 5217"
        }
      },
      {
        "box": {
          "id": "obj-59",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            84,
            44,
            24
          ],
          "text": "RAT3",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "RAT3",
          "varname": "p5233",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.02 RAT3",
              "parameter_shortname": "RAT3",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-60",
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
          "text": "prepend setparam 5233"
        }
      },
      {
        "box": {
          "id": "obj-61",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            84,
            44,
            24
          ],
          "text": "ST16",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "ST16",
          "varname": "p5122",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.03 ST16",
              "parameter_shortname": "ST16",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-62",
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
          "text": "prepend setparam 5122"
        }
      },
      {
        "box": {
          "id": "obj-63",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            84,
            44,
            24
          ],
          "text": "TGATE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "TGATE",
          "varname": "p5138",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.03 TGATE",
              "parameter_shortname": "TGATE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            84,
            44,
            24
          ]
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
            210,
            1040,
            180,
            22
          ],
          "text": "prepend setparam 5138"
        }
      },
      {
        "box": {
          "id": "obj-65",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            84,
            44,
            24
          ],
          "text": "PONG",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "PONG",
          "varname": "p5154",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.03 PONG",
              "parameter_shortname": "PONG",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-66",
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
          "text": "prepend setparam 5154"
        }
      },
      {
        "box": {
          "id": "obj-67",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            84,
            44,
            24
          ],
          "text": "DSTRY",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "DSTRY",
          "varname": "p5170",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.03 DSTRY",
              "parameter_shortname": "DSTRY",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-68",
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
          "text": "prepend setparam 5170"
        }
      },
      {
        "box": {
          "id": "obj-69",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            84,
            44,
            24
          ],
          "text": "SHUF",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SHUF",
          "varname": "p5186",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.03 SHUF",
              "parameter_shortname": "SHUF",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            84,
            44,
            24
          ]
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
            210,
            1124,
            180,
            22
          ],
          "text": "prepend setparam 5186"
        }
      },
      {
        "box": {
          "id": "obj-71",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            84,
            44,
            24
          ],
          "text": "FADE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "FADE",
          "varname": "p5202",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.03 FADE",
              "parameter_shortname": "FADE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-72",
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
          "text": "prepend setparam 5202"
        }
      },
      {
        "box": {
          "id": "obj-73",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            84,
            44,
            24
          ],
          "text": "GAT16",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "GAT16",
          "varname": "p5218",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.03 GAT16",
              "parameter_shortname": "GAT16",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-74",
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
          "text": "prepend setparam 5218"
        }
      },
      {
        "box": {
          "id": "obj-75",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            84,
            44,
            24
          ],
          "text": "RAT4",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "RAT4",
          "varname": "p5234",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.03 RAT4",
              "parameter_shortname": "RAT4",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            84,
            44,
            24
          ]
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
            1208,
            180,
            22
          ],
          "text": "prepend setparam 5234"
        }
      },
      {
        "box": {
          "id": "obj-77",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            84,
            44,
            24
          ],
          "text": "ST32",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "ST32",
          "varname": "p5123",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.04 ST32",
              "parameter_shortname": "ST32",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-78",
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
          "text": "prepend setparam 5123"
        }
      },
      {
        "box": {
          "id": "obj-79",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            84,
            44,
            24
          ],
          "text": "FLANG",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "FLANG",
          "varname": "p5139",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.04 FLANG",
              "parameter_shortname": "FLANG",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-80",
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
          "text": "prepend setparam 5139"
        }
      },
      {
        "box": {
          "id": "obj-81",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            84,
            44,
            24
          ],
          "text": "SLAP",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SLAP",
          "varname": "p5155",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.04 SLAP",
              "parameter_shortname": "SLAP",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            84,
            44,
            24
          ]
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
            1292,
            180,
            22
          ],
          "text": "prepend setparam 5155"
        }
      },
      {
        "box": {
          "id": "obj-83",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            84,
            44,
            24
          ],
          "text": "REPT",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "REPT",
          "varname": "p5171",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.04 REPT",
              "parameter_shortname": "REPT",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-84",
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
          "text": "prepend setparam 5171"
        }
      },
      {
        "box": {
          "id": "obj-85",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            84,
            44,
            24
          ],
          "text": "Q6/8",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "Q6/8",
          "varname": "p5187",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.04 Q6/8",
              "parameter_shortname": "Q6/8",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-86",
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
          "text": "prepend setparam 5187"
        }
      },
      {
        "box": {
          "id": "obj-87",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            84,
            44,
            24
          ],
          "text": "FBACK",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "FBACK",
          "varname": "p5203",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.04 FBACK",
              "parameter_shortname": "FBACK",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            84,
            44,
            24
          ]
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
            1376,
            180,
            22
          ],
          "text": "prepend setparam 5203"
        }
      },
      {
        "box": {
          "id": "obj-89",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            84,
            44,
            24
          ],
          "text": "TRNCE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "TRNCE",
          "varname": "p5219",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.04 TRNCE",
              "parameter_shortname": "TRNCE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-90",
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
          "text": "prepend setparam 5219"
        }
      },
      {
        "box": {
          "id": "obj-91",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            84,
            44,
            24
          ],
          "text": "RAT8",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "RAT8",
          "varname": "p5235",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.04 RAT8",
              "parameter_shortname": "RAT8",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-92",
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
          "text": "prepend setparam 5235"
        }
      },
      {
        "box": {
          "id": "obj-93",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            84,
            44,
            24
          ],
          "text": "REV",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "REV",
          "varname": "p5124",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.05 REV",
              "parameter_shortname": "REV",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            84,
            44,
            24
          ]
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
            400,
            788,
            180,
            22
          ],
          "text": "prepend setparam 5124"
        }
      },
      {
        "box": {
          "id": "obj-95",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            84,
            44,
            24
          ],
          "text": "RING",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "RING",
          "varname": "p5140",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.05 RING",
              "parameter_shortname": "RING",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-96",
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
          "text": "prepend setparam 5140"
        }
      },
      {
        "box": {
          "id": "obj-97",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            84,
            44,
            24
          ],
          "text": "PHASE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "PHASE",
          "varname": "p5156",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.05 PHASE",
              "parameter_shortname": "PHASE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-98",
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
          "text": "prepend setparam 5156"
        }
      },
      {
        "box": {
          "id": "obj-99",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            84,
            44,
            24
          ],
          "text": "RGRN",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "RGRN",
          "varname": "p5172",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.05 RGRN",
              "parameter_shortname": "RGRN",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            84,
            44,
            24
          ]
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
            400,
            872,
            180,
            22
          ],
          "text": "prepend setparam 5172"
        }
      },
      {
        "box": {
          "id": "obj-101",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            84,
            44,
            24
          ],
          "text": "PUNCH",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "PUNCH",
          "varname": "p5188",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.05 PUNCH",
              "parameter_shortname": "PUNCH",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-102",
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
          "text": "prepend setparam 5188"
        }
      },
      {
        "box": {
          "id": "obj-103",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            84,
            44,
            24
          ],
          "text": "DRONE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "DRONE",
          "varname": "p5204",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.05 DRONE",
              "parameter_shortname": "DRONE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-104",
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
          "text": "prepend setparam 5204"
        }
      },
      {
        "box": {
          "id": "obj-105",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            84,
            44,
            24
          ],
          "text": "TREM",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "TREM",
          "varname": "p5220",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.05 TREM",
              "parameter_shortname": "TREM",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-106",
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
          "text": "prepend setparam 5220"
        }
      },
      {
        "box": {
          "id": "obj-107",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            84,
            44,
            24
          ],
          "text": "STRUM",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "STRUM",
          "varname": "p5236",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.05 STRUM",
              "parameter_shortname": "STRUM",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-108",
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
          "text": "prepend setparam 5236"
        }
      },
      {
        "box": {
          "id": "obj-109",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            84,
            44,
            24
          ],
          "text": "CHOR",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "CHOR",
          "varname": "p5125",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.06 CHOR",
              "parameter_shortname": "CHOR",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            84,
            44,
            24
          ]
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
            400,
            1012,
            180,
            22
          ],
          "text": "prepend setparam 5125"
        }
      },
      {
        "box": {
          "id": "obj-111",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            84,
            44,
            24
          ],
          "text": "FOLD",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "FOLD",
          "varname": "p5141",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.06 FOLD",
              "parameter_shortname": "FOLD",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-112",
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
          "text": "prepend setparam 5141"
        }
      },
      {
        "box": {
          "id": "obj-113",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            84,
            44,
            24
          ],
          "text": "WAH",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "WAH",
          "varname": "p5157",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.06 WAH",
              "parameter_shortname": "WAH",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-114",
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
          "text": "prepend setparam 5157"
        }
      },
      {
        "box": {
          "id": "obj-115",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            84,
            44,
            24
          ],
          "text": "FREEZ",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "FREEZ",
          "varname": "p5173",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.06 FREEZ",
              "parameter_shortname": "FREEZ",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            84,
            44,
            24
          ]
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
            400,
            1096,
            180,
            22
          ],
          "text": "prepend setparam 5173"
        }
      },
      {
        "box": {
          "id": "obj-117",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            84,
            44,
            24
          ],
          "text": "FILL",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "FILL",
          "varname": "p5189",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.06 FILL",
              "parameter_shortname": "FILL",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-118",
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
          "text": "prepend setparam 5189"
        }
      },
      {
        "box": {
          "id": "obj-119",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            84,
            44,
            24
          ],
          "text": "FREEZ",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "FREEZ",
          "varname": "p5205",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.06 FREEZ",
              "parameter_shortname": "FREEZ",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-120",
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
          "text": "prepend setparam 5205"
        }
      },
      {
        "box": {
          "id": "obj-121",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            84,
            44,
            24
          ],
          "text": "PAN",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "PAN",
          "varname": "p5221",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.06 PAN",
              "parameter_shortname": "PAN",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            84,
            44,
            24
          ]
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
            400,
            1180,
            180,
            22
          ],
          "text": "prepend setparam 5221"
        }
      },
      {
        "box": {
          "id": "obj-123",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            84,
            44,
            24
          ],
          "text": "TRNC2",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "TRNC2",
          "varname": "p5237",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.06 TRNC2",
              "parameter_shortname": "TRNC2",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-124",
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
          "text": "prepend setparam 5237"
        }
      },
      {
        "box": {
          "id": "obj-125",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            84,
            44,
            24
          ],
          "text": "PAN",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "PAN",
          "varname": "p5126",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.07 PAN",
              "parameter_shortname": "PAN",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-126",
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
          "text": "prepend setparam 5126"
        }
      },
      {
        "box": {
          "id": "obj-127",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            84,
            44,
            24
          ],
          "text": "COMB",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "COMB",
          "varname": "p5142",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.07 COMB",
              "parameter_shortname": "COMB",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            84,
            44,
            24
          ]
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
            400,
            1264,
            180,
            22
          ],
          "text": "prepend setparam 5142"
        }
      },
      {
        "box": {
          "id": "obj-129",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            84,
            44,
            24
          ],
          "text": "TREM",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "TREM",
          "varname": "p5158",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.07 TREM",
              "parameter_shortname": "TREM",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-130",
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
          "text": "prepend setparam 5158"
        }
      },
      {
        "box": {
          "id": "obj-131",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            84,
            44,
            24
          ],
          "text": "METAL",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "METAL",
          "varname": "p5174",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.07 METAL",
              "parameter_shortname": "METAL",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-132",
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
          "text": "prepend setparam 5174"
        }
      },
      {
        "box": {
          "id": "obj-133",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            84,
            44,
            24
          ],
          "text": "BUILD",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "BUILD",
          "varname": "p5190",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.07 BUILD",
              "parameter_shortname": "BUILD",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            84,
            44,
            24
          ]
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
            400,
            1348,
            180,
            22
          ],
          "text": "prepend setparam 5190"
        }
      },
      {
        "box": {
          "id": "obj-135",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            84,
            44,
            24
          ],
          "text": "UNI",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "UNI",
          "varname": "p5206",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.07 UNI",
              "parameter_shortname": "UNI",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-136",
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
          "text": "prepend setparam 5206"
        }
      },
      {
        "box": {
          "id": "obj-137",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            84,
            44,
            24
          ],
          "text": "PUMP",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "PUMP",
          "varname": "p5222",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.07 PUMP",
              "parameter_shortname": "PUMP",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-138",
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
          "text": "prepend setparam 5222"
        }
      },
      {
        "box": {
          "id": "obj-139",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            84,
            44,
            24
          ],
          "text": "STAB",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "STAB",
          "varname": "p5238",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.07 STAB",
              "parameter_shortname": "STAB",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            84,
            44,
            24
          ]
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
            590,
            760,
            180,
            22
          ],
          "text": "prepend setparam 5238"
        }
      },
      {
        "box": {
          "id": "obj-141",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            84,
            44,
            24
          ],
          "text": "CRUSH",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "CRUSH",
          "varname": "p5127",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.08 CRUSH",
              "parameter_shortname": "CRUSH",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-142",
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
          "text": "prepend setparam 5127"
        }
      },
      {
        "box": {
          "id": "obj-143",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            84,
            44,
            24
          ],
          "text": "VOWEL",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "VOWEL",
          "varname": "p5143",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.08 VOWEL",
              "parameter_shortname": "VOWEL",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-144",
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
          "text": "prepend setparam 5143"
        }
      },
      {
        "box": {
          "id": "obj-145",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            84,
            44,
            24
          ],
          "text": "HAAS",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "HAAS",
          "varname": "p5159",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.08 HAAS",
              "parameter_shortname": "HAAS",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            84,
            44,
            24
          ]
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
            590,
            844,
            180,
            22
          ],
          "text": "prepend setparam 5159"
        }
      },
      {
        "box": {
          "id": "obj-147",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            84,
            44,
            24
          ],
          "text": "FRMNT",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "FRMNT",
          "varname": "p5175",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.08 FRMNT",
              "parameter_shortname": "FRMNT",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-148",
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
          "text": "prepend setparam 5175"
        }
      },
      {
        "box": {
          "id": "obj-149",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            84,
            44,
            24
          ],
          "text": "BALL",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "BALL",
          "varname": "p5191",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.08 BALL",
              "parameter_shortname": "BALL",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-150",
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
          "text": "prepend setparam 5191"
        }
      },
      {
        "box": {
          "id": "obj-151",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            84,
            44,
            24
          ],
          "text": "UNILO",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "UNILO",
          "varname": "p5207",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.08 UNILO",
              "parameter_shortname": "UNILO",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            84,
            44,
            24
          ]
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
            590,
            928,
            180,
            22
          ],
          "text": "prepend setparam 5207"
        }
      },
      {
        "box": {
          "id": "obj-153",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            84,
            44,
            24
          ],
          "text": "SWELL",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SWELL",
          "varname": "p5223",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.08 SWELL",
              "parameter_shortname": "SWELL",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            84,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-154",
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
          "text": "prepend setparam 5223"
        }
      },
      {
        "box": {
          "id": "obj-155",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            84,
            44,
            24
          ],
          "text": "RINGL",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "RINGL",
          "varname": "p5239",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.08 RINGL",
              "parameter_shortname": "RINGL",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            84,
            44,
            24
          ]
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
            590,
            984,
            180,
            22
          ],
          "text": "prepend setparam 5239"
        }
      },
      {
        "box": {
          "id": "obj-157",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            112,
            44,
            24
          ],
          "text": "HALF",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "HALF",
          "varname": "p5128",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.09 HALF",
              "parameter_shortname": "HALF",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-158",
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
          "text": "prepend setparam 5128"
        }
      },
      {
        "box": {
          "id": "obj-159",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            112,
            44,
            24
          ],
          "text": "ISO-LO",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "ISO-LO",
          "varname": "p5144",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.09 ISO-LO",
              "parameter_shortname": "ISO-LO",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-160",
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
          "text": "prepend setparam 5144"
        }
      },
      {
        "box": {
          "id": "obj-161",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            112,
            44,
            24
          ],
          "text": "VIBRA",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "VIBRA",
          "varname": "p5160",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.09 VIBRA",
              "parameter_shortname": "VIBRA",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            112,
            44,
            24
          ]
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
            590,
            1068,
            180,
            22
          ],
          "text": "prepend setparam 5160"
        }
      },
      {
        "box": {
          "id": "obj-163",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            112,
            44,
            24
          ],
          "text": "FSHFT",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "FSHFT",
          "varname": "p5176",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.09 FSHFT",
              "parameter_shortname": "FSHFT",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-164",
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
          "text": "prepend setparam 5176"
        }
      },
      {
        "box": {
          "id": "obj-165",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            112,
            44,
            24
          ],
          "text": "LPF",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "LPF",
          "varname": "p5192",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.09 LPF",
              "parameter_shortname": "LPF",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-166",
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
          "text": "prepend setparam 5192"
        }
      },
      {
        "box": {
          "id": "obj-167",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            112,
            44,
            24
          ],
          "text": "DETUN",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "DETUN",
          "varname": "p5208",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.09 DETUN",
              "parameter_shortname": "DETUN",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            112,
            44,
            24
          ]
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
            590,
            1152,
            180,
            22
          ],
          "text": "prepend setparam 5208"
        }
      },
      {
        "box": {
          "id": "obj-169",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            112,
            44,
            24
          ],
          "text": "NOISE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "NOISE",
          "varname": "p5224",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.09 NOISE",
              "parameter_shortname": "NOISE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-170",
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
          "text": "prepend setparam 5224"
        }
      },
      {
        "box": {
          "id": "obj-171",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            16,
            112,
            44,
            24
          ],
          "text": "RINGH",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "RINGH",
          "varname": "p5240",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.09 RINGH",
              "parameter_shortname": "RINGH",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            16,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-172",
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
          "text": "prepend setparam 5240"
        }
      },
      {
        "box": {
          "id": "obj-173",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            112,
            44,
            24
          ],
          "text": "DBL",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "DBL",
          "varname": "p5129",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.10 DBL",
              "parameter_shortname": "DBL",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            112,
            44,
            24
          ]
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
            590,
            1236,
            180,
            22
          ],
          "text": "prepend setparam 5129"
        }
      },
      {
        "box": {
          "id": "obj-175",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            112,
            44,
            24
          ],
          "text": "ISO-MD",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "ISO-MD",
          "varname": "p5145",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.10 ISO-MD",
              "parameter_shortname": "ISO-MD",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-176",
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
          "text": "prepend setparam 5145"
        }
      },
      {
        "box": {
          "id": "obj-177",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            112,
            44,
            24
          ],
          "text": "DIMEN",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "DIMEN",
          "varname": "p5161",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.10 DIMEN",
              "parameter_shortname": "DIMEN",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-178",
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
          "text": "prepend setparam 5161"
        }
      },
      {
        "box": {
          "id": "obj-179",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            112,
            44,
            24
          ],
          "text": "FUZZ",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "FUZZ",
          "varname": "p5177",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.10 FUZZ",
              "parameter_shortname": "FUZZ",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            112,
            44,
            24
          ]
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
            590,
            1320,
            180,
            22
          ],
          "text": "prepend setparam 5177"
        }
      },
      {
        "box": {
          "id": "obj-181",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            112,
            44,
            24
          ],
          "text": "HPF",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "HPF",
          "varname": "p5193",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.10 HPF",
              "parameter_shortname": "HPF",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-182",
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
          "text": "prepend setparam 5193"
        }
      },
      {
        "box": {
          "id": "obj-183",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            112,
            44,
            24
          ],
          "text": "PLFO",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "PLFO",
          "varname": "p5209",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.10 PLFO",
              "parameter_shortname": "PLFO",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-184",
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
          "text": "prepend setparam 5209"
        }
      },
      {
        "box": {
          "id": "obj-185",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            112,
            44,
            24
          ],
          "text": "WIDE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "WIDE",
          "varname": "p5225",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.10 WIDE",
              "parameter_shortname": "WIDE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            112,
            44,
            24
          ]
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
            590,
            1404,
            180,
            22
          ],
          "text": "prepend setparam 5225"
        }
      },
      {
        "box": {
          "id": "obj-187",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            64,
            112,
            44,
            24
          ],
          "text": "VIBR",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "VIBR",
          "varname": "p5241",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.10 VIBR",
              "parameter_shortname": "VIBR",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            64,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-188",
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
          "text": "prepend setparam 5241"
        }
      },
      {
        "box": {
          "id": "obj-189",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            112,
            44,
            24
          ],
          "text": "BZZ",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "BZZ",
          "varname": "p5130",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.11 BZZ",
              "parameter_shortname": "BZZ",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-190",
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
          "text": "prepend setparam 5130"
        }
      },
      {
        "box": {
          "id": "obj-191",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            112,
            44,
            24
          ],
          "text": "ISO-HI",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "ISO-HI",
          "varname": "p5146",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.11 ISO-HI",
              "parameter_shortname": "ISO-HI",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            112,
            44,
            24
          ]
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
            780,
            816,
            180,
            22
          ],
          "text": "prepend setparam 5146"
        }
      },
      {
        "box": {
          "id": "obj-193",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            112,
            44,
            24
          ],
          "text": "SPRNG",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SPRNG",
          "varname": "p5162",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.11 SPRNG",
              "parameter_shortname": "SPRNG",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-194",
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
          "text": "prepend setparam 5162"
        }
      },
      {
        "box": {
          "id": "obj-195",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            112,
            44,
            24
          ],
          "text": "GSEQ",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "GSEQ",
          "varname": "p5178",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.11 GSEQ",
              "parameter_shortname": "GSEQ",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-196",
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
          "text": "prepend setparam 5178"
        }
      },
      {
        "box": {
          "id": "obj-197",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            112,
            44,
            24
          ],
          "text": "SQUSH",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SQUSH",
          "varname": "p5194",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.11 SQUSH",
              "parameter_shortname": "SQUSH",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            112,
            44,
            24
          ]
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
            780,
            900,
            180,
            22
          ],
          "text": "prepend setparam 5194"
        }
      },
      {
        "box": {
          "id": "obj-199",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            112,
            44,
            24
          ],
          "text": "BND+",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "BND+",
          "varname": "p5210",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.11 BND+",
              "parameter_shortname": "BND+",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-200",
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
          "text": "prepend setparam 5210"
        }
      },
      {
        "box": {
          "id": "obj-201",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            112,
            44,
            24
          ],
          "text": "MONO",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "MONO",
          "varname": "p5226",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.11 MONO",
              "parameter_shortname": "MONO",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-202",
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
          "text": "prepend setparam 5226"
        }
      },
      {
        "box": {
          "id": "obj-203",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            112,
            112,
            44,
            24
          ],
          "text": "CHOP",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "CHOP",
          "varname": "p5242",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.11 CHOP",
              "parameter_shortname": "CHOP",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            112,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-204",
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
          "text": "prepend setparam 5242"
        }
      },
      {
        "box": {
          "id": "obj-205",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            112,
            44,
            24
          ],
          "text": "MUTE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "MUTE",
          "varname": "p5131",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.12 MUTE",
              "parameter_shortname": "MUTE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-206",
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
          "text": "prepend setparam 5131"
        }
      },
      {
        "box": {
          "id": "obj-207",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            112,
            44,
            24
          ],
          "text": "PHONE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "PHONE",
          "varname": "p5147",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.12 PHONE",
              "parameter_shortname": "PHONE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            112,
            44,
            24
          ]
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
            780,
            1040,
            180,
            22
          ],
          "text": "prepend setparam 5147"
        }
      },
      {
        "box": {
          "id": "obj-209",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            112,
            44,
            24
          ],
          "text": "SIREN",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SIREN",
          "varname": "p5163",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.12 SIREN",
              "parameter_shortname": "SIREN",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-210",
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
          "text": "prepend setparam 5163"
        }
      },
      {
        "box": {
          "id": "obj-211",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            112,
            44,
            24
          ],
          "text": "SRAMP",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SRAMP",
          "varname": "p5179",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.12 SRAMP",
              "parameter_shortname": "SRAMP",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-212",
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
          "text": "prepend setparam 5179"
        }
      },
      {
        "box": {
          "id": "obj-213",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            112,
            44,
            24
          ],
          "text": "SCRTC",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SCRTC",
          "varname": "p5195",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.12 SCRTC",
              "parameter_shortname": "SCRTC",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            112,
            44,
            24
          ]
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
            780,
            1124,
            180,
            22
          ],
          "text": "prepend setparam 5195"
        }
      },
      {
        "box": {
          "id": "obj-215",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            112,
            44,
            24
          ],
          "text": "BND-",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "BND-",
          "varname": "p5211",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.12 BND-",
              "parameter_shortname": "BND-",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-216",
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
          "text": "prepend setparam 5211"
        }
      },
      {
        "box": {
          "id": "obj-217",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            112,
            44,
            24
          ],
          "text": "BRITE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "BRITE",
          "varname": "p5227",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.12 BRITE",
              "parameter_shortname": "BRITE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-218",
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
          "text": "prepend setparam 5227"
        }
      },
      {
        "box": {
          "id": "obj-219",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            160,
            112,
            44,
            24
          ],
          "text": "HALF",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "HALF",
          "varname": "p5243",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.12 HALF",
              "parameter_shortname": "HALF",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            160,
            112,
            44,
            24
          ]
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
            780,
            1208,
            180,
            22
          ],
          "text": "prepend setparam 5243"
        }
      },
      {
        "box": {
          "id": "obj-221",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            112,
            44,
            24
          ],
          "text": "FLT",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "FLT",
          "varname": "p5132",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.13 FLT",
              "parameter_shortname": "FLT",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-222",
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
          "text": "prepend setparam 5132"
        }
      },
      {
        "box": {
          "id": "obj-223",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            112,
            44,
            24
          ],
          "text": "GRAIN",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "GRAIN",
          "varname": "p5148",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.13 GRAIN",
              "parameter_shortname": "GRAIN",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-224",
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
          "text": "prepend setparam 5148"
        }
      },
      {
        "box": {
          "id": "obj-225",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            112,
            44,
            24
          ],
          "text": "WARM",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "WARM",
          "varname": "p5164",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.13 WARM",
              "parameter_shortname": "WARM",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            112,
            44,
            24
          ]
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
            780,
            1292,
            180,
            22
          ],
          "text": "prepend setparam 5164"
        }
      },
      {
        "box": {
          "id": "obj-227",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            112,
            44,
            24
          ],
          "text": "VSTOP",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "VSTOP",
          "varname": "p5180",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.13 VSTOP",
              "parameter_shortname": "VSTOP",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-228",
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
          "text": "prepend setparam 5180"
        }
      },
      {
        "box": {
          "id": "obj-229",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            112,
            44,
            24
          ],
          "text": "SCRTF",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SCRTF",
          "varname": "p5196",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.13 SCRTF",
              "parameter_shortname": "SCRTF",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-230",
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
          "text": "prepend setparam 5196"
        }
      },
      {
        "box": {
          "id": "obj-231",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            112,
            44,
            24
          ],
          "text": "WARBL",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "WARBL",
          "varname": "p5212",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.13 WARBL",
              "parameter_shortname": "WARBL",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            112,
            44,
            24
          ]
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
            780,
            1376,
            180,
            22
          ],
          "text": "prepend setparam 5212"
        }
      },
      {
        "box": {
          "id": "obj-233",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            112,
            44,
            24
          ],
          "text": "DARK",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "DARK",
          "varname": "p5228",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.13 DARK",
              "parameter_shortname": "DARK",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-234",
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
          "text": "prepend setparam 5228"
        }
      },
      {
        "box": {
          "id": "obj-235",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            208,
            112,
            44,
            24
          ],
          "text": "DBL",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "DBL",
          "varname": "p5244",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.13 DBL",
              "parameter_shortname": "DBL",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            208,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-236",
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
          "text": "prepend setparam 5244"
        }
      },
      {
        "box": {
          "id": "obj-237",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            112,
            44,
            24
          ],
          "text": "WASH",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "WASH",
          "varname": "p5133",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.14 WASH",
              "parameter_shortname": "WASH",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            112,
            44,
            24
          ]
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
            970,
            788,
            180,
            22
          ],
          "text": "prepend setparam 5133"
        }
      },
      {
        "box": {
          "id": "obj-239",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            112,
            44,
            24
          ],
          "text": "WOW",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "WOW",
          "varname": "p5149",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.14 WOW",
              "parameter_shortname": "WOW",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-240",
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
          "text": "prepend setparam 5149"
        }
      },
      {
        "box": {
          "id": "obj-241",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            112,
            44,
            24
          ],
          "text": "SUB",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SUB",
          "varname": "p5165",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.14 SUB",
              "parameter_shortname": "SUB",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-242",
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
          "text": "prepend setparam 5165"
        }
      },
      {
        "box": {
          "id": "obj-243",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            112,
            44,
            24
          ],
          "text": "RADIO",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "RADIO",
          "varname": "p5181",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.14 RADIO",
              "parameter_shortname": "RADIO",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            112,
            44,
            24
          ]
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
            970,
            872,
            180,
            22
          ],
          "text": "prepend setparam 5181"
        }
      },
      {
        "box": {
          "id": "obj-245",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            112,
            44,
            24
          ],
          "text": "GLCH",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "GLCH",
          "varname": "p5197",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.14 GLCH",
              "parameter_shortname": "GLCH",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-246",
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
          "text": "prepend setparam 5197"
        }
      },
      {
        "box": {
          "id": "obj-247",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            112,
            44,
            24
          ],
          "text": "5TH",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "5TH",
          "varname": "p5213",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.14 5TH",
              "parameter_shortname": "5TH",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-248",
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
          "text": "prepend setparam 5213"
        }
      },
      {
        "box": {
          "id": "obj-249",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            112,
            44,
            24
          ],
          "text": "DRIVE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "DRIVE",
          "varname": "p5229",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.14 DRIVE",
              "parameter_shortname": "DRIVE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            112,
            44,
            24
          ]
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
            970,
            956,
            180,
            22
          ],
          "text": "prepend setparam 5229"
        }
      },
      {
        "box": {
          "id": "obj-251",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            256,
            112,
            44,
            24
          ],
          "text": "GLCH",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "GLCH",
          "varname": "p5245",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.14 GLCH",
              "parameter_shortname": "GLCH",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            256,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-252",
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
          "id": "obj-253",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            112,
            44,
            24
          ],
          "text": "DIST",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "DIST",
          "varname": "p5134",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.15 DIST",
              "parameter_shortname": "DIST",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            112,
            44,
            24
          ]
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
            970,
            1012,
            180,
            22
          ],
          "text": "prepend setparam 5134"
        }
      },
      {
        "box": {
          "id": "obj-255",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            112,
            44,
            24
          ],
          "text": "DOWN",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "DOWN",
          "varname": "p5150",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.15 DOWN",
              "parameter_shortname": "DOWN",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-256",
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
          "text": "prepend setparam 5150"
        }
      },
      {
        "box": {
          "id": "obj-257",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            112,
            44,
            24
          ],
          "text": "AIR",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "AIR",
          "varname": "p5166",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.15 AIR",
              "parameter_shortname": "AIR",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-258",
          "maxclass": "newobj",
          "numinlets": 2,
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
          "text": "prepend setparam 5166"
        }
      },
      {
        "box": {
          "id": "obj-259",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            112,
            44,
            24
          ],
          "text": "NGATE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "NGATE",
          "varname": "p5182",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.15 NGATE",
              "parameter_shortname": "NGATE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            112,
            44,
            24
          ]
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
            970,
            1096,
            180,
            22
          ],
          "text": "prepend setparam 5182"
        }
      },
      {
        "box": {
          "id": "obj-261",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            112,
            44,
            24
          ],
          "text": "BLIND",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "BLIND",
          "varname": "p5198",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.15 BLIND",
              "parameter_shortname": "BLIND",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-262",
          "maxclass": "newobj",
          "numinlets": 2,
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
          "text": "prepend setparam 5198"
        }
      },
      {
        "box": {
          "id": "obj-263",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            112,
            44,
            24
          ],
          "text": "SUB",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SUB",
          "varname": "p5214",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.15 SUB",
              "parameter_shortname": "SUB",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-264",
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
          "text": "prepend setparam 5214"
        }
      },
      {
        "box": {
          "id": "obj-265",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            112,
            44,
            24
          ],
          "text": "SGATE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SGATE",
          "varname": "p5230",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.15 SGATE",
              "parameter_shortname": "SGATE",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            112,
            44,
            24
          ]
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
            970,
            1180,
            180,
            22
          ],
          "text": "prepend setparam 5230"
        }
      },
      {
        "box": {
          "id": "obj-267",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            304,
            112,
            44,
            24
          ],
          "text": "REVG",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "REVG",
          "varname": "p5246",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.15 REVG",
              "parameter_shortname": "REVG",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            304,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-268",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            970,
            1208,
            180,
            22
          ],
          "text": "prepend setparam 5246"
        }
      },
      {
        "box": {
          "id": "obj-269",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            112,
            44,
            24
          ],
          "text": "STOP",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "STOP",
          "varname": "p5135",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX1.16 STOP",
              "parameter_shortname": "STOP",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-270",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            970,
            1236,
            180,
            22
          ],
          "text": "prepend setparam 5135"
        }
      },
      {
        "box": {
          "id": "obj-271",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            112,
            44,
            24
          ],
          "text": "SCAT",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SCAT",
          "varname": "p5151",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX2.16 SCAT",
              "parameter_shortname": "SCAT",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            112,
            44,
            24
          ]
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
            970,
            1264,
            180,
            22
          ],
          "text": "prepend setparam 5151"
        }
      },
      {
        "box": {
          "id": "obj-273",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            112,
            44,
            24
          ],
          "text": "PUMP",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "PUMP",
          "varname": "p5167",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX3.16 PUMP",
              "parameter_shortname": "PUMP",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-274",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            970,
            1292,
            180,
            22
          ],
          "text": "prepend setparam 5167"
        }
      },
      {
        "box": {
          "id": "obj-275",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            112,
            44,
            24
          ],
          "text": "SHMOD",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SHMOD",
          "varname": "p5183",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX4.16 SHMOD",
              "parameter_shortname": "SHMOD",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-276",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            970,
            1320,
            180,
            22
          ],
          "text": "prepend setparam 5183"
        }
      },
      {
        "box": {
          "id": "obj-277",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            112,
            44,
            24
          ],
          "text": "S&H",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "S&H",
          "varname": "p5199",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX5.16 S&H",
              "parameter_shortname": "S&H",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            112,
            44,
            24
          ]
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
            970,
            1348,
            180,
            22
          ],
          "text": "prepend setparam 5199"
        }
      },
      {
        "box": {
          "id": "obj-279",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            112,
            44,
            24
          ],
          "text": "LOOP",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "LOOP",
          "varname": "p5215",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX6.16 LOOP",
              "parameter_shortname": "LOOP",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-280",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            970,
            1376,
            180,
            22
          ],
          "text": "prepend setparam 5215"
        }
      },
      {
        "box": {
          "id": "obj-281",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            112,
            44,
            24
          ],
          "text": "KILL",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "KILL",
          "varname": "p5231",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX7.16 KILL",
              "parameter_shortname": "KILL",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            112,
            44,
            24
          ]
        }
      },
      {
        "box": {
          "id": "obj-282",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            970,
            1404,
            180,
            22
          ],
          "text": "prepend setparam 5231"
        }
      },
      {
        "box": {
          "id": "obj-283",
          "maxclass": "live.text",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            352,
            112,
            44,
            24
          ],
          "text": "SIL",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "SIL",
          "varname": "p5247",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX8.16 SIL",
              "parameter_shortname": "SIL",
              "parameter_type": 2,
              "parameter_mmax": 1.0,
              "parameter_enum": [
                "off",
                "on"
              ],
              "parameter_initial_enable": 1,
              "parameter_initial": [
                0.0
              ],
              "parameter_speedlim": 0.0
            }
          },
          "bgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgoncolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "activebgcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activebgoncolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "textcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "textoffcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activetextcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activetextoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "rounded": 4,
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            352,
            112,
            44,
            24
          ]
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
            1160,
            760,
            180,
            22
          ],
          "text": "prepend setparam 5247"
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
            16,
            136,
            380,
            16
          ],
          "text": "cells = engage on / off; each is a Live parameter: automate it",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "presentation": 1,
          "presentation_rect": [
            16,
            136,
            380,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-286",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            40,
            216,
            16
          ],
          "text": "bank FX params",
          "textcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fontsize": 12.0,
          "fontname": "Arial",
          "fontface": 1,
          "textjustification": 1,
          "presentation": 1,
          "presentation_rect": [
            412,
            40,
            216,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-287",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            428,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb0AMOUNT",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 1 Amount",
              "parameter_shortname": "AMOUNT",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            428,
            64,
            40,
            40
          ]
        }
      },
      {
        "box": {
          "id": "obj-288",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            788,
            180,
            22
          ],
          "text": "prepend br Lfxb0AMOUNT"
        }
      },
      {
        "box": {
          "id": "obj-289",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            816,
            180,
            22
          ],
          "text": "receive pmor_rx"
        }
      },
      {
        "box": {
          "id": "obj-290",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "",
            "",
            ""
          ],
          "patching_rect": [
            1160,
            844,
            180,
            22
          ],
          "text": "js pmor_bank.js",
          "varname": "bank"
        }
      },
      {
        "box": {
          "id": "obj-291",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            872,
            180,
            22
          ],
          "text": "receive pmor_ui_status"
        }
      },
      {
        "box": {
          "id": "obj-292",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            1160,
            900,
            180,
            22
          ],
          "text": "route st"
        }
      },
      {
        "box": {
          "id": "obj-293",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            928,
            180,
            22
          ],
          "text": "receive pmor_ui_sel"
        }
      },
      {
        "box": {
          "id": "obj-294",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            1160,
            956,
            180,
            22
          ],
          "text": "send pmor_ui_sel"
        }
      },
      {
        "box": {
          "id": "obj-295",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            104,
            72,
            16
          ],
          "text": "AMOUNT",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb0AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            104,
            72,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-296",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb0AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            120,
            72,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-297",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            500,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb0LIDAR",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 1 Lidar",
              "parameter_shortname": "LIDAR",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            500,
            64,
            40,
            40
          ]
        }
      },
      {
        "box": {
          "id": "obj-298",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            984,
            180,
            22
          ],
          "text": "prepend br Lfxb0LIDAR"
        }
      },
      {
        "box": {
          "id": "obj-299",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            484,
            104,
            72,
            16
          ],
          "text": "LIDAR",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb0LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            104,
            72,
            16
          ]
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
            484,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb0LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            120,
            72,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-301",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            428,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb1AMOUNT",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 2 Amount",
              "parameter_shortname": "AMOUNT",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            428,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-302",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            1012,
            180,
            22
          ],
          "text": "prepend br Lfxb1AMOUNT"
        }
      },
      {
        "box": {
          "id": "obj-303",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            104,
            72,
            16
          ],
          "text": "AMOUNT",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb1AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            104,
            72,
            16
          ],
          "hidden": 1
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
            412,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb1AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-305",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            500,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb1LIDAR",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 2 Lidar",
              "parameter_shortname": "LIDAR",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            500,
            64,
            40,
            40
          ],
          "hidden": 1
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
            1160,
            1040,
            180,
            22
          ],
          "text": "prepend br Lfxb1LIDAR"
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
            484,
            104,
            72,
            16
          ],
          "text": "LIDAR",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb1LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-308",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            484,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb1LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-309",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            572,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb1MIX",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 2 Mix",
              "parameter_shortname": "MIX",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            572,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-310",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            1068,
            180,
            22
          ],
          "text": "prepend br Lfxb1MIX"
        }
      },
      {
        "box": {
          "id": "obj-311",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            556,
            104,
            72,
            16
          ],
          "text": "MIX",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb1MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-312",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            556,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb1MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-313",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            428,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb2AMOUNT",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 3 Amount",
              "parameter_shortname": "AMOUNT",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            428,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-314",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            1096,
            180,
            22
          ],
          "text": "prepend br Lfxb2AMOUNT"
        }
      },
      {
        "box": {
          "id": "obj-315",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            104,
            72,
            16
          ],
          "text": "AMOUNT",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb2AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            104,
            72,
            16
          ],
          "hidden": 1
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
            412,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb2AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-317",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            500,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb2LIDAR",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 3 Lidar",
              "parameter_shortname": "LIDAR",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            500,
            64,
            40,
            40
          ],
          "hidden": 1
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
            1160,
            1124,
            180,
            22
          ],
          "text": "prepend br Lfxb2LIDAR"
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
            484,
            104,
            72,
            16
          ],
          "text": "LIDAR",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb2LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-320",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            484,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb2LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-321",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            572,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb2MIX",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 3 Mix",
              "parameter_shortname": "MIX",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            572,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-322",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            1152,
            180,
            22
          ],
          "text": "prepend br Lfxb2MIX"
        }
      },
      {
        "box": {
          "id": "obj-323",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            556,
            104,
            72,
            16
          ],
          "text": "MIX",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb2MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-324",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            556,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb2MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-325",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            428,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb3AMOUNT",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 4 Amount",
              "parameter_shortname": "AMOUNT",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            428,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-326",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            1180,
            180,
            22
          ],
          "text": "prepend br Lfxb3AMOUNT"
        }
      },
      {
        "box": {
          "id": "obj-327",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            104,
            72,
            16
          ],
          "text": "AMOUNT",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb3AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            104,
            72,
            16
          ],
          "hidden": 1
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
            412,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb3AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-329",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            500,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb3LIDAR",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 4 Lidar",
              "parameter_shortname": "LIDAR",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            500,
            64,
            40,
            40
          ],
          "hidden": 1
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
            1160,
            1208,
            180,
            22
          ],
          "text": "prepend br Lfxb3LIDAR"
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
            484,
            104,
            72,
            16
          ],
          "text": "LIDAR",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb3LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-332",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            484,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb3LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-333",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            572,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb3MIX",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 4 Mix",
              "parameter_shortname": "MIX",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            572,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-334",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            1236,
            180,
            22
          ],
          "text": "prepend br Lfxb3MIX"
        }
      },
      {
        "box": {
          "id": "obj-335",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            556,
            104,
            72,
            16
          ],
          "text": "MIX",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb3MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-336",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            556,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb3MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-337",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            428,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb4AMOUNT",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 5 Amount",
              "parameter_shortname": "AMOUNT",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            428,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-338",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            1264,
            180,
            22
          ],
          "text": "prepend br Lfxb4AMOUNT"
        }
      },
      {
        "box": {
          "id": "obj-339",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            104,
            72,
            16
          ],
          "text": "AMOUNT",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb4AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            104,
            72,
            16
          ],
          "hidden": 1
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
            412,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb4AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-341",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            500,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb4LIDAR",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 5 Lidar",
              "parameter_shortname": "LIDAR",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            500,
            64,
            40,
            40
          ],
          "hidden": 1
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
            1160,
            1292,
            180,
            22
          ],
          "text": "prepend br Lfxb4LIDAR"
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
            484,
            104,
            72,
            16
          ],
          "text": "LIDAR",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb4LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-344",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            484,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb4LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-345",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            572,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb4MIX",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 5 Mix",
              "parameter_shortname": "MIX",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            572,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-346",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            1320,
            180,
            22
          ],
          "text": "prepend br Lfxb4MIX"
        }
      },
      {
        "box": {
          "id": "obj-347",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            556,
            104,
            72,
            16
          ],
          "text": "MIX",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb4MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-348",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            556,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb4MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-349",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            428,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb5AMOUNT",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 6 Amount",
              "parameter_shortname": "AMOUNT",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            428,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-350",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            1348,
            180,
            22
          ],
          "text": "prepend br Lfxb5AMOUNT"
        }
      },
      {
        "box": {
          "id": "obj-351",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            104,
            72,
            16
          ],
          "text": "AMOUNT",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb5AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-352",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb5AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-353",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            500,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb5LIDAR",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 6 Lidar",
              "parameter_shortname": "LIDAR",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            500,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-354",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1160,
            1376,
            180,
            22
          ],
          "text": "prepend br Lfxb5LIDAR"
        }
      },
      {
        "box": {
          "id": "obj-355",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            484,
            104,
            72,
            16
          ],
          "text": "LIDAR",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb5LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            104,
            72,
            16
          ],
          "hidden": 1
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
            484,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb5LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-357",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            572,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb5MIX",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 6 Mix",
              "parameter_shortname": "MIX",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            572,
            64,
            40,
            40
          ],
          "hidden": 1
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
            1160,
            1404,
            180,
            22
          ],
          "text": "prepend br Lfxb5MIX"
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
            556,
            104,
            72,
            16
          ],
          "text": "MIX",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb5MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-360",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            556,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb5MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-361",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            428,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb6AMOUNT",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 7 Amount",
              "parameter_shortname": "AMOUNT",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            428,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-362",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1350,
            760,
            180,
            22
          ],
          "text": "prepend br Lfxb6AMOUNT"
        }
      },
      {
        "box": {
          "id": "obj-363",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            104,
            72,
            16
          ],
          "text": "AMOUNT",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb6AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-364",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb6AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-365",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            500,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb6LIDAR",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 7 Lidar",
              "parameter_shortname": "LIDAR",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            500,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-366",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1350,
            788,
            180,
            22
          ],
          "text": "prepend br Lfxb6LIDAR"
        }
      },
      {
        "box": {
          "id": "obj-367",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            484,
            104,
            72,
            16
          ],
          "text": "LIDAR",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb6LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            104,
            72,
            16
          ],
          "hidden": 1
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
            484,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb6LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-369",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            572,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb6MIX",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 7 Mix",
              "parameter_shortname": "MIX",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            572,
            64,
            40,
            40
          ],
          "hidden": 1
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
            1350,
            816,
            180,
            22
          ],
          "text": "prepend br Lfxb6MIX"
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
            556,
            104,
            72,
            16
          ],
          "text": "MIX",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb6MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-372",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            556,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb6MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-373",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            428,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb7AMOUNT",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 8 Amount",
              "parameter_shortname": "AMOUNT",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            428,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-374",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1350,
            844,
            180,
            22
          ],
          "text": "prepend br Lfxb7AMOUNT"
        }
      },
      {
        "box": {
          "id": "obj-375",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            104,
            72,
            16
          ],
          "text": "AMOUNT",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb7AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-376",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            412,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb7AMOUNT",
          "presentation": 1,
          "presentation_rect": [
            412,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-377",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            500,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb7LIDAR",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 8 Lidar",
              "parameter_shortname": "LIDAR",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            500,
            64,
            40,
            40
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-378",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1350,
            872,
            180,
            22
          ],
          "text": "prepend br Lfxb7LIDAR"
        }
      },
      {
        "box": {
          "id": "obj-379",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            484,
            104,
            72,
            16
          ],
          "text": "LIDAR",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb7LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            104,
            72,
            16
          ],
          "hidden": 1
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
            484,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb7LIDAR",
          "presentation": 1,
          "presentation_rect": [
            484,
            120,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-381",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            572,
            64,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lfxb7MIX",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "FX Bank 8 Mix",
              "parameter_shortname": "MIX",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 1
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.3098,
            0.851,
            0.7686,
            1.0
          ],
          "fgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "activefgdialcolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "needlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "activeneedlecolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "bordercolor": [
            0.3569,
            0.2941,
            0.6196,
            1.0
          ],
          "focusbordercolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "presentation": 1,
          "presentation_rect": [
            572,
            64,
            40,
            40
          ],
          "hidden": 1
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
            1350,
            900,
            180,
            22
          ],
          "text": "prepend br Lfxb7MIX"
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
            556,
            104,
            72,
            16
          ],
          "text": "MIX",
          "textcolor": [
            0.7882,
            0.7373,
            0.9412,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 1,
          "varname": "bl_Lfxb7MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            104,
            72,
            16
          ],
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-384",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            556,
            120,
            72,
            16
          ],
          "text": "-",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "textjustification": 1,
          "varname": "bv_Lfxb7MIX",
          "presentation": 1,
          "presentation_rect": [
            556,
            120,
            72,
            16
          ],
          "hidden": 1
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
            1350,
            928,
            180,
            22
          ],
          "text": "prepend page"
        }
      },
      {
        "box": {
          "id": "obj-386",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1350,
            956,
            180,
            22
          ],
          "text": "receive pmor_rx"
        }
      },
      {
        "box": {
          "id": "obj-387",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "",
            "",
            ""
          ],
          "patching_rect": [
            1350,
            984,
            180,
            22
          ],
          "text": "js pmor_ui.js",
          "varname": "ui"
        }
      },
      {
        "box": {
          "id": "obj-388",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1350,
            1012,
            180,
            22
          ],
          "text": "receive pmor_ui_status"
        }
      },
      {
        "box": {
          "id": "obj-389",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            1350,
            1040,
            180,
            22
          ],
          "text": "route st"
        }
      },
      {
        "box": {
          "id": "obj-390",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            1350,
            1068,
            180,
            22
          ],
          "text": "js pmor_fx.js"
        }
      },
      {
        "box": {
          "id": "obj-391",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1350,
            1096,
            180,
            22
          ],
          "text": "receive pmor_rx"
        }
      },
      {
        "box": {
          "id": "obj-392",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1350,
            1124,
            180,
            22
          ],
          "text": "prepend bank"
        }
      },
      {
        "box": {
          "id": "obj-393",
          "maxclass": "newobj",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            1350,
            1152,
            180,
            22
          ],
          "text": "prepend eff"
        }
      }
    ],
    "lines": [
      {
        "patchline": {
          "source": [
            "obj-7",
            0
          ],
          "destination": [
            "obj-8",
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
            "obj-5",
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
            "obj-10",
            0
          ],
          "destination": [
            "obj-11",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-10",
            1
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
            "obj-11",
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
            "obj-16",
            0
          ],
          "destination": [
            "obj-17",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-13",
            0
          ],
          "destination": [
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-13",
            2
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
            "obj-13",
            3
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
            "obj-20",
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
            "obj-13",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-13",
            1
          ],
          "destination": [
            "obj-8",
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
            "obj-14",
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
            "obj-14",
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
            "obj-32",
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
            "obj-14",
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
            "obj-34",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-34",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-37",
            0
          ],
          "destination": [
            "obj-38",
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
            "obj-14",
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
            "obj-40",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-40",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-43",
            0
          ],
          "destination": [
            "obj-44",
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
            "obj-14",
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
            "obj-46",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-46",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-49",
            0
          ],
          "destination": [
            "obj-50",
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
            "obj-14",
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
            "obj-52",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-52",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-55",
            0
          ],
          "destination": [
            "obj-56",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-56",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-59",
            0
          ],
          "destination": [
            "obj-60",
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
            "obj-14",
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
            "obj-62",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-62",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-65",
            0
          ],
          "destination": [
            "obj-66",
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
            "obj-14",
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
            "obj-68",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-68",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-71",
            0
          ],
          "destination": [
            "obj-72",
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
            "obj-14",
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
            "obj-74",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-74",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-77",
            0
          ],
          "destination": [
            "obj-78",
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
            "obj-14",
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
            "obj-80",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-80",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-83",
            0
          ],
          "destination": [
            "obj-84",
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
            "obj-14",
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
            "obj-86",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-86",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-89",
            0
          ],
          "destination": [
            "obj-90",
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
            "obj-14",
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
            "obj-92",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-92",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-95",
            0
          ],
          "destination": [
            "obj-96",
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
            "obj-14",
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
            "obj-98",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-98",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-101",
            0
          ],
          "destination": [
            "obj-102",
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
            "obj-14",
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
            "obj-104",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-104",
            0
          ],
          "destination": [
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-105",
            0
          ],
          "destination": [
            "obj-106",
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
            "obj-14",
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
            "obj-108",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-108",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-111",
            0
          ],
          "destination": [
            "obj-112",
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
            "obj-14",
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
            "obj-114",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-114",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-117",
            0
          ],
          "destination": [
            "obj-118",
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
            "obj-14",
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
            "obj-120",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-120",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-123",
            0
          ],
          "destination": [
            "obj-124",
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
            "obj-14",
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
            "obj-126",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-126",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-129",
            0
          ],
          "destination": [
            "obj-130",
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
            "obj-14",
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
            "obj-132",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-132",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-135",
            0
          ],
          "destination": [
            "obj-136",
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
            "obj-14",
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
            "obj-138",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-138",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-141",
            0
          ],
          "destination": [
            "obj-142",
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
            "obj-14",
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
            "obj-144",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-144",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-147",
            0
          ],
          "destination": [
            "obj-148",
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
            "obj-14",
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
            "obj-150",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-150",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-153",
            0
          ],
          "destination": [
            "obj-154",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-154",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-157",
            0
          ],
          "destination": [
            "obj-158",
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
            "obj-14",
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
            "obj-160",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-160",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-163",
            0
          ],
          "destination": [
            "obj-164",
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
            "obj-14",
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
            "obj-166",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-166",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-169",
            0
          ],
          "destination": [
            "obj-170",
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
            "obj-14",
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
            "obj-172",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-172",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-175",
            0
          ],
          "destination": [
            "obj-176",
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
            "obj-14",
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
            "obj-178",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-178",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-181",
            0
          ],
          "destination": [
            "obj-182",
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
            "obj-14",
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
            "obj-184",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-184",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-187",
            0
          ],
          "destination": [
            "obj-188",
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
            "obj-14",
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
            "obj-190",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-190",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-193",
            0
          ],
          "destination": [
            "obj-194",
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
            "obj-14",
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
            "obj-196",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-196",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-199",
            0
          ],
          "destination": [
            "obj-200",
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
            "obj-14",
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
            "obj-202",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-202",
            0
          ],
          "destination": [
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-203",
            0
          ],
          "destination": [
            "obj-204",
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
            "obj-14",
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
            "obj-206",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-206",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-209",
            0
          ],
          "destination": [
            "obj-210",
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
            "obj-14",
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
            "obj-212",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-212",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-215",
            0
          ],
          "destination": [
            "obj-216",
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
            "obj-14",
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
            "obj-218",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-218",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-221",
            0
          ],
          "destination": [
            "obj-222",
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
            "obj-14",
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
            "obj-224",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-224",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-227",
            0
          ],
          "destination": [
            "obj-228",
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
            "obj-14",
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
            "obj-230",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-230",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-233",
            0
          ],
          "destination": [
            "obj-234",
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
            "obj-14",
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
            "obj-236",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-236",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-239",
            0
          ],
          "destination": [
            "obj-240",
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
            "obj-14",
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
            "obj-242",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-242",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-245",
            0
          ],
          "destination": [
            "obj-246",
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
            "obj-14",
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
            "obj-248",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-248",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-251",
            0
          ],
          "destination": [
            "obj-252",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-252",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-255",
            0
          ],
          "destination": [
            "obj-256",
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
            "obj-14",
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
            "obj-258",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-258",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-261",
            0
          ],
          "destination": [
            "obj-262",
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
            "obj-14",
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
            "obj-264",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-264",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-267",
            0
          ],
          "destination": [
            "obj-268",
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
            "obj-14",
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
            "obj-270",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-270",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-273",
            0
          ],
          "destination": [
            "obj-274",
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
            "obj-14",
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
            "obj-276",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-276",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-279",
            0
          ],
          "destination": [
            "obj-280",
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
            "obj-14",
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
            "obj-282",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-282",
            0
          ],
          "destination": [
            "obj-14",
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
            "obj-14",
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
            "obj-288",
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
            "obj-14",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-291",
            0
          ],
          "destination": [
            "obj-292",
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
            "obj-290",
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
            "obj-290",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-290",
            2
          ],
          "destination": [
            "obj-294",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-288",
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
            "obj-297",
            0
          ],
          "destination": [
            "obj-298",
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
            "obj-290",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-301",
            0
          ],
          "destination": [
            "obj-302",
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
            "obj-290",
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
            "obj-290",
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
            "obj-310",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-310",
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
            "obj-313",
            0
          ],
          "destination": [
            "obj-314",
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
            "obj-290",
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
            "obj-290",
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
            "obj-322",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-322",
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
            "obj-325",
            0
          ],
          "destination": [
            "obj-326",
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
            "obj-290",
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
            "obj-290",
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
            "obj-334",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-334",
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
            "obj-337",
            0
          ],
          "destination": [
            "obj-338",
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
            "obj-290",
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
            "obj-290",
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
            "obj-346",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-346",
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
            "obj-349",
            0
          ],
          "destination": [
            "obj-350",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-350",
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
            "obj-353",
            0
          ],
          "destination": [
            "obj-354",
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
            "obj-290",
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
            "obj-290",
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
            "obj-362",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-362",
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
            "obj-365",
            0
          ],
          "destination": [
            "obj-366",
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
            "obj-290",
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
            "obj-290",
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
            "obj-374",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-374",
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
            "obj-377",
            0
          ],
          "destination": [
            "obj-378",
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
            "obj-290",
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
            "obj-290",
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
            "obj-290",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-386",
            0
          ],
          "destination": [
            "obj-387",
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
            "obj-14",
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
            "obj-389",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-389",
            0
          ],
          "destination": [
            "obj-387",
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
            "obj-390",
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
            "obj-392",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-392",
            0
          ],
          "destination": [
            "obj-390",
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
            "obj-393",
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
            "obj-390",
            0
          ]
        }
      }
    ],
    "devicewidth": 748.0,
    "openrect": [
      0.0,
      0.0,
      748.0,
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
    "parameters": {
      "obj-19": [
        "PMOR editor window",
        "PMOR win",
        0
      ],
      "obj-26": [
        "FX Distortion algo",
        "Dist algo",
        0
      ],
      "obj-29": [
        "FX1.01 ST4",
        "ST4",
        0
      ],
      "obj-31": [
        "FX2.01 DUB",
        "DUB",
        0
      ],
      "obj-33": [
        "FX3.01 PLATE",
        "PLATE",
        0
      ],
      "obj-35": [
        "FX4.01 OCT+",
        "OCT+",
        0
      ],
      "obj-37": [
        "FX5.01 RPT2",
        "RPT2",
        0
      ],
      "obj-39": [
        "FX6.01 DELAY",
        "DELAY",
        0
      ],
      "obj-41": [
        "FX7.01 GATE4",
        "GATE4",
        0
      ],
      "obj-43": [
        "FX8.01 RAT2",
        "RAT2",
        0
      ],
      "obj-45": [
        "FX1.02 ST8",
        "ST8",
        0
      ],
      "obj-47": [
        "FX2.02 GVERB",
        "GVERB",
        0
      ],
      "obj-49": [
        "FX3.02 SHIMR",
        "SHIMR",
        0
      ],
      "obj-51": [
        "FX4.02 OCT-",
        "OCT-",
        0
      ],
      "obj-53": [
        "FX5.02 RPT6",
        "RPT6",
        0
      ],
      "obj-55": [
        "FX6.02 PNGPG",
        "PNGPG",
        0
      ],
      "obj-57": [
        "FX7.02 GATE8",
        "GATE8",
        0
      ],
      "obj-59": [
        "FX8.02 RAT3",
        "RAT3",
        0
      ],
      "obj-61": [
        "FX1.03 ST16",
        "ST16",
        0
      ],
      "obj-63": [
        "FX2.03 TGATE",
        "TGATE",
        0
      ],
      "obj-65": [
        "FX3.03 PONG",
        "PONG",
        0
      ],
      "obj-67": [
        "FX4.03 DSTRY",
        "DSTRY",
        0
      ],
      "obj-69": [
        "FX5.03 SHUF",
        "SHUF",
        0
      ],
      "obj-71": [
        "FX6.03 FADE",
        "FADE",
        0
      ],
      "obj-73": [
        "FX7.03 GAT16",
        "GAT16",
        0
      ],
      "obj-75": [
        "FX8.03 RAT4",
        "RAT4",
        0
      ],
      "obj-77": [
        "FX1.04 ST32",
        "ST32",
        0
      ],
      "obj-79": [
        "FX2.04 FLANG",
        "FLANG",
        0
      ],
      "obj-81": [
        "FX3.04 SLAP",
        "SLAP",
        0
      ],
      "obj-83": [
        "FX4.04 REPT",
        "REPT",
        0
      ],
      "obj-85": [
        "FX5.04 Q6/8",
        "Q6/8",
        0
      ],
      "obj-87": [
        "FX6.04 FBACK",
        "FBACK",
        0
      ],
      "obj-89": [
        "FX7.04 TRNCE",
        "TRNCE",
        0
      ],
      "obj-91": [
        "FX8.04 RAT8",
        "RAT8",
        0
      ],
      "obj-93": [
        "FX1.05 REV",
        "REV",
        0
      ],
      "obj-95": [
        "FX2.05 RING",
        "RING",
        0
      ],
      "obj-97": [
        "FX3.05 PHASE",
        "PHASE",
        0
      ],
      "obj-99": [
        "FX4.05 RGRN",
        "RGRN",
        0
      ],
      "obj-101": [
        "FX5.05 PUNCH",
        "PUNCH",
        0
      ],
      "obj-103": [
        "FX6.05 DRONE",
        "DRONE",
        0
      ],
      "obj-105": [
        "FX7.05 TREM",
        "TREM",
        0
      ],
      "obj-107": [
        "FX8.05 STRUM",
        "STRUM",
        0
      ],
      "obj-109": [
        "FX1.06 CHOR",
        "CHOR",
        0
      ],
      "obj-111": [
        "FX2.06 FOLD",
        "FOLD",
        0
      ],
      "obj-113": [
        "FX3.06 WAH",
        "WAH",
        0
      ],
      "obj-115": [
        "FX4.06 FREEZ",
        "FREEZ",
        0
      ],
      "obj-117": [
        "FX5.06 FILL",
        "FILL",
        0
      ],
      "obj-119": [
        "FX6.06 FREEZ",
        "FREEZ",
        0
      ],
      "obj-121": [
        "FX7.06 PAN",
        "PAN",
        0
      ],
      "obj-123": [
        "FX8.06 TRNC2",
        "TRNC2",
        0
      ],
      "obj-125": [
        "FX1.07 PAN",
        "PAN",
        0
      ],
      "obj-127": [
        "FX2.07 COMB",
        "COMB",
        0
      ],
      "obj-129": [
        "FX3.07 TREM",
        "TREM",
        0
      ],
      "obj-131": [
        "FX4.07 METAL",
        "METAL",
        0
      ],
      "obj-133": [
        "FX5.07 BUILD",
        "BUILD",
        0
      ],
      "obj-135": [
        "FX6.07 UNI",
        "UNI",
        0
      ],
      "obj-137": [
        "FX7.07 PUMP",
        "PUMP",
        0
      ],
      "obj-139": [
        "FX8.07 STAB",
        "STAB",
        0
      ],
      "obj-141": [
        "FX1.08 CRUSH",
        "CRUSH",
        0
      ],
      "obj-143": [
        "FX2.08 VOWEL",
        "VOWEL",
        0
      ],
      "obj-145": [
        "FX3.08 HAAS",
        "HAAS",
        0
      ],
      "obj-147": [
        "FX4.08 FRMNT",
        "FRMNT",
        0
      ],
      "obj-149": [
        "FX5.08 BALL",
        "BALL",
        0
      ],
      "obj-151": [
        "FX6.08 UNILO",
        "UNILO",
        0
      ],
      "obj-153": [
        "FX7.08 SWELL",
        "SWELL",
        0
      ],
      "obj-155": [
        "FX8.08 RINGL",
        "RINGL",
        0
      ],
      "obj-157": [
        "FX1.09 HALF",
        "HALF",
        0
      ],
      "obj-159": [
        "FX2.09 ISO-LO",
        "ISO-LO",
        0
      ],
      "obj-161": [
        "FX3.09 VIBRA",
        "VIBRA",
        0
      ],
      "obj-163": [
        "FX4.09 FSHFT",
        "FSHFT",
        0
      ],
      "obj-165": [
        "FX5.09 LPF",
        "LPF",
        0
      ],
      "obj-167": [
        "FX6.09 DETUN",
        "DETUN",
        0
      ],
      "obj-169": [
        "FX7.09 NOISE",
        "NOISE",
        0
      ],
      "obj-171": [
        "FX8.09 RINGH",
        "RINGH",
        0
      ],
      "obj-173": [
        "FX1.10 DBL",
        "DBL",
        0
      ],
      "obj-175": [
        "FX2.10 ISO-MD",
        "ISO-MD",
        0
      ],
      "obj-177": [
        "FX3.10 DIMEN",
        "DIMEN",
        0
      ],
      "obj-179": [
        "FX4.10 FUZZ",
        "FUZZ",
        0
      ],
      "obj-181": [
        "FX5.10 HPF",
        "HPF",
        0
      ],
      "obj-183": [
        "FX6.10 PLFO",
        "PLFO",
        0
      ],
      "obj-185": [
        "FX7.10 WIDE",
        "WIDE",
        0
      ],
      "obj-187": [
        "FX8.10 VIBR",
        "VIBR",
        0
      ],
      "obj-189": [
        "FX1.11 BZZ",
        "BZZ",
        0
      ],
      "obj-191": [
        "FX2.11 ISO-HI",
        "ISO-HI",
        0
      ],
      "obj-193": [
        "FX3.11 SPRNG",
        "SPRNG",
        0
      ],
      "obj-195": [
        "FX4.11 GSEQ",
        "GSEQ",
        0
      ],
      "obj-197": [
        "FX5.11 SQUSH",
        "SQUSH",
        0
      ],
      "obj-199": [
        "FX6.11 BND+",
        "BND+",
        0
      ],
      "obj-201": [
        "FX7.11 MONO",
        "MONO",
        0
      ],
      "obj-203": [
        "FX8.11 CHOP",
        "CHOP",
        0
      ],
      "obj-205": [
        "FX1.12 MUTE",
        "MUTE",
        0
      ],
      "obj-207": [
        "FX2.12 PHONE",
        "PHONE",
        0
      ],
      "obj-209": [
        "FX3.12 SIREN",
        "SIREN",
        0
      ],
      "obj-211": [
        "FX4.12 SRAMP",
        "SRAMP",
        0
      ],
      "obj-213": [
        "FX5.12 SCRTC",
        "SCRTC",
        0
      ],
      "obj-215": [
        "FX6.12 BND-",
        "BND-",
        0
      ],
      "obj-217": [
        "FX7.12 BRITE",
        "BRITE",
        0
      ],
      "obj-219": [
        "FX8.12 HALF",
        "HALF",
        0
      ],
      "obj-221": [
        "FX1.13 FLT",
        "FLT",
        0
      ],
      "obj-223": [
        "FX2.13 GRAIN",
        "GRAIN",
        0
      ],
      "obj-225": [
        "FX3.13 WARM",
        "WARM",
        0
      ],
      "obj-227": [
        "FX4.13 VSTOP",
        "VSTOP",
        0
      ],
      "obj-229": [
        "FX5.13 SCRTF",
        "SCRTF",
        0
      ],
      "obj-231": [
        "FX6.13 WARBL",
        "WARBL",
        0
      ],
      "obj-233": [
        "FX7.13 DARK",
        "DARK",
        0
      ],
      "obj-235": [
        "FX8.13 DBL",
        "DBL",
        0
      ],
      "obj-237": [
        "FX1.14 WASH",
        "WASH",
        0
      ],
      "obj-239": [
        "FX2.14 WOW",
        "WOW",
        0
      ],
      "obj-241": [
        "FX3.14 SUB",
        "SUB",
        0
      ],
      "obj-243": [
        "FX4.14 RADIO",
        "RADIO",
        0
      ],
      "obj-245": [
        "FX5.14 GLCH",
        "GLCH",
        0
      ],
      "obj-247": [
        "FX6.14 5TH",
        "5TH",
        0
      ],
      "obj-249": [
        "FX7.14 DRIVE",
        "DRIVE",
        0
      ],
      "obj-251": [
        "FX8.14 GLCH",
        "GLCH",
        0
      ],
      "obj-253": [
        "FX1.15 DIST",
        "DIST",
        0
      ],
      "obj-255": [
        "FX2.15 DOWN",
        "DOWN",
        0
      ],
      "obj-257": [
        "FX3.15 AIR",
        "AIR",
        0
      ],
      "obj-259": [
        "FX4.15 NGATE",
        "NGATE",
        0
      ],
      "obj-261": [
        "FX5.15 BLIND",
        "BLIND",
        0
      ],
      "obj-263": [
        "FX6.15 SUB",
        "SUB",
        0
      ],
      "obj-265": [
        "FX7.15 SGATE",
        "SGATE",
        0
      ],
      "obj-267": [
        "FX8.15 REVG",
        "REVG",
        0
      ],
      "obj-269": [
        "FX1.16 STOP",
        "STOP",
        0
      ],
      "obj-271": [
        "FX2.16 SCAT",
        "SCAT",
        0
      ],
      "obj-273": [
        "FX3.16 PUMP",
        "PUMP",
        0
      ],
      "obj-275": [
        "FX4.16 SHMOD",
        "SHMOD",
        0
      ],
      "obj-277": [
        "FX5.16 S&H",
        "S&H",
        0
      ],
      "obj-279": [
        "FX6.16 LOOP",
        "LOOP",
        0
      ],
      "obj-281": [
        "FX7.16 KILL",
        "KILL",
        0
      ],
      "obj-283": [
        "FX8.16 SIL",
        "SIL",
        0
      ],
      "obj-287": [
        "FX Bank 1 Amount",
        "AMOUNT",
        0
      ],
      "obj-297": [
        "FX Bank 1 Lidar",
        "LIDAR",
        0
      ],
      "obj-301": [
        "FX Bank 2 Amount",
        "AMOUNT",
        0
      ],
      "obj-305": [
        "FX Bank 2 Lidar",
        "LIDAR",
        0
      ],
      "obj-309": [
        "FX Bank 2 Mix",
        "MIX",
        0
      ],
      "obj-313": [
        "FX Bank 3 Amount",
        "AMOUNT",
        0
      ],
      "obj-317": [
        "FX Bank 3 Lidar",
        "LIDAR",
        0
      ],
      "obj-321": [
        "FX Bank 3 Mix",
        "MIX",
        0
      ],
      "obj-325": [
        "FX Bank 4 Amount",
        "AMOUNT",
        0
      ],
      "obj-329": [
        "FX Bank 4 Lidar",
        "LIDAR",
        0
      ],
      "obj-333": [
        "FX Bank 4 Mix",
        "MIX",
        0
      ],
      "obj-337": [
        "FX Bank 5 Amount",
        "AMOUNT",
        0
      ],
      "obj-341": [
        "FX Bank 5 Lidar",
        "LIDAR",
        0
      ],
      "obj-345": [
        "FX Bank 5 Mix",
        "MIX",
        0
      ],
      "obj-349": [
        "FX Bank 6 Amount",
        "AMOUNT",
        0
      ],
      "obj-353": [
        "FX Bank 6 Lidar",
        "LIDAR",
        0
      ],
      "obj-357": [
        "FX Bank 6 Mix",
        "MIX",
        0
      ],
      "obj-361": [
        "FX Bank 7 Amount",
        "AMOUNT",
        0
      ],
      "obj-365": [
        "FX Bank 7 Lidar",
        "LIDAR",
        0
      ],
      "obj-369": [
        "FX Bank 7 Mix",
        "MIX",
        0
      ],
      "obj-373": [
        "FX Bank 8 Amount",
        "AMOUNT",
        0
      ],
      "obj-377": [
        "FX Bank 8 Lidar",
        "LIDAR",
        0
      ],
      "obj-381": [
        "FX Bank 8 Mix",
        "MIX",
        0
      ],
      "inherited_shortname": 1
    },
    "is_mpe": 0,
    "latency": 0,
    "minimum_live_version": "",
    "minimum_max_version": "",
    "platform_compatibility": 0,
    "autosave": 0
  }
}
