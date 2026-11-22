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
    "devicewidth": 150.0,
    "bglocked": 0,
    "openrect": [
      0.0,
      0.0,
      150.0,
      60.0
    ],
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
            150,
            60
          ],
          "presentation": 1,
          "presentation_rect": [
            0,
            0,
            150,
            60
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
            4,
            142,
            52
          ],
          "presentation": 1,
          "presentation_rect": [
            4,
            4,
            142,
            52
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
            8,
            130,
            20
          ],
          "text": "PMOR control+fx",
          "presentation": 1,
          "presentation_rect": [
            10,
            8,
            130,
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
            10,
            30,
            130,
            20
          ],
          "text": "-> PMOR core  (ch 4 5)",
          "presentation": 1,
          "presentation_rect": [
            10,
            30,
            130,
            20
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
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            "int"
          ],
          "patching_rect": [
            20,
            100,
            50,
            22
          ],
          "text": "midiin"
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
            150,
            170,
            22
          ],
          "text": "js pmor_lane.js 4 5"
        }
      },
      {
        "box": {
          "id": "obj-7",
          "maxclass": "newobj",
          "numinlets": 3,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            20,
            200,
            80,
            22
          ],
          "text": "pack 0 0 0"
        }
      },
      {
        "box": {
          "id": "obj-8",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            20,
            250,
            110,
            22
          ],
          "text": "send pmor_midi"
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
            "obj-8",
            0
          ]
        }
      }
    ]
  }
}
