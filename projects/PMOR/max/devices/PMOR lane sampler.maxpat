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
            500,
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
            500,
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
            484,
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
            484,
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
            148,
            4,
            204,
            24
          ],
          "text": "PMOR sampler",
          "textcolor": [
            1.0,
            0.6588,
            0.4,
            1.0
          ],
          "fontsize": 14.0,
          "fontname": "Arial",
          "fontface": 1,
          "textjustification": 1,
          "varname": "title",
          "presentation": 1,
          "presentation_rect": [
            148,
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
          "text": "any ch -> ch 3 -> PMOR core",
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
            388,
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
            1.0,
            0.6588,
            0.4,
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
            388,
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
              364.0
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
                    364
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
                    364
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
                    284
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
                    284
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
                  "text": "PMOR sampler",
                  "textcolor": [
                    1.0,
                    0.6588,
                    0.4,
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
                  "varname": "selinfo",
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
                    "SLOT",
                    "MIX",
                    "SEQ",
                    "ENV",
                    "POOL"
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
                  "id": "obj-6",
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
                  "id": "obj-7",
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
                  "id": "obj-8",
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
                  "text": "js pmor_bank.js gated sampler",
                  "varname": "bank"
                }
              },
              {
                "box": {
                  "id": "obj-9",
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
                  "text": "send pmor_tx"
                }
              },
              {
                "box": {
                  "id": "obj-11",
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
                  "id": "obj-12",
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
                  "id": "obj-13",
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
                  "id": "obj-14",
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
                  "id": "obj-15",
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
                  "text": "receive pmor_ui_sel"
                }
              },
              {
                "box": {
                  "id": "obj-17",
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
                  "id": "obj-18",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    300,
                    688,
                    16
                  ],
                  "text": "values come from PMOR; every edit is sent to PMOR, which keeps it",
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
                    340,
                    688,
                    16
                  ]
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
                    16,
                    40,
                    312,
                    16
                  ],
                  "text": "SLOTS  (click to select)",
                  "textcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh0_13",
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    80,
                    312,
                    16
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
                    344,
                    40,
                    360,
                    16
                  ],
                  "text": "sound of the selected slot",
                  "textcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh0_14",
                  "presentation": 1,
                  "presentation_rect": [
                    344,
                    80,
                    360,
                    16
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-21",
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
                    64,
                    72,
                    48
                  ],
                  "text": "0",
                  "texton": "0",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_0",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    104,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-22",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1124,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-23",
                  "maxclass": "message",
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
                  "text": "pick slot 0"
                }
              },
              {
                "box": {
                  "id": "obj-24",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    96,
                    64,
                    72,
                    48
                  ],
                  "text": "1",
                  "texton": "1",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_1",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    96,
                    104,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-25",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1180,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-26",
                  "maxclass": "message",
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
                  "text": "pick slot 1"
                }
              },
              {
                "box": {
                  "id": "obj-27",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    176,
                    64,
                    72,
                    48
                  ],
                  "text": "2",
                  "texton": "2",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_2",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    176,
                    104,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-28",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1236,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-29",
                  "maxclass": "message",
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
                  "text": "pick slot 2"
                }
              },
              {
                "box": {
                  "id": "obj-30",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    256,
                    64,
                    72,
                    48
                  ],
                  "text": "3",
                  "texton": "3",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_3",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    256,
                    104,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-31",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1292,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-32",
                  "maxclass": "message",
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
                  "text": "pick slot 3"
                }
              },
              {
                "box": {
                  "id": "obj-33",
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
                    120,
                    72,
                    48
                  ],
                  "text": "4",
                  "texton": "4",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_4",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    160,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-34",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1348,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-35",
                  "maxclass": "message",
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
                  "text": "pick slot 4"
                }
              },
              {
                "box": {
                  "id": "obj-36",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    96,
                    120,
                    72,
                    48
                  ],
                  "text": "5",
                  "texton": "5",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_5",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    96,
                    160,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-37",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1404,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-38",
                  "maxclass": "message",
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
                  "text": "pick slot 5"
                }
              },
              {
                "box": {
                  "id": "obj-39",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    176,
                    120,
                    72,
                    48
                  ],
                  "text": "6",
                  "texton": "6",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_6",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    176,
                    160,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-40",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    788,
                    180,
                    22
                  ],
                  "text": "sel 1"
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
                    816,
                    180,
                    22
                  ],
                  "text": "pick slot 6"
                }
              },
              {
                "box": {
                  "id": "obj-42",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    256,
                    120,
                    72,
                    48
                  ],
                  "text": "7",
                  "texton": "7",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_7",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    256,
                    160,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-43",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    844,
                    180,
                    22
                  ],
                  "text": "sel 1"
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
                    872,
                    180,
                    22
                  ],
                  "text": "pick slot 7"
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
                    176,
                    72,
                    48
                  ],
                  "text": "8",
                  "texton": "8",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_8",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    216,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-46",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    900,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-47",
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
                  "text": "pick slot 8"
                }
              },
              {
                "box": {
                  "id": "obj-48",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    96,
                    176,
                    72,
                    48
                  ],
                  "text": "9",
                  "texton": "9",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_9",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    96,
                    216,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-49",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    956,
                    180,
                    22
                  ],
                  "text": "sel 1"
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
                    984,
                    180,
                    22
                  ],
                  "text": "pick slot 9"
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
                    176,
                    176,
                    72,
                    48
                  ],
                  "text": "10",
                  "texton": "10",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_10",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    176,
                    216,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-52",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1012,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-53",
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
                  "text": "pick slot 10"
                }
              },
              {
                "box": {
                  "id": "obj-54",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    256,
                    176,
                    72,
                    48
                  ],
                  "text": "11",
                  "texton": "11",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_11",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    256,
                    216,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-55",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1068,
                    180,
                    22
                  ],
                  "text": "sel 1"
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
                    1096,
                    180,
                    22
                  ],
                  "text": "pick slot 11"
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
                    16,
                    232,
                    72,
                    48
                  ],
                  "text": "12",
                  "texton": "12",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_12",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    272,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-58",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1124,
                    180,
                    22
                  ],
                  "text": "sel 1"
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
                    1152,
                    180,
                    22
                  ],
                  "text": "pick slot 12"
                }
              },
              {
                "box": {
                  "id": "obj-60",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    96,
                    232,
                    72,
                    48
                  ],
                  "text": "13",
                  "texton": "13",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_13",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    96,
                    272,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-61",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1180,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-62",
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
                  "text": "pick slot 13"
                }
              },
              {
                "box": {
                  "id": "obj-63",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    176,
                    232,
                    72,
                    48
                  ],
                  "text": "14",
                  "texton": "14",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_14",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    176,
                    272,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-64",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1236,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-65",
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
                  "text": "pick slot 14"
                }
              },
              {
                "box": {
                  "id": "obj-66",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    256,
                    232,
                    72,
                    48
                  ],
                  "text": "15",
                  "texton": "15",
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
                  "rounded": 4,
                  "mode": 0,
                  "varname": "sl_15",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    256,
                    272,
                    72,
                    48
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-67",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    1292,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-68",
                  "maxclass": "message",
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
                  "text": "pick slot 15"
                }
              },
              {
                "box": {
                  "id": "obj-69",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    372,
                    136,
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
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_s0",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    372,
                    176,
                    40,
                    40
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
                    1348,
                    180,
                    22
                  ],
                  "text": "prepend bc s0"
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
                    348,
                    176,
                    88,
                    16
                  ],
                  "text": "PITCH",
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
                  "varname": "bl_s0",
                  "presentation": 1,
                  "presentation_rect": [
                    348,
                    216,
                    88,
                    16
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-72",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    348,
                    192,
                    88,
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
                  "varname": "bv_s0",
                  "presentation": 1,
                  "presentation_rect": [
                    348,
                    232,
                    88,
                    16
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-73",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    460,
                    136,
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
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_s1",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    460,
                    176,
                    40,
                    40
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
                    1376,
                    180,
                    22
                  ],
                  "text": "prepend bc s1"
                }
              },
              {
                "box": {
                  "id": "obj-75",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    436,
                    176,
                    88,
                    16
                  ],
                  "text": "FORMANT",
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
                  "varname": "bl_s1",
                  "presentation": 1,
                  "presentation_rect": [
                    436,
                    216,
                    88,
                    16
                  ]
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
                    436,
                    192,
                    88,
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
                  "varname": "bv_s1",
                  "presentation": 1,
                  "presentation_rect": [
                    436,
                    232,
                    88,
                    16
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-77",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    548,
                    136,
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
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_s2",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    548,
                    176,
                    40,
                    40
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
                    1404,
                    180,
                    22
                  ],
                  "text": "prepend bc s2"
                }
              },
              {
                "box": {
                  "id": "obj-79",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    524,
                    176,
                    88,
                    16
                  ],
                  "text": "CUTOFF",
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
                  "varname": "bl_s2",
                  "presentation": 1,
                  "presentation_rect": [
                    524,
                    216,
                    88,
                    16
                  ]
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
                    524,
                    192,
                    88,
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
                  "varname": "bv_s2",
                  "presentation": 1,
                  "presentation_rect": [
                    524,
                    232,
                    88,
                    16
                  ]
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
                    640,
                    140,
                    32,
                    32
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "checkedcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "elementcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bordercolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "varname": "bkt_s3",
                  "presentation": 1,
                  "presentation_rect": [
                    640,
                    180,
                    32,
                    32
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
                    400,
                    760,
                    180,
                    22
                  ],
                  "text": "prepend br s3"
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
                    612,
                    176,
                    88,
                    16
                  ],
                  "text": "REVERSE",
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
                  "varname": "bl_s3",
                  "presentation": 1,
                  "presentation_rect": [
                    612,
                    216,
                    88,
                    16
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-84",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    612,
                    192,
                    88,
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
                  "varname": "bv_s3",
                  "presentation": 1,
                  "presentation_rect": [
                    612,
                    232,
                    88,
                    16
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-85",
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
                  "text": "sampler mix",
                  "textcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh1_15",
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
                  "id": "obj-86",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    160,
                    72,
                    64,
                    64
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_smix3328",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    160,
                    112,
                    64,
                    64
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-87",
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
                  "text": "prepend bc smix3328"
                }
              },
              {
                "box": {
                  "id": "obj-88",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    140,
                    136,
                    104,
                    16
                  ],
                  "text": "VOLUME",
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
                  "varname": "bl_smix3328",
                  "presentation": 1,
                  "presentation_rect": [
                    140,
                    176,
                    104,
                    16
                  ],
                  "hidden": 1
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
                    140,
                    152,
                    104,
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
                  "varname": "bv_smix3328",
                  "presentation": 1,
                  "presentation_rect": [
                    140,
                    192,
                    104,
                    16
                  ],
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
                    272,
                    72,
                    64,
                    64
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_smix3329",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    272,
                    112,
                    64,
                    64
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
                    400,
                    816,
                    180,
                    22
                  ],
                  "text": "prepend bc smix3329"
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
                    252,
                    136,
                    104,
                    16
                  ],
                  "text": "WIDTH",
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
                  "varname": "bl_smix3329",
                  "presentation": 1,
                  "presentation_rect": [
                    252,
                    176,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-93",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    252,
                    152,
                    104,
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
                  "varname": "bv_smix3329",
                  "presentation": 1,
                  "presentation_rect": [
                    252,
                    192,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-94",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    384,
                    72,
                    64,
                    64
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_smix3330",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    384,
                    112,
                    64,
                    64
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-95",
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
                  "text": "prepend bc smix3330"
                }
              },
              {
                "box": {
                  "id": "obj-96",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    364,
                    136,
                    104,
                    16
                  ],
                  "text": "WIDTHPRB",
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
                  "varname": "bl_smix3330",
                  "presentation": 1,
                  "presentation_rect": [
                    364,
                    176,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-97",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    364,
                    152,
                    104,
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
                  "varname": "bv_smix3330",
                  "presentation": 1,
                  "presentation_rect": [
                    364,
                    192,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-98",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    496,
                    72,
                    64,
                    64
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_smix3331",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    496,
                    112,
                    64,
                    64
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-99",
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
                  "text": "prepend bc smix3331"
                }
              },
              {
                "box": {
                  "id": "obj-100",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    476,
                    136,
                    104,
                    16
                  ],
                  "text": "POLY",
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
                  "varname": "bl_smix3331",
                  "presentation": 1,
                  "presentation_rect": [
                    476,
                    176,
                    104,
                    16
                  ],
                  "hidden": 1
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
                    476,
                    152,
                    104,
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
                  "varname": "bv_smix3331",
                  "presentation": 1,
                  "presentation_rect": [
                    476,
                    192,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-102",
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
                  "text": "sampler sequencer / generative",
                  "textcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh2_16",
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
                  "id": "obj-103",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    60,
                    88,
                    24,
                    24
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "checkedcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "elementcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bordercolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "varname": "bkt_q4160",
                  "presentation": 1,
                  "presentation_rect": [
                    60,
                    128,
                    24,
                    24
                  ],
                  "hidden": 1
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
                    900,
                    180,
                    22
                  ],
                  "text": "prepend br q4160"
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
                    24,
                    120,
                    96,
                    16
                  ],
                  "text": "REC",
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
                  "varname": "bl_q4160",
                  "presentation": 1,
                  "presentation_rect": [
                    24,
                    160,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-106",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    24,
                    136,
                    96,
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
                  "varname": "bv_q4160",
                  "presentation": 1,
                  "presentation_rect": [
                    24,
                    176,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-107",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    156,
                    88,
                    24,
                    24
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "checkedcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "elementcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bordercolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "varname": "bkt_q4161",
                  "presentation": 1,
                  "presentation_rect": [
                    156,
                    128,
                    24,
                    24
                  ],
                  "hidden": 1
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
                    928,
                    180,
                    22
                  ],
                  "text": "prepend br q4161"
                }
              },
              {
                "box": {
                  "id": "obj-109",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    120,
                    120,
                    96,
                    16
                  ],
                  "text": "CLEAR",
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
                  "varname": "bl_q4161",
                  "presentation": 1,
                  "presentation_rect": [
                    120,
                    160,
                    96,
                    16
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
                    120,
                    136,
                    96,
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
                  "varname": "bv_q4161",
                  "presentation": 1,
                  "presentation_rect": [
                    120,
                    176,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-111",
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
                    88,
                    80,
                    24
                  ],
                  "items": [
                    "MANUAL",
                    ",",
                    "PLAY",
                    ",",
                    "GEN"
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
                  "varname": "bkm_q4162",
                  "presentation": 1,
                  "presentation_rect": [
                    224,
                    128,
                    80,
                    24
                  ],
                  "hidden": 1
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
                    956,
                    180,
                    22
                  ],
                  "text": "prepend bm q4162"
                }
              },
              {
                "box": {
                  "id": "obj-113",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    216,
                    120,
                    96,
                    16
                  ],
                  "text": "MODE",
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
                  "varname": "bl_q4162",
                  "presentation": 1,
                  "presentation_rect": [
                    216,
                    160,
                    96,
                    16
                  ],
                  "hidden": 1
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
                    216,
                    136,
                    96,
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
                  "varname": "bv_q4162",
                  "presentation": 1,
                  "presentation_rect": [
                    216,
                    176,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-115",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    340,
                    80,
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
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_q4163",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    340,
                    120,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    984,
                    180,
                    22
                  ],
                  "text": "prepend bc q4163"
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
                    312,
                    120,
                    96,
                    16
                  ],
                  "text": "PROB",
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
                  "varname": "bl_q4163",
                  "presentation": 1,
                  "presentation_rect": [
                    312,
                    160,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-118",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    312,
                    136,
                    96,
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
                  "varname": "bv_q4163",
                  "presentation": 1,
                  "presentation_rect": [
                    312,
                    176,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-119",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    436,
                    80,
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
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_q4164",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    436,
                    120,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    1012,
                    180,
                    22
                  ],
                  "text": "prepend bc q4164"
                }
              },
              {
                "box": {
                  "id": "obj-121",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    408,
                    120,
                    96,
                    16
                  ],
                  "text": "DENSITY",
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
                  "varname": "bl_q4164",
                  "presentation": 1,
                  "presentation_rect": [
                    408,
                    160,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-122",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    408,
                    136,
                    96,
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
                  "varname": "bv_q4164",
                  "presentation": 1,
                  "presentation_rect": [
                    408,
                    176,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-123",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    532,
                    80,
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
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_q4165",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    532,
                    120,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    1040,
                    180,
                    22
                  ],
                  "text": "prepend bc q4165"
                }
              },
              {
                "box": {
                  "id": "obj-125",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    504,
                    120,
                    96,
                    16
                  ],
                  "text": "BARS",
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
                  "varname": "bl_q4165",
                  "presentation": 1,
                  "presentation_rect": [
                    504,
                    160,
                    96,
                    16
                  ],
                  "hidden": 1
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
                    504,
                    136,
                    96,
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
                  "varname": "bv_q4165",
                  "presentation": 1,
                  "presentation_rect": [
                    504,
                    176,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-127",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    628,
                    80,
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
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_q4166",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    628,
                    120,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    1068,
                    180,
                    22
                  ],
                  "text": "prepend bc q4166"
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
                    600,
                    120,
                    96,
                    16
                  ],
                  "text": "STEPS",
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
                  "varname": "bl_q4166",
                  "presentation": 1,
                  "presentation_rect": [
                    600,
                    160,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-130",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    600,
                    136,
                    96,
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
                  "varname": "bv_q4166",
                  "presentation": 1,
                  "presentation_rect": [
                    600,
                    176,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-131",
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
                  "text": "playback window and envelope of the selected slot",
                  "textcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh3_17",
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
                  "id": "obj-132",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    104,
                    72,
                    64,
                    64
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_x0",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    104,
                    112,
                    64,
                    64
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-133",
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
                  "text": "prepend bc x0"
                }
              },
              {
                "box": {
                  "id": "obj-134",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    84,
                    136,
                    104,
                    16
                  ],
                  "text": "START",
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
                  "varname": "bl_x0",
                  "presentation": 1,
                  "presentation_rect": [
                    84,
                    176,
                    104,
                    16
                  ],
                  "hidden": 1
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
                    84,
                    152,
                    104,
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
                  "varname": "bv_x0",
                  "presentation": 1,
                  "presentation_rect": [
                    84,
                    192,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-136",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    216,
                    72,
                    64,
                    64
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_x1",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    216,
                    112,
                    64,
                    64
                  ],
                  "hidden": 1
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
                    400,
                    1124,
                    180,
                    22
                  ],
                  "text": "prepend bc x1"
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
                    196,
                    136,
                    104,
                    16
                  ],
                  "text": "END",
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
                  "varname": "bl_x1",
                  "presentation": 1,
                  "presentation_rect": [
                    196,
                    176,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-139",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    196,
                    152,
                    104,
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
                  "varname": "bv_x1",
                  "presentation": 1,
                  "presentation_rect": [
                    196,
                    192,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-140",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    344,
                    88,
                    32,
                    32
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "checkedcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "elementcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bordercolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "varname": "bkt_x2",
                  "presentation": 1,
                  "presentation_rect": [
                    344,
                    128,
                    32,
                    32
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-141",
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
                  "text": "prepend br x2"
                }
              },
              {
                "box": {
                  "id": "obj-142",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    308,
                    136,
                    104,
                    16
                  ],
                  "text": "LOOP",
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
                  "varname": "bl_x2",
                  "presentation": 1,
                  "presentation_rect": [
                    308,
                    176,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-143",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    308,
                    152,
                    104,
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
                  "varname": "bv_x2",
                  "presentation": 1,
                  "presentation_rect": [
                    308,
                    192,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-144",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    440,
                    72,
                    64,
                    64
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_x3",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    440,
                    112,
                    64,
                    64
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-145",
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
                  "text": "prepend bc x3"
                }
              },
              {
                "box": {
                  "id": "obj-146",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    420,
                    136,
                    104,
                    16
                  ],
                  "text": "ATTACK",
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
                  "varname": "bl_x3",
                  "presentation": 1,
                  "presentation_rect": [
                    420,
                    176,
                    104,
                    16
                  ],
                  "hidden": 1
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
                    420,
                    152,
                    104,
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
                  "varname": "bv_x3",
                  "presentation": 1,
                  "presentation_rect": [
                    420,
                    192,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-148",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    552,
                    72,
                    64,
                    64
                  ],
                  "needlecolor": [
                    0.9529,
                    0.9333,
                    1.0,
                    1.0
                  ],
                  "outlinecolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_x4",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    552,
                    112,
                    64,
                    64
                  ],
                  "hidden": 1
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
                    1208,
                    180,
                    22
                  ],
                  "text": "prepend bc x4"
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
                    532,
                    136,
                    104,
                    16
                  ],
                  "text": "RELEASE",
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
                  "varname": "bl_x4",
                  "presentation": 1,
                  "presentation_rect": [
                    532,
                    176,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-151",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    532,
                    152,
                    104,
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
                  "varname": "bv_x4",
                  "presentation": 1,
                  "presentation_rect": [
                    532,
                    192,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-152",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_ui_status"
                }
              },
              {
                "box": {
                  "id": "obj-153",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "",
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1264,
                    180,
                    22
                  ],
                  "text": "route st"
                }
              },
              {
                "box": {
                  "id": "obj-154",
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
                    400,
                    1292,
                    180,
                    22
                  ],
                  "text": "js pmor_browser.js 0 gated",
                  "varname": "br"
                }
              },
              {
                "box": {
                  "id": "obj-155",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    48,
                    240,
                    16
                  ],
                  "text": "SAMPLE POOL -> selected slot",
                  "textcolor": [
                    1.0,
                    0.6588,
                    0.4,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh4_18",
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    88,
                    240,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-156",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    264,
                    44,
                    320,
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
                    264,
                    84,
                    320,
                    24
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-157",
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
                  "text": "prepend poolpick"
                }
              },
              {
                "box": {
                  "id": "obj-158",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    592,
                    44,
                    112,
                    24
                  ],
                  "text": "assign to slot",
                  "texton": "assign to slot",
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
                    0.6588,
                    0.4,
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
                    592,
                    84,
                    112,
                    24
                  ],
                  "varname": "bh4_x1",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-159",
                  "maxclass": "message",
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
                  "text": "assign"
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
                    16,
                    80,
                    688,
                    16
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
                    16,
                    120,
                    688,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-161",
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
                    400,
                    1376,
                    180,
                    22
                  ],
                  "text": "route poolinfo store trans"
                }
              },
              {
                "box": {
                  "id": "obj-162",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    104,
                    688,
                    16
                  ],
                  "text": "assign needs EDIT mode (opening this window switches the session to EDIT)",
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
                    144,
                    688,
                    16
                  ],
                  "varname": "bh4_x2",
                  "hidden": 1
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
                    "obj-13",
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
                    "obj-8",
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
                    "obj-8",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-8",
                    2
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
                    "obj-6",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-28",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-35",
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
                    "obj-8",
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
                    "obj-37",
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
                    "obj-8",
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
                    "obj-41",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-59",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-87",
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
                    "obj-94",
                    0
                  ],
                  "destination": [
                    "obj-95",
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
                    "obj-8",
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
                    "obj-99",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-133",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-141",
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
                    "obj-8",
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
                    "obj-145",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-153",
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
                    1
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
                    "obj-156",
                    0
                  ],
                  "destination": [
                    "obj-157",
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
                    "obj-154",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-154",
                    2
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
                    "obj-154",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-154",
                    3
                  ],
                  "destination": [
                    "obj-161",
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
                    "obj-160",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-8",
                    1
                  ],
                  "destination": [
                    "obj-154",
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
          "text": "js pmor_lane.js 3 remap",
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
            40,
            364,
            24
          ],
          "items": [
            "slot 0",
            ",",
            "slot 1",
            ",",
            "slot 2",
            ",",
            "slot 3",
            ",",
            "slot 4",
            ",",
            "slot 5",
            ",",
            "slot 6",
            ",",
            "slot 7",
            ",",
            "slot 8",
            ",",
            "slot 9",
            ",",
            "slot 10",
            ",",
            "slot 11",
            ",",
            "slot 12",
            ",",
            "slot 13",
            ",",
            "slot 14",
            ",",
            "slot 15"
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
          "varname": "selslot",
          "presentation": 1,
          "presentation_rect": [
            16,
            40,
            364,
            24
          ]
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
            1208,
            180,
            22
          ],
          "text": "prepend pick slot"
        }
      },
      {
        "box": {
          "id": "obj-23",
          "maxclass": "newobj",
          "numinlets": 1,
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
          "text": "receive pmor_rx"
        }
      },
      {
        "box": {
          "id": "obj-24",
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
            1264,
            180,
            22
          ],
          "text": "js pmor_bank.js",
          "varname": "bank"
        }
      },
      {
        "box": {
          "id": "obj-25",
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
          "text": "receive pmor_ui_status"
        }
      },
      {
        "box": {
          "id": "obj-26",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            ""
          ],
          "patching_rect": [
            20,
            1320,
            180,
            22
          ],
          "text": "route st"
        }
      },
      {
        "box": {
          "id": "obj-27",
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
          "text": "receive pmor_ui_sel"
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
            1376,
            180,
            22
          ],
          "text": "send pmor_ui_sel"
        }
      },
      {
        "box": {
          "id": "obj-29",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            68,
            72,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lsmix3328",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Sampler Volume",
              "parameter_shortname": "VOLUME",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.25,
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
            1.0,
            0.6588,
            0.4,
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
            68,
            72,
            40,
            40
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
            1404,
            180,
            22
          ],
          "text": "prepend br Lsmix3328"
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
            52,
            112,
            72,
            16
          ],
          "text": "VOLUME",
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
          "varname": "bl_Lsmix3328",
          "presentation": 1,
          "presentation_rect": [
            52,
            112,
            72,
            16
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
            52,
            128,
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
          "varname": "bv_Lsmix3328",
          "presentation": 1,
          "presentation_rect": [
            52,
            128,
            72,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-33",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            140,
            72,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Ls0",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Sampler Sel Pitch",
              "parameter_shortname": "PITCH",
              "parameter_mmin": -1.0,
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
            1.0,
            0.6588,
            0.4,
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
            140,
            72,
            40,
            40
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
            210,
            760,
            180,
            22
          ],
          "text": "prepend br Ls0"
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
            124,
            112,
            72,
            16
          ],
          "text": "PITCH",
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
          "varname": "bl_Ls0",
          "presentation": 1,
          "presentation_rect": [
            124,
            112,
            72,
            16
          ]
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
            124,
            128,
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
          "varname": "bv_Ls0",
          "presentation": 1,
          "presentation_rect": [
            124,
            128,
            72,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-37",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            212,
            72,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Ls1",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Sampler Sel Formant",
              "parameter_shortname": "FORMANT",
              "parameter_mmin": -1.0,
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
            1.0,
            0.6588,
            0.4,
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
            212,
            72,
            40,
            40
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
            210,
            788,
            180,
            22
          ],
          "text": "prepend br Ls1"
        }
      },
      {
        "box": {
          "id": "obj-39",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            196,
            112,
            72,
            16
          ],
          "text": "FORMANT",
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
          "varname": "bl_Ls1",
          "presentation": 1,
          "presentation_rect": [
            196,
            112,
            72,
            16
          ]
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
            196,
            128,
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
          "varname": "bv_Ls1",
          "presentation": 1,
          "presentation_rect": [
            196,
            128,
            72,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-41",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            284,
            72,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Ls2",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Sampler Sel Cutoff",
              "parameter_shortname": "CUTOFF",
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
            1.0,
            0.6588,
            0.4,
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
            284,
            72,
            40,
            40
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
            210,
            816,
            180,
            22
          ],
          "text": "prepend br Ls2"
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
            268,
            112,
            72,
            16
          ],
          "text": "CUTOFF",
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
          "varname": "bl_Ls2",
          "presentation": 1,
          "presentation_rect": [
            268,
            112,
            72,
            16
          ]
        }
      },
      {
        "box": {
          "id": "obj-44",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            268,
            128,
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
          "varname": "bv_Ls2",
          "presentation": 1,
          "presentation_rect": [
            268,
            128,
            72,
            16
          ]
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
            16,
            72,
            40,
            24
          ],
          "text": "REVERSE",
          "parameter_enable": 1,
          "mode": 1,
          "texton": "REVERSE",
          "varname": "bkt_Ls3",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Sampler Sel Reverse",
              "parameter_shortname": "REVERSE",
              "parameter_mmin": 0.0,
              "parameter_mmax": 1.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 2,
              "parameter_enum": [
                "off",
                "on"
              ]
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
            1.0,
            0.6588,
            0.4,
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
          "fontname": "Arial"
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
            844,
            180,
            22
          ],
          "text": "prepend br Ls3"
        }
      },
      {
        "box": {
          "id": "obj-47",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            0,
            96,
            72,
            16
          ],
          "text": "REVERSE",
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
          "varname": "bl_Ls3"
        }
      },
      {
        "box": {
          "id": "obj-48",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            0,
            112,
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
          "varname": "bv_Ls3"
        }
      },
      {
        "box": {
          "id": "obj-49",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            16,
            72,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lsmix3329",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Sampler Width",
              "parameter_shortname": "WIDTH",
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
            1.0,
            0.6588,
            0.4,
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
          "fontname": "Arial"
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
            872,
            180,
            22
          ],
          "text": "prepend br Lsmix3329"
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
            0,
            112,
            72,
            16
          ],
          "text": "WIDTH",
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
          "varname": "bl_Lsmix3329"
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
            0,
            128,
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
          "varname": "bv_Lsmix3329"
        }
      },
      {
        "box": {
          "id": "obj-53",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            16,
            72,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lsmix3330",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Sampler W Prob",
              "parameter_shortname": "W PROB",
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
            1.0,
            0.6588,
            0.4,
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
          "fontname": "Arial"
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
          "text": "prepend br Lsmix3330"
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
            0,
            112,
            72,
            16
          ],
          "text": "W PROB",
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
          "varname": "bl_Lsmix3330"
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
            0,
            128,
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
          "varname": "bv_Lsmix3330"
        }
      },
      {
        "box": {
          "id": "obj-57",
          "maxclass": "live.dial",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": [
            "",
            "float"
          ],
          "patching_rect": [
            16,
            72,
            40,
            40
          ],
          "parameter_enable": 1,
          "showname": 0,
          "shownumber": 0,
          "varname": "bkr_Lsmix3331",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Sampler Poly",
              "parameter_shortname": "POLY",
              "parameter_mmin": 1.0,
              "parameter_mmax": 8.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 1,
              "parameter_exponent": 1.0,
              "parameter_unitstyle": 0
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            1.0,
            0.6588,
            0.4,
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
          "fontname": "Arial"
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
            928,
            180,
            22
          ],
          "text": "prepend br Lsmix3331"
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
            0,
            112,
            72,
            16
          ],
          "text": "POLY",
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
          "varname": "bl_Lsmix3331"
        }
      },
      {
        "box": {
          "id": "obj-60",
          "maxclass": "comment",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            0,
            128,
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
          "varname": "bv_Lsmix3331"
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
            8,
            8,
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
          "varname": "selinfo"
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
            "obj-14",
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
            "obj-24",
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
            "obj-24",
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
            "obj-28",
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
            "obj-24",
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
            "obj-24",
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
            "obj-24",
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
            "obj-24",
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
            "obj-24",
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
            "obj-24",
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
            "obj-24",
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
            "obj-24",
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
            "obj-24",
            0
          ]
        }
      }
    ],
    "devicewidth": 500.0,
    "openrect": [
      0.0,
      0.0,
      500.0,
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
      "obj-29": [
        "Sampler Volume",
        "VOLUME",
        0
      ],
      "obj-33": [
        "Sampler Sel Pitch",
        "PITCH",
        0
      ],
      "obj-37": [
        "Sampler Sel Formant",
        "FORMANT",
        0
      ],
      "obj-41": [
        "Sampler Sel Cutoff",
        "CUTOFF",
        0
      ],
      "obj-45": [
        "Sampler Sel Reverse",
        "REVERSE",
        0
      ],
      "obj-49": [
        "Sampler Width",
        "WIDTH",
        0
      ],
      "obj-53": [
        "Sampler W Prob",
        "W PROB",
        0
      ],
      "obj-57": [
        "Sampler Poly",
        "POLY",
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
