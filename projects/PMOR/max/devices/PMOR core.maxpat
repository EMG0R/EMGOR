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
            520,
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
            520,
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
            504,
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
            504,
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
            160,
            4,
            200,
            24
          ],
          "text": "PMOR core",
          "textcolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "fontsize": 14.0,
          "fontname": "Arial",
          "fontface": 1,
          "textjustification": 1,
          "varname": "title",
          "presentation": 1,
          "presentation_rect": [
            160,
            4,
            200,
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
            140,
            16
          ],
          "text": "engine + USB host link",
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
            140,
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
            408,
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
            0.7451,
            0.6118,
            1.0,
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
            408,
            40,
            96,
            112
          ]
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
            16,
            40,
            384,
            16
          ],
          "text": "disconnected",
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
          "varname": "status",
          "presentation": 1,
          "presentation_rect": [
            16,
            40,
            384,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-7",
          "maxclass": "textbutton",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "",
            "",
            "int"
          ],
          "patching_rect": [
            16,
            92,
            384,
            28
          ],
          "text": "CONNECT",
          "texton": "CONNECTED",
          "mode": 1,
          "bgcolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "bgoncolor": [
            0.651,
            0.9098,
            0.3529,
            1.0
          ],
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "usebgoncolor": 1,
          "fontsize": 11.0,
          "fontname": "Arial",
          "rounded": 4,
          "varname": "connect",
          "presentation": 1,
          "presentation_rect": [
            16,
            92,
            384,
            28
          ]
        }
      },
      {
        "box": {
          "id": "obj-8",
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
              776.0,
              288.0
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
                    776,
                    288
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
                    776,
                    288
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
                    760,
                    208
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
                    760,
                    208
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
                    288,
                    4,
                    200,
                    24
                  ],
                  "text": "PMOR core",
                  "textcolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "fontsize": 14.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 1,
                  "varname": "title",
                  "presentation": 1,
                  "presentation_rect": [
                    288,
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
                    744,
                    28
                  ],
                  "tabs": [
                    "LINK",
                    "SAMPLES",
                    "PRESETS",
                    "GLOBAL",
                    "BIN"
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
                    744,
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
                  "text": "js pmor_bank.js gated core",
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
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_ui_status"
                }
              },
              {
                "box": {
                  "id": "obj-18",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1152,
                    180,
                    22
                  ],
                  "text": "route st"
                }
              },
              {
                "box": {
                  "id": "obj-19",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 8,
                  "outlettype": [
                    "",
                    "",
                    "",
                    "",
                    "",
                    "",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1180,
                    180,
                    22
                  ],
                  "text": "js pmor_browser.js 0 gated",
                  "varname": "br"
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
                    40,
                    744,
                    16
                  ],
                  "text": "LINK: connection, pull progress, transport",
                  "textcolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh0_1",
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    80,
                    744,
                    16
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-21",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    76,
                    40,
                    16
                  ],
                  "text": "mode",
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
                    16,
                    116,
                    40,
                    16
                  ],
                  "varname": "bh0_x1"
                }
              },
              {
                "box": {
                  "id": "obj-22",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    64,
                    72,
                    96,
                    24
                  ],
                  "items": [
                    "STUDIO",
                    ",",
                    "EDIT"
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
                  "varname": "mode",
                  "presentation": 1,
                  "presentation_rect": [
                    64,
                    112,
                    96,
                    24
                  ]
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
                    176,
                    76,
                    40,
                    16
                  ],
                  "text": "port",
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
                    176,
                    116,
                    40,
                    16
                  ],
                  "varname": "bh0_x2"
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
                    224,
                    72,
                    96,
                    24
                  ],
                  "items": [
                    "auto",
                    ",",
                    "a",
                    ",",
                    "b",
                    ",",
                    "c",
                    ",",
                    "d",
                    ",",
                    "e",
                    ",",
                    "f"
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
                  "varname": "port",
                  "presentation": 1,
                  "presentation_rect": [
                    224,
                    112,
                    96,
                    24
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-25",
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
                  "text": "send pmor_lnk_in"
                }
              },
              {
                "box": {
                  "id": "obj-26",
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
                  "text": "prepend pmode"
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
                    1264,
                    180,
                    22
                  ],
                  "text": "prepend port"
                }
              },
              {
                "box": {
                  "id": "obj-28",
                  "maxclass": "slider",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    16,
                    112,
                    744,
                    8
                  ],
                  "size": 101.0,
                  "min": 0.0,
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "knobcolor": [
                    0.7451,
                    0.6118,
                    1.0,
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
                  "varname": "progress",
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    152,
                    744,
                    8
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-29",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    124,
                    744,
                    16
                  ],
                  "text": "samples: idle",
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
                  "varname": "samples",
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    164,
                    744,
                    16
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
                    144,
                    744,
                    16
                  ],
                  "text": "stop bpm - clk - cpu - ovr - drop -",
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
                  "varname": "transport",
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    184,
                    744,
                    16
                  ]
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
                    16,
                    164,
                    744,
                    16
                  ],
                  "text": "join -",
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
                  "varname": "joinline",
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    204,
                    744,
                    16
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-32",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_lnk_mode"
                }
              },
              {
                "box": {
                  "id": "obj-33",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_lnk_progress"
                }
              },
              {
                "box": {
                  "id": "obj-34",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_lnk_samples"
                }
              },
              {
                "box": {
                  "id": "obj-35",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_lnk_transport"
                }
              },
              {
                "box": {
                  "id": "obj-36",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_lnk_joinline"
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
                    40,
                    360,
                    16
                  ],
                  "text": "SAMPLE POOL",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    208,
                    80,
                    360,
                    16
                  ],
                  "varname": "bh1_x3",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-38",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    16,
                    64,
                    288,
                    24
                  ],
                  "items": [
                    "(empty pool)"
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
                  "varname": "pool",
                  "presentation": 1,
                  "presentation_rect": [
                    208,
                    104,
                    288,
                    24
                  ],
                  "hidden": 1
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
                    210,
                    760,
                    180,
                    22
                  ],
                  "text": "prepend poolpick"
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
                    16,
                    100,
                    288,
                    16
                  ],
                  "text": "[x] marked  * cached  - not  ! bad  xN = same audio",
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
                    208,
                    140,
                    288,
                    16
                  ],
                  "varname": "bh1_x4",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-41",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    312,
                    64,
                    64,
                    24
                  ],
                  "text": "mark",
                  "texton": "mark",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "textoncolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "bgcolor": [
                    0.2275,
                    0.1804,
                    0.451,
                    1.0
                  ],
                  "bgoncolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    504,
                    104,
                    64,
                    24
                  ],
                  "varname": "bh1_x5",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-42",
                  "maxclass": "message",
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
                  "text": "selcur"
                }
              },
              {
                "box": {
                  "id": "obj-43",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    312,
                    96,
                    64,
                    24
                  ],
                  "text": "copies",
                  "texton": "copies",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "textoncolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "bgcolor": [
                    0.2275,
                    0.1804,
                    0.451,
                    1.0
                  ],
                  "bgoncolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    504,
                    136,
                    64,
                    24
                  ],
                  "varname": "bh1_x6",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-44",
                  "maxclass": "message",
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
                  "text": "expandcur"
                }
              },
              {
                "box": {
                  "id": "obj-45",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    16,
                    128,
                    84,
                    24
                  ],
                  "text": "pull",
                  "texton": "pull",
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
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "bgoncolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    208,
                    168,
                    84,
                    24
                  ],
                  "varname": "bh1_x7",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-46",
                  "maxclass": "message",
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
                  "text": "pull"
                }
              },
              {
                "box": {
                  "id": "obj-47",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    108,
                    128,
                    84,
                    24
                  ],
                  "text": "pull patch",
                  "texton": "pull patch",
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
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "bgoncolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    300,
                    168,
                    84,
                    24
                  ],
                  "varname": "bh1_x8",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-48",
                  "maxclass": "message",
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
                  "text": "pull current"
                }
              },
              {
                "box": {
                  "id": "obj-49",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    200,
                    128,
                    84,
                    24
                  ],
                  "text": "pull all",
                  "texton": "pull all",
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
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "bgoncolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    392,
                    168,
                    84,
                    24
                  ],
                  "varname": "bh1_x9",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-50",
                  "maxclass": "message",
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
                  "text": "pull all"
                }
              },
              {
                "box": {
                  "id": "obj-51",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    292,
                    128,
                    84,
                    24
                  ],
                  "text": "pull off",
                  "texton": "pull off",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "textoncolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "bgcolor": [
                    0.2275,
                    0.1804,
                    0.451,
                    1.0
                  ],
                  "bgoncolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    484,
                    168,
                    84,
                    24
                  ],
                  "varname": "bh1_x10",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-52",
                  "maxclass": "message",
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
                  "text": "pull off"
                }
              },
              {
                "box": {
                  "id": "obj-53",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    16,
                    160,
                    84,
                    24
                  ],
                  "text": "refresh pool",
                  "texton": "refresh pool",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "textoncolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "bgcolor": [
                    0.2275,
                    0.1804,
                    0.451,
                    1.0
                  ],
                  "bgoncolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    208,
                    200,
                    84,
                    24
                  ],
                  "varname": "bh1_x11",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-54",
                  "maxclass": "message",
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
                  "text": "pool"
                }
              },
              {
                "box": {
                  "id": "obj-55",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    108,
                    160,
                    84,
                    24
                  ],
                  "text": "clear marks",
                  "texton": "clear marks",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "textoncolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "bgcolor": [
                    0.2275,
                    0.1804,
                    0.451,
                    1.0
                  ],
                  "bgoncolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    300,
                    200,
                    84,
                    24
                  ],
                  "varname": "bh1_x12",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-56",
                  "maxclass": "message",
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
                  "text": "selclear"
                }
              },
              {
                "box": {
                  "id": "obj-57",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    200,
                    160,
                    176,
                    24
                  ],
                  "text": "HOLD 2 s: delete marked",
                  "texton": "HOLD 2 s: delete marked",
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
                    1.0,
                    0.4784,
                    0.5412,
                    1.0
                  ],
                  "bgoncolor": [
                    1.0,
                    0.8471,
                    0.302,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "mode": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    392,
                    200,
                    176,
                    24
                  ],
                  "varname": "bh1_x13",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-58",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "bang",
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1012,
                    180,
                    22
                  ],
                  "text": "sel 1 0"
                }
              },
              {
                "box": {
                  "id": "obj-59",
                  "maxclass": "message",
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
                  "text": "delarm"
                }
              },
              {
                "box": {
                  "id": "obj-60",
                  "maxclass": "message",
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
                  "text": "delgo"
                }
              },
              {
                "box": {
                  "id": "obj-61",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    188,
                    360,
                    32
                  ],
                  "text": "pool: idle",
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
                  "varname": "poolinfo",
                  "presentation": 1,
                  "presentation_rect": [
                    208,
                    228,
                    360,
                    32
                  ],
                  "hidden": 1
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
                    400,
                    40,
                    360,
                    16
                  ],
                  "text": "PRESETS",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    208,
                    80,
                    360,
                    16
                  ],
                  "varname": "bh2_x14",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-63",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    400,
                    64,
                    160,
                    24
                  ],
                  "items": [
                    "KICK",
                    ",",
                    "SNARE",
                    ",",
                    "HAT",
                    ",",
                    "PERC",
                    ",",
                    "syn THIEF",
                    ",",
                    "syn FM",
                    ",",
                    "syn WAVE",
                    ",",
                    "syn PLUCK",
                    ",",
                    "syn SUPER",
                    ",",
                    "syn CZ",
                    ",",
                    "syn FOLD",
                    ",",
                    "syn ADD"
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
                  "varname": "engine",
                  "presentation": 1,
                  "presentation_rect": [
                    208,
                    104,
                    160,
                    24
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-64",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    568,
                    64,
                    88,
                    24
                  ],
                  "items": [
                    "pad 0",
                    ",",
                    "pad 1",
                    ",",
                    "pad 2",
                    ",",
                    "pad 3",
                    ",",
                    "pad 4",
                    ",",
                    "pad 5",
                    ",",
                    "pad 6",
                    ",",
                    "pad 7",
                    ",",
                    "pad 8",
                    ",",
                    "pad 9",
                    ",",
                    "pad 10",
                    ",",
                    "pad 11",
                    ",",
                    "pad 12",
                    ",",
                    "pad 13",
                    ",",
                    "pad 14",
                    ",",
                    "pad 15"
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
                  "varname": "pad",
                  "presentation": 1,
                  "presentation_rect": [
                    376,
                    104,
                    88,
                    24
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-65",
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
                  "text": "prepend engine"
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
                    1124,
                    180,
                    22
                  ],
                  "text": "prepend padsel"
                }
              },
              {
                "box": {
                  "id": "obj-67",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    664,
                    68,
                    96,
                    16
                  ],
                  "text": "pad: drums only",
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
                    472,
                    108,
                    96,
                    16
                  ],
                  "varname": "bh2_x15",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-68",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    400,
                    96,
                    264,
                    24
                  ],
                  "items": [],
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
                  "varname": "presets",
                  "presentation": 1,
                  "presentation_rect": [
                    208,
                    136,
                    264,
                    24
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-69",
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
                  "text": "prepend presetpick"
                }
              },
              {
                "box": {
                  "id": "obj-70",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    672,
                    96,
                    88,
                    24
                  ],
                  "text": "preset",
                  "texton": "preset",
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
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "bgoncolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    480,
                    136,
                    88,
                    24
                  ],
                  "varname": "bh2_x16",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-71",
                  "maxclass": "message",
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
                  "text": "dopreset"
                }
              },
              {
                "box": {
                  "id": "obj-72",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    400,
                    128,
                    112,
                    24
                  ],
                  "text": "reload presets",
                  "texton": "reload presets",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "textoncolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "bgcolor": [
                    0.2275,
                    0.1804,
                    0.451,
                    1.0
                  ],
                  "bgoncolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    208,
                    168,
                    112,
                    24
                  ],
                  "varname": "bh2_x17",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-73",
                  "maxclass": "message",
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
                  "text": "presets"
                }
              },
              {
                "box": {
                  "id": "obj-74",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    520,
                    128,
                    88,
                    24
                  ],
                  "text": "resync",
                  "texton": "resync",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "textoncolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "bgcolor": [
                    0.2275,
                    0.1804,
                    0.451,
                    1.0
                  ],
                  "bgoncolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    328,
                    168,
                    88,
                    24
                  ],
                  "varname": "bh2_x18",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-75",
                  "maxclass": "message",
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
                  "text": "getpatch"
                }
              },
              {
                "box": {
                  "id": "obj-76",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    616,
                    132,
                    144,
                    16
                  ],
                  "text": "f = factory   u = user",
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
                    424,
                    172,
                    144,
                    16
                  ],
                  "varname": "bh2_x19",
                  "hidden": 1
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
                    400,
                    164,
                    240,
                    16
                  ],
                  "text": "patch -  store -",
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
                  "varname": "store",
                  "presentation": 1,
                  "presentation_rect": [
                    208,
                    204,
                    240,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-78",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    648,
                    160,
                    112,
                    24
                  ],
                  "text": "store retry",
                  "texton": "store retry",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "textoncolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "bgcolor": [
                    0.2275,
                    0.1804,
                    0.451,
                    1.0
                  ],
                  "bgoncolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    456,
                    200,
                    112,
                    24
                  ],
                  "varname": "bh2_x20",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-79",
                  "maxclass": "message",
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
                  "text": "storeretry"
                }
              },
              {
                "box": {
                  "id": "obj-80",
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
                    210,
                    1292,
                    180,
                    22
                  ],
                  "text": "route poolinfo store trans"
                }
              },
              {
                "box": {
                  "id": "obj-81",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    240,
                    200,
                    16
                  ],
                  "text": "GLOBAL",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    80,
                    200,
                    16
                  ],
                  "varname": "bh3_x21",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-82",
                  "maxclass": "flonum",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "",
                    "bang"
                  ],
                  "patching_rect": [
                    20,
                    272,
                    72,
                    24
                  ],
                  "bgcolor": [
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
                  "bordercolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "tricolor": [
                    0.7882,
                    0.7373,
                    0.9412,
                    1.0
                  ],
                  "ignoreclick": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "varname": "p6144",
                  "presentation": 1,
                  "presentation_rect": [
                    20,
                    112,
                    72,
                    24
                  ],
                  "hidden": 1
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
                    16,
                    308,
                    80,
                    16
                  ],
                  "text": "BPM (follow)",
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
                    148,
                    80,
                    16
                  ],
                  "varname": "bh3_x22",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-84",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    100,
                    272,
                    72,
                    24
                  ],
                  "items": [
                    "IONIAN",
                    ",",
                    "DORIAN",
                    ",",
                    "PHRYG",
                    ",",
                    "LYDIAN",
                    ",",
                    "MIXO",
                    ",",
                    "AEOL",
                    ",",
                    "LOCR",
                    ",",
                    "HARMIN",
                    ",",
                    "PENTA"
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
                  "varname": "p6149",
                  "presentation": 1,
                  "presentation_rect": [
                    100,
                    112,
                    72,
                    24
                  ],
                  "hidden": 1
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
                    1320,
                    180,
                    22
                  ],
                  "text": "prepend setparam 6149"
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
                    96,
                    308,
                    80,
                    16
                  ],
                  "text": "SCALE",
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
                    96,
                    148,
                    80,
                    16
                  ],
                  "varname": "bh3_x23",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-87",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    196,
                    264,
                    40,
                    40
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "p6148",
                  "min": -12,
                  "size": 1001,
                  "mult": 0.024,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    196,
                    104,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    1348,
                    180,
                    22
                  ],
                  "text": "prepend setparam 6148"
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
                    176,
                    308,
                    80,
                    16
                  ],
                  "text": "KEY",
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
                    176,
                    148,
                    80,
                    16
                  ],
                  "varname": "bh3_x24",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-90",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    276,
                    264,
                    40,
                    40
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "p6146",
                  "min": 0,
                  "size": 1001,
                  "mult": 0.0013333,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    276,
                    104,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    1376,
                    180,
                    22
                  ],
                  "text": "prepend setparam 6146"
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
                    256,
                    308,
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
                  "textjustification": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    256,
                    148,
                    80,
                    16
                  ],
                  "varname": "bh3_x25",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-93",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    356,
                    264,
                    40,
                    40
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "p6150",
                  "min": 0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    356,
                    104,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    1404,
                    180,
                    22
                  ],
                  "text": "prepend setparam 6150"
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
                    336,
                    308,
                    80,
                    16
                  ],
                  "text": "METRO",
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
                    336,
                    148,
                    80,
                    16
                  ],
                  "varname": "bh3_x26",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-96",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    436,
                    264,
                    40,
                    40
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "p6147",
                  "min": 0,
                  "size": 1001,
                  "mult": 0.002,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    436,
                    104,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    400,
                    760,
                    180,
                    22
                  ],
                  "text": "prepend setparam 6147"
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
                    416,
                    308,
                    80,
                    16
                  ],
                  "text": "INGAIN",
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
                    416,
                    148,
                    80,
                    16
                  ],
                  "varname": "bh3_x27",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-99",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    516,
                    264,
                    40,
                    40
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "p6145",
                  "min": 0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    516,
                    104,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    788,
                    180,
                    22
                  ],
                  "text": "prepend setparam 6145"
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
                    496,
                    308,
                    80,
                    16
                  ],
                  "text": "SWING",
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
                    496,
                    148,
                    80,
                    16
                  ],
                  "varname": "bh3_x28",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-102",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    20,
                    324,
                    40,
                    40
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_clklead",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    20,
                    164,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    400,
                    816,
                    180,
                    22
                  ],
                  "text": "prepend bc clklead"
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
                    72,
                    336,
                    340,
                    16
                  ],
                  "text": "clock lead (ms): + plays earlier, to match Live's click",
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
                  "varname": "clkleadtxt",
                  "presentation": 1,
                  "presentation_rect": [
                    72,
                    176,
                    340,
                    16
                  ],
                  "hidden": 1
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
                    416,
                    336,
                    80,
                    16
                  ],
                  "text": "-",
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
                  "varname": "bv_clklead",
                  "presentation": 1,
                  "presentation_rect": [
                    416,
                    176,
                    80,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-106",
                  "maxclass": "panel",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    580,
                    244,
                    1,
                    120
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
                    580,
                    84,
                    1,
                    120
                  ],
                  "varname": "bh3_x29",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-107",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    588,
                    240,
                    172,
                    16
                  ],
                  "text": "UNKNOWN TO THIS DEVICE",
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
                    588,
                    80,
                    172,
                    16
                  ],
                  "varname": "bh3_x30",
                  "hidden": 1
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
                    588,
                    256,
                    172,
                    16
                  ],
                  "text": "",
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
                  "varname": "unknown",
                  "presentation": 1,
                  "presentation_rect": [
                    588,
                    96,
                    172,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-109",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    588,
                    276,
                    172,
                    24
                  ],
                  "items": [
                    "(none)"
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
                  "varname": "unk",
                  "presentation": 1,
                  "presentation_rect": [
                    588,
                    116,
                    172,
                    24
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-110",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    588,
                    304,
                    172,
                    16
                  ],
                  "text": "fw - proto -",
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
                  "varname": "ver",
                  "presentation": 1,
                  "presentation_rect": [
                    588,
                    144,
                    172,
                    16
                  ],
                  "hidden": 1
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
                    16,
                    392,
                    744,
                    16
                  ],
                  "text": "MIX volumes and the SEQUENCER params live in the lane editors (drums / synth / sampler): one place per job",
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
                    232,
                    744,
                    16
                  ],
                  "varname": "bh3_x31",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-112",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    432,
                    200,
                    16
                  ],
                  "text": "RECYCLE BIN",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    80,
                    200,
                    16
                  ],
                  "varname": "bh4_x32",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-113",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    16,
                    452,
                    352,
                    24
                  ],
                  "items": [
                    "(bin: refresh)"
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
                  "varname": "bin",
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    100,
                    352,
                    24
                  ],
                  "hidden": 1
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
                    844,
                    180,
                    22
                  ],
                  "text": "prepend binsel"
                }
              },
              {
                "box": {
                  "id": "obj-115",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    376,
                    452,
                    84,
                    24
                  ],
                  "text": "refresh",
                  "texton": "refresh",
                  "textcolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "textoncolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "bgcolor": [
                    0.2275,
                    0.1804,
                    0.451,
                    1.0
                  ],
                  "bgoncolor": [
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    376,
                    100,
                    84,
                    24
                  ],
                  "varname": "bh4_x33",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-116",
                  "maxclass": "message",
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
                  "text": "bin"
                }
              },
              {
                "box": {
                  "id": "obj-117",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    468,
                    452,
                    84,
                    24
                  ],
                  "text": "restore",
                  "texton": "restore",
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
                    0.7451,
                    0.6118,
                    1.0,
                    1.0
                  ],
                  "bgoncolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "presentation": 1,
                  "presentation_rect": [
                    468,
                    100,
                    84,
                    24
                  ],
                  "varname": "bh4_x34",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-118",
                  "maxclass": "message",
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
                  "text": "binrestore"
                }
              },
              {
                "box": {
                  "id": "obj-119",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    560,
                    452,
                    200,
                    24
                  ],
                  "text": "HOLD 2 s: empty bin",
                  "texton": "HOLD 2 s: empty bin",
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
                    1.0,
                    0.4784,
                    0.5412,
                    1.0
                  ],
                  "bgoncolor": [
                    1.0,
                    0.8471,
                    0.302,
                    1.0
                  ],
                  "usebgoncolor": 1,
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "fontface": 0,
                  "rounded": 4,
                  "mode": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    560,
                    100,
                    200,
                    24
                  ],
                  "varname": "bh4_x35",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-120",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "bang",
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    400,
                    928,
                    180,
                    22
                  ],
                  "text": "sel 1 0"
                }
              },
              {
                "box": {
                  "id": "obj-121",
                  "maxclass": "message",
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
                  "text": "binemptyarm"
                }
              },
              {
                "box": {
                  "id": "obj-122",
                  "maxclass": "message",
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
                  "text": "binemptygo"
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
                    16,
                    484,
                    744,
                    16
                  ],
                  "text": "deleted pool samples land here first: name, deleted when, boots left (auto-purge after 25 reboots). restore returns it to the pool.",
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
                    132,
                    744,
                    16
                  ],
                  "varname": "bh4_x36",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-124",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_rx"
                }
              },
              {
                "box": {
                  "id": "obj-125",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1040,
                    180,
                    22
                  ],
                  "text": "js pmor_ui.js gated",
                  "varname": "ui"
                }
              },
              {
                "box": {
                  "id": "obj-126",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_ui_status"
                }
              },
              {
                "box": {
                  "id": "obj-127",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "",
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1096,
                    180,
                    22
                  ],
                  "text": "route st"
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
                    "obj-19",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-19",
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
                    "obj-22",
                    1
                  ],
                  "destination": [
                    "obj-26",
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
                    "obj-25",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-24",
                    1
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
                    "obj-25",
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
                    "obj-22",
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
                    "obj-28",
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
                    "obj-29",
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
                    "obj-30",
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
                    "obj-31",
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
                    "obj-19",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-19",
                    2
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
                    "obj-19",
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
                    "obj-19",
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
                    "obj-19",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-19",
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
                    "obj-59",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-58",
                    1
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
                    "obj-59",
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
                    "obj-60",
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
                    "obj-63",
                    0
                  ],
                  "destination": [
                    "obj-65",
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
                    "obj-19",
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
                    "obj-19",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-19",
                    4
                  ],
                  "destination": [
                    "obj-63",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-19",
                    5
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
                    "obj-68",
                    0
                  ],
                  "destination": [
                    "obj-69",
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
                    "obj-68",
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
                    "obj-71",
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
                    "obj-19",
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
                    "obj-8",
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
                    "obj-75",
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
                    "obj-8",
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
                    "obj-8",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-19",
                    3
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
                    "obj-61",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-80",
                    1
                  ],
                  "destination": [
                    "obj-77",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-7",
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
                    "obj-19",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-19",
                    6
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
                    "obj-19",
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
                    "obj-19",
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
                    "obj-121",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-120",
                    1
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
                    "obj-121",
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
                    "obj-122",
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
                    "obj-8",
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
                    "obj-127",
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
                    "obj-125",
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
          "id": "obj-9",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            "bang"
          ],
          "patching_rect": [
            20,
            816,
            180,
            22
          ],
          "text": "loadbang"
        }
      },
      {
        "box": {
          "id": "obj-10",
          "maxclass": "message",
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
          "text": "cachedir ~/Music/PMOR_Cache"
        }
      },
      {
        "box": {
          "id": "obj-11",
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
          "text": "js pmor_port.js"
        }
      },
      {
        "box": {
          "id": "obj-12",
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
            900,
            180,
            22
          ],
          "text": "pmor.proto",
          "varname": "proto"
        }
      },
      {
        "box": {
          "id": "obj-13",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "signal",
            "signal"
          ],
          "patching_rect": [
            20,
            928,
            180,
            22
          ],
          "text": "pmor.core~",
          "varname": "core"
        }
      },
      {
        "box": {
          "id": "obj-14",
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
          "text": "plugout~ 1"
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
            984,
            180,
            22
          ],
          "text": "plugout~ 2"
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
            1012,
            180,
            22
          ],
          "text": "receive pmor_midi"
        }
      },
      {
        "box": {
          "id": "obj-17",
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
          "text": "prepend midi"
        }
      },
      {
        "box": {
          "id": "obj-18",
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
          "text": "js pmor_where.js"
        }
      },
      {
        "box": {
          "id": "obj-19",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 5,
          "outlettype": [
            "",
            "",
            "",
            "",
            ""
          ],
          "patching_rect": [
            20,
            1096,
            180,
            22
          ],
          "text": "js pmor_session.js"
        }
      },
      {
        "box": {
          "id": "obj-20",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            20,
            1124,
            180,
            22
          ],
          "text": "js pmor_join.js"
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
            1152,
            180,
            22
          ],
          "text": "prepend connect"
        }
      },
      {
        "box": {
          "id": "obj-22",
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
            1180,
            180,
            22
          ],
          "text": "route sample progress"
        }
      },
      {
        "box": {
          "id": "obj-23",
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
          "text": "prepend set samples:"
        }
      },
      {
        "box": {
          "id": "obj-24",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 4,
          "outlettype": [
            "",
            "int",
            "int",
            "int"
          ],
          "patching_rect": [
            20,
            1236,
            180,
            22
          ],
          "text": "unpack s 0 0 0"
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
            1264,
            180,
            22
          ],
          "text": "expr 100.*$f1/max(1.,$f2)"
        }
      },
      {
        "box": {
          "id": "obj-26",
          "maxclass": "newobj",
          "numinlets": 1,
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
          "text": "receive pmor_tx"
        }
      },
      {
        "box": {
          "id": "obj-27",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            20,
            1320,
            180,
            22
          ],
          "text": "send pmor_rx"
        }
      },
      {
        "box": {
          "id": "obj-28",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            20,
            1348,
            180,
            22
          ],
          "text": "send pmor_ui_status"
        }
      },
      {
        "box": {
          "id": "obj-29",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 8,
          "outlettype": [
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
          ],
          "patching_rect": [
            20,
            1376,
            180,
            22
          ],
          "text": "js pmor_browser.js"
        }
      },
      {
        "box": {
          "id": "obj-30",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            20,
            1404,
            180,
            22
          ],
          "text": "route trans"
        }
      },
      {
        "box": {
          "id": "obj-31",
          "maxclass": "newobj",
          "numinlets": 1,
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
          "text": "receive pmor_lnk_in"
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
            210,
            788,
            180,
            22
          ],
          "text": "send pmor_lnk_mode"
        }
      },
      {
        "box": {
          "id": "obj-33",
          "maxclass": "newobj",
          "numinlets": 1,
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
          "text": "js pmor_tempo.js"
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
            210,
            844,
            180,
            22
          ],
          "text": "prepend st"
        }
      },
      {
        "box": {
          "id": "obj-35",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            210,
            872,
            180,
            22
          ],
          "text": "send pmor_lnk_samples"
        }
      },
      {
        "box": {
          "id": "obj-36",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            210,
            900,
            180,
            22
          ],
          "text": "send pmor_lnk_progress"
        }
      },
      {
        "box": {
          "id": "obj-37",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            210,
            928,
            180,
            22
          ],
          "text": "send pmor_lnk_transport"
        }
      },
      {
        "box": {
          "id": "obj-38",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            210,
            956,
            180,
            22
          ],
          "text": "send pmor_lnk_joinline"
        }
      },
      {
        "box": {
          "id": "obj-39",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "bang",
            "bang",
            ""
          ],
          "patching_rect": [
            210,
            984,
            180,
            22
          ],
          "text": "sel 1 0"
        }
      },
      {
        "box": {
          "id": "obj-40",
          "maxclass": "message",
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
          "text": "show"
        }
      },
      {
        "box": {
          "id": "obj-41",
          "maxclass": "message",
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
          "text": "hide"
        }
      },
      {
        "box": {
          "id": "obj-42",
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
            210,
            1068,
            180,
            22
          ],
          "text": "js pmor_edit.js full",
          "varname": "edit"
        }
      },
      {
        "box": {
          "id": "obj-43",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            210,
            1096,
            180,
            22
          ],
          "text": "js pmor_guard.js",
          "varname": "guard"
        }
      },
      {
        "box": {
          "id": "obj-44",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            210,
            1124,
            180,
            22
          ],
          "text": "send pmor_tx"
        }
      },
      {
        "box": {
          "id": "obj-45",
          "maxclass": "newobj",
          "numinlets": 1,
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
          "text": "receive pmor_rx"
        }
      },
      {
        "box": {
          "id": "obj-46",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            210,
            1180,
            180,
            22
          ],
          "text": "route param"
        }
      },
      {
        "box": {
          "id": "obj-47",
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
          "text": "prepend param"
        }
      },
      {
        "box": {
          "id": "obj-48",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "",
            "",
            ""
          ],
          "patching_rect": [
            210,
            1236,
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
          "id": "obj-49",
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
          "text": "prepend rectload"
        }
      },
      {
        "box": {
          "id": "obj-50",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            16,
            56,
            384,
            16
          ],
          "text": "set: no copy yet",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "fontsize": 10.0,
          "fontname": "Arial",
          "fontface": 0,
          "textjustification": 0,
          "varname": "snapline",
          "presentation": 1,
          "presentation_rect": [
            16,
            56,
            384,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-51",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            16,
            72,
            384,
            16
          ],
          "text": "",
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
          "varname": "snaphint",
          "presentation": 1,
          "presentation_rect": [
            16,
            72,
            384,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-52",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "",
            "",
            ""
          ],
          "patching_rect": [
            210,
            1292,
            180,
            22
          ],
          "text": "js pmor_snap.js",
          "varname": "snap"
        }
      },
      {
        "box": {
          "id": "obj-53",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            210,
            1320,
            180,
            22
          ],
          "text": "route snapshot"
        }
      },
      {
        "box": {
          "id": "obj-54",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "",
            "",
            ""
          ],
          "patching_rect": [
            210,
            1348,
            180,
            22
          ],
          "text": "pattr pmor_snapshot @parameter_enable 1",
          "varname": "snapstore",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "PMOR set snapshot",
              "parameter_shortname": "PMOR snap",
              "parameter_type": 3,
              "parameter_invisible": 1
            }
          }
        }
      },
      {
        "box": {
          "id": "obj-55",
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
          "text": "prepend snapload"
        }
      },
      {
        "box": {
          "id": "obj-56",
          "maxclass": "textbutton",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "",
            "",
            "int"
          ],
          "patching_rect": [
            16,
            128,
            120,
            24
          ],
          "text": "adopt PMOR's",
          "texton": "adopt PMOR's",
          "textcolor": [
            0.9529,
            0.9333,
            1.0,
            1.0
          ],
          "textoncolor": [
            0.051,
            0.0353,
            0.098,
            1.0
          ],
          "bgcolor": [
            0.2275,
            0.1804,
            0.451,
            1.0
          ],
          "bgoncolor": [
            0.7451,
            0.6118,
            1.0,
            1.0
          ],
          "usebgoncolor": 1,
          "fontsize": 11.0,
          "fontname": "Arial",
          "fontface": 0,
          "rounded": 4,
          "hidden": 1,
          "presentation": 1,
          "presentation_rect": [
            16,
            128,
            120,
            24
          ],
          "varname": "snap_adopt"
        }
      },
      {
        "box": {
          "id": "obj-57",
          "maxclass": "message",
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
          "text": "adopt"
        }
      },
      {
        "box": {
          "id": "obj-58",
          "maxclass": "textbutton",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "",
            "",
            "int"
          ],
          "patching_rect": [
            144,
            128,
            168,
            24
          ],
          "text": "HOLD 2 s: load on PMOR",
          "texton": "HOLD 2 s: load on PMOR",
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
            1.0,
            0.4784,
            0.5412,
            1.0
          ],
          "bgoncolor": [
            1.0,
            0.8471,
            0.302,
            1.0
          ],
          "usebgoncolor": 1,
          "fontsize": 11.0,
          "fontname": "Arial",
          "fontface": 0,
          "rounded": 4,
          "mode": 0,
          "presentation": 1,
          "presentation_rect": [
            144,
            128,
            168,
            24
          ],
          "varname": "snap_write",
          "hidden": 1
        }
      },
      {
        "box": {
          "id": "obj-59",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "bang",
            "bang",
            ""
          ],
          "patching_rect": [
            400,
            760,
            180,
            22
          ],
          "text": "sel 1 0"
        }
      },
      {
        "box": {
          "id": "obj-60",
          "maxclass": "message",
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
          "text": "writearm"
        }
      },
      {
        "box": {
          "id": "obj-61",
          "maxclass": "message",
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
          "text": "writego"
        }
      }
    ],
    "lines": [
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
            "obj-10",
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
            "obj-18",
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
            1
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
            1
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
            "obj-17",
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
            "obj-26",
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
            1
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
            "obj-7",
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
            "obj-19",
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
            "obj-20",
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
            "obj-11",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-19",
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
            "obj-19",
            2
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
            "obj-19",
            3
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
            "obj-19",
            4
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
            "obj-7",
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
            "obj-12",
            2
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
            "obj-12",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-12",
            2
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
            "obj-12",
            2
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
            "obj-12",
            2
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
            "obj-28",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-12",
            2
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
            "obj-12",
            2
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
            "obj-23",
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
            "obj-35",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-22",
            1
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
            1
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
            "obj-24",
            2
          ],
          "destination": [
            "obj-25",
            1
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
            "obj-36",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-29",
            3
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
            "obj-37",
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
            "obj-12",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-20",
            1
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
            "obj-5",
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
            "obj-40",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-39",
            1
          ],
          "destination": [
            "obj-41",
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
            "obj-42",
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
            "obj-47",
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
            "obj-43",
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
            "obj-43",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-42",
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
            "obj-42",
            3
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
            "obj-49",
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
            "obj-42",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-42",
            1
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
            "obj-12",
            2
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
            "obj-12",
            2
          ],
          "destination": [
            "obj-53",
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
            "obj-55",
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
            "obj-12",
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
            "obj-57",
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
            "obj-52",
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
            "obj-59",
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
            "obj-59",
            1
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
            "obj-60",
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
            "obj-61",
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
            "obj-50",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "obj-52",
            1
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
            "obj-52",
            2
          ],
          "destination": [
            "obj-12",
            0
          ]
        }
      }
    ],
    "devicewidth": 520.0,
    "openrect": [
      0.0,
      0.0,
      520.0,
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
      "amxdtype": 1768515945,
      "readonly": 0,
      "devpathtype": 0,
      "devpath": ".",
      "sortmode": 0,
      "viewmode": 0,
      "includepackages": 0
    },
    "dependency_cache": [],
    "parameters": {
      "obj-48": [
        "PMOR editor window",
        "PMOR win",
        0
      ],
      "obj-54": [
        "PMOR set snapshot",
        "PMOR snap",
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
