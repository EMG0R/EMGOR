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
          "text": "PMOR synth",
          "textcolor": [
            0.4902,
            0.7216,
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
          "text": "any ch -> ch 2 -> PMOR core",
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
            0.4902,
            0.7216,
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
              324.0
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
                    324
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
                    324
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
                    244
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
                    244
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
                  "text": "PMOR synth",
                  "textcolor": [
                    0.4902,
                    0.7216,
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
                    "SOUND",
                    "OSC",
                    "AMP",
                    "FILTER",
                    "GEN",
                    "LFO",
                    "SAVE"
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
                  "text": "js pmor_bank.js gated synth",
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
                    252,
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
                    292,
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
                    688,
                    16
                  ],
                  "text": "ALGORITHMS  (click to select; each lists the four knobs K1..K4 it gives)",
                  "textcolor": [
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh0_7",
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    80,
                    688,
                    16
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-20",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    0,
                    0,
                    96,
                    24
                  ],
                  "items": [
                    "THIEF",
                    ",",
                    "FM",
                    ",",
                    "WAVE",
                    ",",
                    "PLUCK",
                    ",",
                    "SUPER",
                    ",",
                    "CZ",
                    ",",
                    "FOLD",
                    ",",
                    "ADD"
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
                  "varname": "bkm_algo"
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
                    1124,
                    180,
                    22
                  ],
                  "text": "prepend bm algo"
                }
              },
              {
                "box": {
                  "id": "obj-22",
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
                    340,
                    24
                  ],
                  "text": "THIEF",
                  "texton": "THIEF",
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
                  "varname": "ag_0",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    104,
                    340,
                    24
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-23",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1152,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-24",
                  "maxclass": "message",
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
                  "text": "bm algo 0"
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
                    16,
                    88,
                    340,
                    16
                  ],
                  "text": "CHARACTR   GLIDEPCT   SWELLPCT   DRIFT",
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
                    128,
                    340,
                    16
                  ],
                  "varname": "bh0_x1"
                }
              },
              {
                "box": {
                  "id": "obj-26",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    364,
                    64,
                    340,
                    24
                  ],
                  "text": "FM",
                  "texton": "FM",
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
                  "varname": "ag_1",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    364,
                    104,
                    340,
                    24
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-27",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1208,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-28",
                  "maxclass": "message",
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
                  "text": "bm algo 1"
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
                    364,
                    88,
                    340,
                    16
                  ],
                  "text": "RATIO   INDEX   IDXDECAY   FEEDBK",
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
                    364,
                    128,
                    340,
                    16
                  ],
                  "varname": "bh0_x2"
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
                    16,
                    108,
                    340,
                    24
                  ],
                  "text": "WAVE",
                  "texton": "WAVE",
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
                  "varname": "ag_2",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    148,
                    340,
                    24
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
                    1264,
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
                    1292,
                    180,
                    22
                  ],
                  "text": "bm algo 2"
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
                    16,
                    132,
                    340,
                    16
                  ],
                  "text": "TABLE   MORPH   DETUNE   ENVMORPH",
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
                    172,
                    340,
                    16
                  ],
                  "varname": "bh0_x3"
                }
              },
              {
                "box": {
                  "id": "obj-34",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    364,
                    108,
                    340,
                    24
                  ],
                  "text": "PLUCK",
                  "texton": "PLUCK",
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
                  "varname": "ag_3",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    364,
                    148,
                    340,
                    24
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-35",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1320,
                    180,
                    22
                  ],
                  "text": "sel 1"
                }
              },
              {
                "box": {
                  "id": "obj-36",
                  "maxclass": "message",
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
                  "text": "bm algo 3"
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
                    364,
                    132,
                    340,
                    16
                  ],
                  "text": "DAMP   BRIGHT   PICKPOS   DECAY",
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
                    364,
                    172,
                    340,
                    16
                  ],
                  "varname": "bh0_x4"
                }
              },
              {
                "box": {
                  "id": "obj-38",
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
                    152,
                    340,
                    24
                  ],
                  "text": "SUPER",
                  "texton": "SUPER",
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
                  "varname": "ag_4",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    192,
                    340,
                    24
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-39",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    20,
                    1376,
                    180,
                    22
                  ],
                  "text": "sel 1"
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
                    20,
                    1404,
                    180,
                    22
                  ],
                  "text": "bm algo 4"
                }
              },
              {
                "box": {
                  "id": "obj-41",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    176,
                    340,
                    16
                  ],
                  "text": "DETUNE   SPREAD   MIX   SUB",
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
                    216,
                    340,
                    16
                  ],
                  "varname": "bh0_x5"
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
                    364,
                    152,
                    340,
                    24
                  ],
                  "text": "CZ",
                  "texton": "CZ",
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
                  "varname": "ag_5",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    364,
                    192,
                    340,
                    24
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
                    760,
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
                    788,
                    180,
                    22
                  ],
                  "text": "bm algo 5"
                }
              },
              {
                "box": {
                  "id": "obj-45",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    364,
                    176,
                    340,
                    16
                  ],
                  "text": "DISTORT   RESO   ENVDIST   SHAPE",
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
                    364,
                    216,
                    340,
                    16
                  ],
                  "varname": "bh0_x6"
                }
              },
              {
                "box": {
                  "id": "obj-46",
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
                    196,
                    340,
                    24
                  ],
                  "text": "FOLD",
                  "texton": "FOLD",
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
                  "varname": "ag_6",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    16,
                    236,
                    340,
                    24
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-47",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    816,
                    180,
                    22
                  ],
                  "text": "sel 1"
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
                    844,
                    180,
                    22
                  ],
                  "text": "bm algo 6"
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
                    16,
                    220,
                    340,
                    16
                  ],
                  "text": "FOLD   SYM   ENVFOLD   DRIVE",
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
                    260,
                    340,
                    16
                  ],
                  "varname": "bh0_x7"
                }
              },
              {
                "box": {
                  "id": "obj-50",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    364,
                    196,
                    340,
                    24
                  ],
                  "text": "ADD",
                  "texton": "ADD",
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
                  "varname": "ag_7",
                  "ignoreclick": 0,
                  "presentation": 1,
                  "presentation_rect": [
                    364,
                    236,
                    340,
                    24
                  ]
                }
              },
              {
                "box": {
                  "id": "obj-51",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    ""
                  ],
                  "patching_rect": [
                    210,
                    872,
                    180,
                    22
                  ],
                  "text": "sel 1"
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
                    900,
                    180,
                    22
                  ],
                  "text": "bm algo 7"
                }
              },
              {
                "box": {
                  "id": "obj-53",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    364,
                    220,
                    340,
                    16
                  ],
                  "text": "TILT   ODDEVEN   DECAYHI   DRIFT",
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
                    364,
                    260,
                    340,
                    16
                  ],
                  "varname": "bh0_x8"
                }
              },
              {
                "box": {
                  "id": "obj-54",
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
                  "text": "oscillator: algorithm knobs + volume, then pitch / glide / width",
                  "textcolor": [
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh1_8",
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
                  "id": "obj-55",
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_k0",
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
                  "text": "prepend bc k0"
                }
              },
              {
                "box": {
                  "id": "obj-57",
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
                  "text": "CHARACTR",
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
                  "varname": "bl_k0",
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
                  "id": "obj-58",
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
                  "varname": "bv_k0",
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
                  "id": "obj-59",
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_k1",
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
                  "id": "obj-60",
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
                  "text": "prepend bc k1"
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
                    196,
                    136,
                    104,
                    16
                  ],
                  "text": "GLIDEPCT",
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
                  "varname": "bl_k1",
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
                  "id": "obj-62",
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
                  "varname": "bv_k1",
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
                  "id": "obj-63",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    328,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_k2",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    328,
                    112,
                    64,
                    64
                  ],
                  "hidden": 1
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
                    984,
                    180,
                    22
                  ],
                  "text": "prepend bc k2"
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
                    308,
                    136,
                    104,
                    16
                  ],
                  "text": "SWELLPCT",
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
                  "varname": "bl_k2",
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
                  "id": "obj-66",
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
                  "varname": "bv_k2",
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
                  "id": "obj-67",
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_k3",
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
                  "id": "obj-68",
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
                  "text": "prepend bc k3"
                }
              },
              {
                "box": {
                  "id": "obj-69",
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
                  "text": "DRIFT",
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
                  "varname": "bl_k3",
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
                  "id": "obj-70",
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
                  "varname": "bv_k3",
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
                  "id": "obj-71",
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_vol",
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
                  "id": "obj-72",
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
                  "text": "prepend bc vol"
                }
              },
              {
                "box": {
                  "id": "obj-73",
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
                  "varname": "bl_vol",
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
                  "id": "obj-74",
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
                  "varname": "bv_vol",
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
                  "id": "obj-75",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    116,
                    176,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_o15",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    116,
                    216,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    1068,
                    180,
                    22
                  ],
                  "text": "prepend bc o15"
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
                    84,
                    216,
                    104,
                    16
                  ],
                  "text": "SEMIS",
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
                  "varname": "bl_o15",
                  "presentation": 1,
                  "presentation_rect": [
                    84,
                    256,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-78",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    84,
                    232,
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
                  "varname": "bv_o15",
                  "presentation": 1,
                  "presentation_rect": [
                    84,
                    272,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-79",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    228,
                    176,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_o16",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    228,
                    216,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    1096,
                    180,
                    22
                  ],
                  "text": "prepend bc o16"
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
                    196,
                    216,
                    104,
                    16
                  ],
                  "text": "GLIDEIN",
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
                  "varname": "bl_o16",
                  "presentation": 1,
                  "presentation_rect": [
                    196,
                    256,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-82",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    196,
                    232,
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
                  "varname": "bv_o16",
                  "presentation": 1,
                  "presentation_rect": [
                    196,
                    272,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-83",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    340,
                    176,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_o17",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    340,
                    216,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    1124,
                    180,
                    22
                  ],
                  "text": "prepend bc o17"
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
                    308,
                    216,
                    104,
                    16
                  ],
                  "text": "GLIDEOUT",
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
                  "varname": "bl_o17",
                  "presentation": 1,
                  "presentation_rect": [
                    308,
                    256,
                    104,
                    16
                  ],
                  "hidden": 1
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
                    308,
                    232,
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
                  "varname": "bv_o17",
                  "presentation": 1,
                  "presentation_rect": [
                    308,
                    272,
                    104,
                    16
                  ],
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
                    452,
                    176,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_o18",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    452,
                    216,
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
                    1152,
                    180,
                    22
                  ],
                  "text": "prepend bc o18"
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
                    420,
                    216,
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
                  "varname": "bl_o18",
                  "presentation": 1,
                  "presentation_rect": [
                    420,
                    256,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-90",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    420,
                    232,
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
                  "varname": "bv_o18",
                  "presentation": 1,
                  "presentation_rect": [
                    420,
                    272,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-91",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    564,
                    176,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_o19",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    564,
                    216,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    210,
                    1180,
                    180,
                    22
                  ],
                  "text": "prepend bc o19"
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
                    532,
                    216,
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
                  "varname": "bl_o19",
                  "presentation": 1,
                  "presentation_rect": [
                    532,
                    256,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-94",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    532,
                    232,
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
                  "varname": "bv_o19",
                  "presentation": 1,
                  "presentation_rect": [
                    532,
                    272,
                    104,
                    16
                  ],
                  "hidden": 1
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
                    16,
                    40,
                    688,
                    16
                  ],
                  "text": "amp envelope",
                  "textcolor": [
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh2_9",
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
                  "id": "obj-96",
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_a4",
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
                  "id": "obj-97",
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
                  "text": "prepend bc a4"
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
                    140,
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
                  "varname": "bl_a4",
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
                  "id": "obj-99",
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
                  "varname": "bv_a4",
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
                  "id": "obj-100",
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_a5",
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
                  "id": "obj-101",
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
                  "text": "prepend bc a5"
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
                    252,
                    136,
                    104,
                    16
                  ],
                  "text": "DECAY",
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
                  "varname": "bl_a5",
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
                  "id": "obj-103",
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
                  "varname": "bv_a5",
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
                  "id": "obj-104",
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_a6",
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
                  "id": "obj-105",
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
                  "text": "prepend bc a6"
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
                    364,
                    136,
                    104,
                    16
                  ],
                  "text": "SUSTAIN",
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
                  "varname": "bl_a6",
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
                  "id": "obj-107",
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
                  "varname": "bv_a6",
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
                  "id": "obj-108",
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_a7",
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
                  "id": "obj-109",
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
                  "text": "prepend bc a7"
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
                    476,
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
                  "varname": "bl_a7",
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
                  "id": "obj-111",
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
                  "varname": "bv_a7",
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
                  "id": "obj-112",
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
                  "text": "filter: cutoff / resonance, filter envelope",
                  "textcolor": [
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh3_10",
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
                  "id": "obj-113",
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_f8",
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
                  "id": "obj-114",
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
                  "text": "prepend bc f8"
                }
              },
              {
                "box": {
                  "id": "obj-115",
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
                  "varname": "bl_f8",
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
                  "id": "obj-116",
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
                  "varname": "bv_f8",
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
                  "id": "obj-117",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    328,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_f9",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    328,
                    112,
                    64,
                    64
                  ],
                  "hidden": 1
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
                    210,
                    1348,
                    180,
                    22
                  ],
                  "text": "prepend bc f9"
                }
              },
              {
                "box": {
                  "id": "obj-119",
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
                  "text": "RESO",
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
                  "varname": "bl_f9",
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
                  "id": "obj-120",
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
                  "varname": "bv_f9",
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
                  "id": "obj-121",
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_f14",
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
                  "id": "obj-122",
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
                  "text": "prepend bc f14"
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
                    420,
                    136,
                    104,
                    16
                  ],
                  "text": "FAMOUNT",
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
                  "varname": "bl_f14",
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
                  "id": "obj-124",
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
                  "varname": "bv_f14",
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
                  "id": "obj-125",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    172,
                    176,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_f10",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    172,
                    216,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    210,
                    1404,
                    180,
                    22
                  ],
                  "text": "prepend bc f10"
                }
              },
              {
                "box": {
                  "id": "obj-127",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    140,
                    216,
                    104,
                    16
                  ],
                  "text": "FATTACK",
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
                  "varname": "bl_f10",
                  "presentation": 1,
                  "presentation_rect": [
                    140,
                    256,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-128",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    140,
                    232,
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
                  "varname": "bv_f10",
                  "presentation": 1,
                  "presentation_rect": [
                    140,
                    272,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-129",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    284,
                    176,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_f11",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    284,
                    216,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    760,
                    180,
                    22
                  ],
                  "text": "prepend bc f11"
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
                    252,
                    216,
                    104,
                    16
                  ],
                  "text": "FDECAY",
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
                  "varname": "bl_f11",
                  "presentation": 1,
                  "presentation_rect": [
                    252,
                    256,
                    104,
                    16
                  ],
                  "hidden": 1
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
                    252,
                    232,
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
                  "varname": "bv_f11",
                  "presentation": 1,
                  "presentation_rect": [
                    252,
                    272,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-133",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    396,
                    176,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_f12",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    396,
                    216,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    788,
                    180,
                    22
                  ],
                  "text": "prepend bc f12"
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
                    364,
                    216,
                    104,
                    16
                  ],
                  "text": "FSUSTAIN",
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
                  "varname": "bl_f12",
                  "presentation": 1,
                  "presentation_rect": [
                    364,
                    256,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-136",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    364,
                    232,
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
                  "varname": "bv_f12",
                  "presentation": 1,
                  "presentation_rect": [
                    364,
                    272,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-137",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    508,
                    176,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_f13",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    508,
                    216,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    816,
                    180,
                    22
                  ],
                  "text": "prepend bc f13"
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
                    476,
                    216,
                    104,
                    16
                  ],
                  "text": "FRELEASE",
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
                  "varname": "bl_f13",
                  "presentation": 1,
                  "presentation_rect": [
                    476,
                    256,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-140",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    476,
                    232,
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
                  "varname": "bv_f13",
                  "presentation": 1,
                  "presentation_rect": [
                    476,
                    272,
                    104,
                    16
                  ],
                  "hidden": 1
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
                    16,
                    40,
                    688,
                    16
                  ],
                  "text": "synth sequencer / generative",
                  "textcolor": [
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh4_11",
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
                  "id": "obj-142",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    108,
                    80,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "elementcolor": [
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bordercolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "varname": "bkt_q4128",
                  "presentation": 1,
                  "presentation_rect": [
                    108,
                    120,
                    24,
                    24
                  ],
                  "hidden": 1
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
                    400,
                    844,
                    180,
                    22
                  ],
                  "text": "prepend br q4128"
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
                    72,
                    112,
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
                  "varname": "bl_q4128",
                  "presentation": 1,
                  "presentation_rect": [
                    72,
                    152,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-145",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    72,
                    128,
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
                  "varname": "bv_q4128",
                  "presentation": 1,
                  "presentation_rect": [
                    72,
                    168,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-146",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    204,
                    80,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "elementcolor": [
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bordercolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "varname": "bkt_q4129",
                  "presentation": 1,
                  "presentation_rect": [
                    204,
                    120,
                    24,
                    24
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-147",
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
                  "text": "prepend br q4129"
                }
              },
              {
                "box": {
                  "id": "obj-148",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    168,
                    112,
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
                  "varname": "bl_q4129",
                  "presentation": 1,
                  "presentation_rect": [
                    168,
                    152,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-149",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    168,
                    128,
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
                  "varname": "bv_q4129",
                  "presentation": 1,
                  "presentation_rect": [
                    168,
                    168,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-150",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    272,
                    80,
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
                  "varname": "bkm_q4130",
                  "presentation": 1,
                  "presentation_rect": [
                    272,
                    120,
                    80,
                    24
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-151",
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
                  "text": "prepend bm q4130"
                }
              },
              {
                "box": {
                  "id": "obj-152",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    264,
                    112,
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
                  "varname": "bl_q4130",
                  "presentation": 1,
                  "presentation_rect": [
                    264,
                    152,
                    96,
                    16
                  ],
                  "hidden": 1
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
                    264,
                    128,
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
                  "varname": "bv_q4130",
                  "presentation": 1,
                  "presentation_rect": [
                    264,
                    168,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-154",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    388,
                    72,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_q4131",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    388,
                    112,
                    40,
                    40
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-155",
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
                  "text": "prepend bc q4131"
                }
              },
              {
                "box": {
                  "id": "obj-156",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    360,
                    112,
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
                  "varname": "bl_q4131",
                  "presentation": 1,
                  "presentation_rect": [
                    360,
                    152,
                    96,
                    16
                  ],
                  "hidden": 1
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
                    360,
                    128,
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
                  "varname": "bv_q4131",
                  "presentation": 1,
                  "presentation_rect": [
                    360,
                    168,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-158",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    484,
                    72,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_q4132",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    484,
                    112,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    956,
                    180,
                    22
                  ],
                  "text": "prepend bc q4132"
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
                    456,
                    112,
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
                  "varname": "bl_q4132",
                  "presentation": 1,
                  "presentation_rect": [
                    456,
                    152,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-161",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    456,
                    128,
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
                  "varname": "bv_q4132",
                  "presentation": 1,
                  "presentation_rect": [
                    456,
                    168,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-162",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    580,
                    72,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_q4133",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    580,
                    112,
                    40,
                    40
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-163",
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
                  "text": "prepend bc q4133"
                }
              },
              {
                "box": {
                  "id": "obj-164",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    552,
                    112,
                    96,
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
                  "varname": "bl_q4133",
                  "presentation": 1,
                  "presentation_rect": [
                    552,
                    152,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-165",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    552,
                    128,
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
                  "varname": "bv_q4133",
                  "presentation": 1,
                  "presentation_rect": [
                    552,
                    168,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-166",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    100,
                    152,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_q4134",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    100,
                    192,
                    40,
                    40
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-167",
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
                  "text": "prepend bc q4134"
                }
              },
              {
                "box": {
                  "id": "obj-168",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    72,
                    192,
                    96,
                    16
                  ],
                  "text": "OCTAVE",
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
                  "varname": "bl_q4134",
                  "presentation": 1,
                  "presentation_rect": [
                    72,
                    232,
                    96,
                    16
                  ],
                  "hidden": 1
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
                    72,
                    208,
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
                  "varname": "bv_q4134",
                  "presentation": 1,
                  "presentation_rect": [
                    72,
                    248,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-170",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    196,
                    152,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_q4135",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    196,
                    192,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    1040,
                    180,
                    22
                  ],
                  "text": "prepend bc q4135"
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
                    168,
                    192,
                    96,
                    16
                  ],
                  "text": "OCTPROB",
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
                  "varname": "bl_q4135",
                  "presentation": 1,
                  "presentation_rect": [
                    168,
                    232,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-173",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    168,
                    208,
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
                  "varname": "bv_q4135",
                  "presentation": 1,
                  "presentation_rect": [
                    168,
                    248,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-174",
                  "maxclass": "toggle",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "int"
                  ],
                  "patching_rect": [
                    300,
                    160,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "elementcolor": [
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bordercolor": [
                    0.3569,
                    0.2941,
                    0.6196,
                    1.0
                  ],
                  "varname": "bkt_q4136",
                  "presentation": 1,
                  "presentation_rect": [
                    300,
                    200,
                    24,
                    24
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-175",
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
                  "text": "prepend br q4136"
                }
              },
              {
                "box": {
                  "id": "obj-176",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    264,
                    192,
                    96,
                    16
                  ],
                  "text": "ARP",
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
                  "varname": "bl_q4136",
                  "presentation": 1,
                  "presentation_rect": [
                    264,
                    232,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-177",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    264,
                    208,
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
                  "varname": "bv_q4136",
                  "presentation": 1,
                  "presentation_rect": [
                    264,
                    248,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-178",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    368,
                    160,
                    80,
                    24
                  ],
                  "items": [
                    "1/4",
                    ",",
                    "1/8",
                    ",",
                    "1/16",
                    ",",
                    "1/32"
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
                  "varname": "bkm_q4137",
                  "presentation": 1,
                  "presentation_rect": [
                    368,
                    200,
                    80,
                    24
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-179",
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
                  "text": "prepend bm q4137"
                }
              },
              {
                "box": {
                  "id": "obj-180",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    360,
                    192,
                    96,
                    16
                  ],
                  "text": "ARPRATE",
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
                  "varname": "bl_q4137",
                  "presentation": 1,
                  "presentation_rect": [
                    360,
                    232,
                    96,
                    16
                  ],
                  "hidden": 1
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
                    360,
                    208,
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
                  "varname": "bv_q4137",
                  "presentation": 1,
                  "presentation_rect": [
                    360,
                    248,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-182",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    484,
                    152,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_q4138",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    484,
                    192,
                    40,
                    40
                  ],
                  "hidden": 1
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
                    1124,
                    180,
                    22
                  ],
                  "text": "prepend bc q4138"
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
                    456,
                    192,
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
                  "varname": "bl_q4138",
                  "presentation": 1,
                  "presentation_rect": [
                    456,
                    232,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-185",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    456,
                    208,
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
                  "varname": "bv_q4138",
                  "presentation": 1,
                  "presentation_rect": [
                    456,
                    248,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-186",
                  "maxclass": "dial",
                  "numinlets": 1,
                  "numoutlets": 1,
                  "outlettype": [
                    "float"
                  ],
                  "patching_rect": [
                    580,
                    152,
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_q4139",
                  "min": 0.0,
                  "size": 1001,
                  "mult": 0.001,
                  "floatoutput": 1,
                  "presentation": 1,
                  "presentation_rect": [
                    580,
                    192,
                    40,
                    40
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-187",
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
                  "text": "prepend bc q4139"
                }
              },
              {
                "box": {
                  "id": "obj-188",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    552,
                    192,
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
                  "varname": "bl_q4139",
                  "presentation": 1,
                  "presentation_rect": [
                    552,
                    232,
                    96,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-189",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    552,
                    208,
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
                  "varname": "bv_q4139",
                  "presentation": 1,
                  "presentation_rect": [
                    552,
                    248,
                    96,
                    16
                  ],
                  "hidden": 1
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
                    16,
                    40,
                    688,
                    16
                  ],
                  "text": "LFO (engine-wide, depth 0 = off)",
                  "textcolor": [
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "fontsize": 12.0,
                  "fontname": "Arial",
                  "fontface": 1,
                  "textjustification": 0,
                  "varname": "bh5_12",
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
                  "id": "obj-191",
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_lforate",
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
                  "text": "prepend bc lforate"
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
                    140,
                    136,
                    104,
                    16
                  ],
                  "text": "RATE",
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
                  "varname": "bl_lforate",
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
                  "id": "obj-194",
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
                  "varname": "bv_lforate",
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
                  "id": "obj-195",
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
                    0.4902,
                    0.7216,
                    1.0,
                    1.0
                  ],
                  "bgcolor": [
                    0.051,
                    0.0353,
                    0.098,
                    1.0
                  ],
                  "varname": "bk_lfodepth",
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
                  "id": "obj-196",
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
                  "text": "prepend bc lfodepth"
                }
              },
              {
                "box": {
                  "id": "obj-197",
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
                  "text": "DEPTH",
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
                  "varname": "bl_lfodepth",
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
                  "id": "obj-198",
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
                  "varname": "bv_lfodepth",
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
                  "id": "obj-199",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    376,
                    92,
                    96,
                    24
                  ],
                  "items": [
                    "SINE",
                    ",",
                    "TRI",
                    ",",
                    "SQUARE",
                    ",",
                    "S&H"
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
                  "varname": "bkm_lfoshape",
                  "presentation": 1,
                  "presentation_rect": [
                    376,
                    132,
                    96,
                    24
                  ],
                  "hidden": 1
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
                    400,
                    1236,
                    180,
                    22
                  ],
                  "text": "prepend bm lfoshape"
                }
              },
              {
                "box": {
                  "id": "obj-201",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    372,
                    136,
                    104,
                    16
                  ],
                  "text": "SHAPE",
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
                  "varname": "bl_lfoshape",
                  "presentation": 1,
                  "presentation_rect": [
                    372,
                    176,
                    104,
                    16
                  ],
                  "hidden": 1
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
                    372,
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
                  "varname": "bv_lfoshape",
                  "presentation": 1,
                  "presentation_rect": [
                    372,
                    192,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-203",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    488,
                    92,
                    96,
                    24
                  ],
                  "items": [
                    "CUTOFF",
                    ",",
                    "PITCH",
                    ",",
                    "AMP"
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
                  "varname": "bkm_lfodest",
                  "presentation": 1,
                  "presentation_rect": [
                    488,
                    132,
                    96,
                    24
                  ],
                  "hidden": 1
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
                    400,
                    1264,
                    180,
                    22
                  ],
                  "text": "prepend bm lfodest"
                }
              },
              {
                "box": {
                  "id": "obj-205",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    484,
                    136,
                    104,
                    16
                  ],
                  "text": "DEST",
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
                  "varname": "bl_lfodest",
                  "presentation": 1,
                  "presentation_rect": [
                    484,
                    176,
                    104,
                    16
                  ],
                  "hidden": 1
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
                    484,
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
                  "varname": "bv_lfodest",
                  "presentation": 1,
                  "presentation_rect": [
                    484,
                    192,
                    104,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-207",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_ui_status"
                }
              },
              {
                "box": {
                  "id": "obj-208",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "",
                    ""
                  ],
                  "patching_rect": [
                    400,
                    1320,
                    180,
                    22
                  ],
                  "text": "route st"
                }
              },
              {
                "box": {
                  "id": "obj-209",
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
                    1348,
                    180,
                    22
                  ],
                  "text": "js pmor_browser.js 4 gated synth",
                  "varname": "br"
                }
              },
              {
                "box": {
                  "id": "obj-210",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    16,
                    52,
                    136,
                    16
                  ],
                  "text": "SAVE USER PRESET",
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
                    92,
                    136,
                    16
                  ],
                  "varname": "bh6_x9",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-211",
                  "maxclass": "textedit",
                  "numinlets": 1,
                  "numoutlets": 4,
                  "outlettype": [
                    "",
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    152,
                    48,
                    112,
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
                  "fontsize": 11.0,
                  "fontname": "Arial",
                  "border": 1,
                  "rounded": 4,
                  "varname": "savename",
                  "presentation": 1,
                  "presentation_rect": [
                    152,
                    88,
                    112,
                    24
                  ],
                  "hidden": 1
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
                    400,
                    1376,
                    180,
                    22
                  ],
                  "text": "prepend savename"
                }
              },
              {
                "box": {
                  "id": "obj-213",
                  "maxclass": "umenu",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "int",
                    "",
                    ""
                  ],
                  "patching_rect": [
                    272,
                    48,
                    152,
                    24
                  ],
                  "items": [
                    "(connect)"
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
                  "varname": "saveslot",
                  "presentation": 1,
                  "presentation_rect": [
                    272,
                    88,
                    152,
                    24
                  ],
                  "hidden": 1
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
                    1404,
                    180,
                    22
                  ],
                  "text": "prepend saveslotpick"
                }
              },
              {
                "box": {
                  "id": "obj-215",
                  "maxclass": "textbutton",
                  "numinlets": 1,
                  "numoutlets": 3,
                  "outlettype": [
                    "",
                    "",
                    "int"
                  ],
                  "patching_rect": [
                    432,
                    48,
                    64,
                    24
                  ],
                  "text": "save",
                  "texton": "save",
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
                    0.4902,
                    0.7216,
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
                    432,
                    88,
                    64,
                    24
                  ],
                  "varname": "bh6_x10",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-216",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "bang",
                    "bang"
                  ],
                  "patching_rect": [
                    590,
                    760,
                    180,
                    22
                  ],
                  "text": "t b b"
                }
              },
              {
                "box": {
                  "id": "obj-217",
                  "maxclass": "message",
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
                  "text": "bang"
                }
              },
              {
                "box": {
                  "id": "obj-218",
                  "maxclass": "message",
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
                  "text": "dosave"
                }
              },
              {
                "box": {
                  "id": "obj-219",
                  "maxclass": "comment",
                  "numinlets": 1,
                  "numoutlets": 0,
                  "outlettype": [],
                  "patching_rect": [
                    504,
                    52,
                    200,
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
                  "textjustification": 0,
                  "varname": "saveinfo",
                  "presentation": 1,
                  "presentation_rect": [
                    504,
                    92,
                    200,
                    16
                  ],
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-220",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "",
                    ""
                  ],
                  "patching_rect": [
                    590,
                    844,
                    180,
                    22
                  ],
                  "text": "route saveinfo"
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
                    16,
                    76,
                    688,
                    16
                  ],
                  "text": "saves the current algo's sound as a user preset (slots 5..31; 0..4 are factory); an existing slot needs a second click.",
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
                    116,
                    688,
                    16
                  ],
                  "varname": "bh6_x11",
                  "hidden": 1
                }
              },
              {
                "box": {
                  "id": "obj-222",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_ui_sel"
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
                    900,
                    180,
                    22
                  ],
                  "text": "prepend selui"
                }
              },
              {
                "box": {
                  "id": "obj-224",
                  "maxclass": "newobj",
                  "numinlets": 1,
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
                  "text": "receive pmor_rx"
                }
              },
              {
                "box": {
                  "id": "obj-225",
                  "maxclass": "newobj",
                  "numinlets": 1,
                  "numoutlets": 2,
                  "outlettype": [
                    "",
                    ""
                  ],
                  "patching_rect": [
                    590,
                    956,
                    180,
                    22
                  ],
                  "text": "route param"
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
                    984,
                    180,
                    22
                  ],
                  "text": "prepend saveparam"
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-100",
                    0
                  ],
                  "destination": [
                    "obj-101",
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
                    "obj-8",
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
                    "obj-105",
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
                    "obj-8",
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
                    "obj-109",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-147",
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
                    "obj-8",
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
                    "obj-151",
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
                    "obj-8",
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
                    "obj-155",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-163",
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
                    "obj-8",
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
                    "obj-167",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-175",
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
                    "obj-8",
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
                    "obj-179",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-187",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-8",
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
                    "obj-209",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-209",
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
                    "obj-209",
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
                    "obj-209",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-209",
                    7
                  ],
                  "destination": [
                    "obj-213",
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
                    1
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
                    "obj-211",
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
                    "obj-209",
                    0
                  ]
                }
              },
              {
                "patchline": {
                  "source": [
                    "obj-209",
                    3
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
                    "obj-219",
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
                    "obj-209",
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
                    "obj-225",
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
                    "obj-209",
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
          "text": "js pmor_lane.js 2 remap",
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
          "maxclass": "live.menu",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "",
            "",
            "float"
          ],
          "patching_rect": [
            16,
            40,
            364,
            24
          ],
          "parameter_enable": 1,
          "varname": "bkm_Lalgo",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Algorithm",
              "parameter_shortname": "ALGO",
              "parameter_mmin": 0.0,
              "parameter_mmax": 7.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 2,
              "parameter_enum": [
                "THIEF",
                "FM",
                "WAVE",
                "PLUCK",
                "SUPER",
                "CZ",
                "FOLD",
                "ADD"
              ]
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
          "text": "prepend bm Lalgo"
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
          "varname": "bkr_Lvol",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Volume",
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
            0.4902,
            0.7216,
            1.0,
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
          "text": "prepend br Lvol"
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
          "varname": "bl_Lvol",
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
          "varname": "bv_Lvol",
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
          "varname": "bkr_Lp8",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Cutoff",
              "parameter_shortname": "CUTOFF",
              "parameter_mmin": 40.0,
              "parameter_mmax": 12000.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 3.0,
              "parameter_unitstyle": 9,
              "parameter_units": "%.0f Hz"
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.4902,
            0.7216,
            1.0,
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
          "text": "prepend br Lp8"
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
          "varname": "bl_Lp8",
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
          "varname": "bv_Lp8",
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
          "varname": "bkr_Lp9",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Resonance",
              "parameter_shortname": "RESO",
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
            0.4902,
            0.7216,
            1.0,
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
          "text": "prepend br Lp9"
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
          "text": "RESO",
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
          "varname": "bl_Lp9",
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
          "varname": "bv_Lp9",
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
          "varname": "bkr_Lp7",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Release",
              "parameter_shortname": "RELEASE",
              "parameter_mmin": 0.005,
              "parameter_mmax": 6.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 3.0,
              "parameter_unitstyle": 9,
              "parameter_units": "%.3f s"
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.4902,
            0.7216,
            1.0,
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
          "text": "prepend br Lp7"
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
          "varname": "bl_Lp7",
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
          "varname": "bv_Lp7",
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
          "varname": "bkn_Lk0",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Algo Knob 1 (0-1)",
              "parameter_shortname": "CHARACTR",
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
            0.4902,
            0.7216,
            1.0,
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
          "text": "prepend bc Lk0"
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
            112,
            72,
            16
          ],
          "text": "CHARACTR",
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
          "varname": "bl_Lk0"
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
          "varname": "bv_Lk0"
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
          "varname": "bkn_Lk1",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Algo Knob 2 (0-1)",
              "parameter_shortname": "GLIDEPCT",
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
            0.4902,
            0.7216,
            1.0,
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
          "text": "prepend bc Lk1"
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
          "text": "GLIDEPCT",
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
          "varname": "bl_Lk1"
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
          "varname": "bv_Lk1"
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
          "varname": "bkn_Lk2",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Algo Knob 3 (0-1)",
              "parameter_shortname": "SWELLPCT",
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
            0.4902,
            0.7216,
            1.0,
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
          "text": "prepend bc Lk2"
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
          "text": "SWELLPCT",
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
          "varname": "bl_Lk2"
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
          "varname": "bv_Lk2"
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
          "varname": "bkn_Lk3",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Algo Knob 4 (0-1)",
              "parameter_shortname": "DRIFT",
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
            0.4902,
            0.7216,
            1.0,
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
          "text": "prepend bc Lk3"
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
          "text": "DRIFT",
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
          "varname": "bl_Lk3"
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
          "varname": "bv_Lk3"
        }
      },
      {
        "box": {
          "id": "obj-61",
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
          "varname": "bkr_Lp4",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Attack",
              "parameter_shortname": "ATTACK",
              "parameter_mmin": 0.001,
              "parameter_mmax": 4.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 3.0,
              "parameter_unitstyle": 9,
              "parameter_units": "%.3f s"
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.4902,
            0.7216,
            1.0,
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
          "id": "obj-62",
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
          "text": "prepend br Lp4"
        }
      },
      {
        "box": {
          "id": "obj-63",
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
          "varname": "bl_Lp4"
        }
      },
      {
        "box": {
          "id": "obj-64",
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
          "varname": "bv_Lp4"
        }
      },
      {
        "box": {
          "id": "obj-65",
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
          "varname": "bkr_Lp5",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Decay",
              "parameter_shortname": "DECAY",
              "parameter_mmin": 0.005,
              "parameter_mmax": 6.0,
              "parameter_initial_enable": 0,
              "parameter_speedlim": 0.0,
              "parameter_type": 0,
              "parameter_exponent": 3.0,
              "parameter_unitstyle": 9,
              "parameter_units": "%.3f s"
            }
          },
          "dialcolor": [
            0.4314,
            0.3765,
            0.6275,
            1.0
          ],
          "activedialcolor": [
            0.4902,
            0.7216,
            1.0,
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
          "id": "obj-66",
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
          "text": "prepend br Lp5"
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
            0,
            112,
            72,
            16
          ],
          "text": "DECAY",
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
          "varname": "bl_Lp5"
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
          "varname": "bv_Lp5"
        }
      },
      {
        "box": {
          "id": "obj-69",
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
          "varname": "bkr_Lp6",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Sustain",
              "parameter_shortname": "SUSTAIN",
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
            0.4902,
            0.7216,
            1.0,
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
          "id": "obj-70",
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
          "text": "prepend br Lp6"
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
            0,
            112,
            72,
            16
          ],
          "text": "SUSTAIN",
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
          "varname": "bl_Lp6"
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
          "varname": "bv_Lp6"
        }
      },
      {
        "box": {
          "id": "obj-73",
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
          "varname": "bkr_Lp14",
          "saved_attribute_attributes": {
            "valueof": {
              "parameter_longname": "Synth Filter Env Amount",
              "parameter_shortname": "FAMOUNT",
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
            0.4902,
            0.7216,
            1.0,
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
          "id": "obj-74",
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
          "text": "prepend br Lp14"
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
            0,
            112,
            72,
            16
          ],
          "text": "FAMOUNT",
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
          "varname": "bl_Lp14"
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
          "varname": "bv_Lp14"
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
            "obj-24",
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
            "obj-24",
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
            "obj-24",
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
      "obj-21": [
        "Synth Algorithm",
        "ALGO",
        0
      ],
      "obj-29": [
        "Synth Volume",
        "VOLUME",
        0
      ],
      "obj-33": [
        "Synth Cutoff",
        "CUTOFF",
        0
      ],
      "obj-37": [
        "Synth Resonance",
        "RESO",
        0
      ],
      "obj-41": [
        "Synth Release",
        "RELEASE",
        0
      ],
      "obj-45": [
        "Synth Algo Knob 1 (0-1)",
        "CHARACTR",
        0
      ],
      "obj-49": [
        "Synth Algo Knob 2 (0-1)",
        "GLIDEPCT",
        0
      ],
      "obj-53": [
        "Synth Algo Knob 3 (0-1)",
        "SWELLPCT",
        0
      ],
      "obj-57": [
        "Synth Algo Knob 4 (0-1)",
        "DRIFT",
        0
      ],
      "obj-61": [
        "Synth Attack",
        "ATTACK",
        0
      ],
      "obj-65": [
        "Synth Decay",
        "DECAY",
        0
      ],
      "obj-69": [
        "Synth Sustain",
        "SUSTAIN",
        0
      ],
      "obj-73": [
        "Synth Filter Env Amount",
        "FAMOUNT",
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
