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
      500.0,
      320.0
    ],
    "openinpresentation": 0,
    "bglocked": 0,
    "devicewidth": 120.0,
    "openrect": [
      0.0,
      0.0,
      120.0,
      40.0
    ],
    "boxes": [
      {
        "box": {
          "id": "obj-3",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            20,
            130,
            150,
            22
          ],
          "text": "js pmor_dryrun.js"
        }
      },
      {
        "box": {
          "id": "obj-4",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 3,
          "outlettype": [
            "bang",
            "int",
            "int"
          ],
          "patching_rect": [
            200,
            20,
            100,
            22
          ],
          "text": "live.thisdevice"
        }
      },
      {
        "box": {
          "id": "obj-5",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [
            ""
          ],
          "patching_rect": [
            200,
            60,
            110,
            22
          ],
          "text": "prepend thisdevice"
        }
      },
      {
        "box": {
          "id": "obj-6",
          "maxclass": "newobj",
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": [],
          "patching_rect": [
            20,
            200,
            60,
            22
          ],
          "text": "plugout~ 1"
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
            100,
            200,
            60,
            22
          ],
          "text": "plugout~ 2"
        }
      }
    ],
    "lines": [
      {
        "patchline": {
          "source": [
            4,
            1
          ],
          "destination": [
            5,
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            5,
            0
          ],
          "destination": [
            3,
            0
          ]
        }
      }
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
    "parameters": {},
    "is_mpe": 0,
    "latency": 0,
    "minimum_live_version": "",
    "minimum_max_version": "",
    "platform_compatibility": 0,
    "autosave": 0
  }
}
