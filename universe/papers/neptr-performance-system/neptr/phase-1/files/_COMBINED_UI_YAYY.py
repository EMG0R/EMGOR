import requests
from kivy.app import App
from kivy.uix.gridlayout import GridLayout
from kivy.uix.label import Label
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.slider import Slider
from kivy.properties import NumericProperty
from kivy.core.window import Window
from kivy.clock import Clock
from kivy.uix.widget import Widget
from kivy.graphics import Color, Rectangle
from time import sleep
import threading


# Set window size (optional, not needed in full-screen mode)
Window.size = (470, 800)


# Define parameter ranges
parameter_ranges = {
    "BPM": (40, 800),
    "PROBABILITY": (0, 100),
    "VOLUME": (0, 100),
    "spreadNOTEZ": (0, 10),  # Example range for spreadNOTEZ (adjust based on actual parameter range)
    "PAN": (-1, 1),
    "OFFSET": (-24, 24),  
    "CUTOFF": (0, 20000),
    "PEAK": (0, 1),
    "CENTS": (-1, 1),
    "WAVEFORM": (1, 6),
    "VOL1": (0, 150),
    "OFFSET1": (-1, 1),
    "PAN1": (-1, 1),
    "WAVE1": (1, 6),
    "VOL2": (0, 150),
    "OFFSET2": (-1, 1),
    "PAN2": (-1, 1),
    "WAVE2": (1, 6),
    "VOL3": (0, 150),
    "OFFSET3": (-1, 1),
    "PAN3": (-1, 1),
    "WAVE3": (1, 6),
    "RATIO": (0, 7),
    "RATIO1": (0, 7),
    "INDEX1": (0, 10),
    "RATIO2": (0, 7),
    "INDEX2": (0, 10),
    "RATIO3": (0, 7),
    "INDEX3": (0, 10),
    "SEMITONES": (-24, 24),
    "A(vol)": (0, 6000),  
    "D(vol)": (0, 6000),
    "S(vol)": (0, 1),
    "R(vol)": (0, 10000),
    "A(fm)": (0, 6000),
    "D(fm)": (0, 6000),
    "S(fm)": (0, 1),
    "R(fm)": (0, 10000),
    "A(cut)": (0, 6000),
    "D(cut)": (0, 6000),
    "S(cut)": (0, 1),
    "R(cut)": (0, 10000),
    "RATE": (0, 250),
    "COLOR(ph)": (0, 100),
    "FB(fl)": (0, 100),
    "COLOR(les)": (0, 100),
    "CHORUS": (0, 100),
    "VIBRATO": (0, 100),
    "FLANGER": (0, 100),
    "PHASER": (0, 100),
    "PWM": (0, 100),
    "LESLIE": (0, 100),
    "CUT(mod)": (0, 20000),  # Updated range for CUT(mod) to avoid conflict with CUTOFF
    "MIX": (0, 100),
    "MIDS": (-1, 1),
    "EQ(pre)": (-1, 1),  
    "EQ(in)": (-1, 1),
    "REVERB": (0, 100),
    "TIME": (0, 100),
    "VOL(pitch)": (0, 100),
    "PITCH": (-12, 12),
    "FEEDBACK": (0, 100),
    "SUBDIVISION": (0, 16),
    "SPREAD": (0, 100),
    "SPECTRAL": (0, 100), 
    "fPITCH": (-12, 12),
    "fTIME": (0, 10000),
    "SMOOTHf": (0, 100), 
    "spectralLFO": (0, 100), 
}

json_addresses = {

# OSC1 Parameters
"VOL1": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC", "CONTENTS", "OSC1vol", "VALUE"],
"OFFSET1": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.4", "CONTENTS", "OSC1offset", "VALUE"],
"PAN1": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.5", "CONTENTS", "OSC1pan", "VALUE"],
"WAVE1": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.8", "CONTENTS", "OSC1wave", "VALUE"],

# OSC2 Parameters
"VOL2": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.11", "CONTENTS", "OSC2vol", "VALUE"],
"OFFSET2": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.12", "CONTENTS", "OSC2offset", "VALUE"],
"PAN2": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.13", "CONTENTS", "OSC2pan", "VALUE"],
"WAVE2": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.14", "CONTENTS", "OSC2wave", "VALUE"],

# OSC3 Parameters
"VOL3": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.17", "CONTENTS", "OSC3vol", "VALUE"],
"OFFSET3": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.18", "CONTENTS", "OSC3offset", "VALUE"],
"PAN3": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.19", "CONTENTS", "OSC3pan", "VALUE"],
"WAVE3": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.20", "CONTENTS", "OSC3wave", "VALUE"],

# FM1 Parameters
"RATIO1": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.9", "CONTENTS", "ratio1", "VALUE"],
"INDEX1": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.10", "CONTENTS", "index1", "VALUE"],

# FM2 Parameters
"RATIO2": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.15", "CONTENTS", "ratio2", "VALUE"],
"INDEX2": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.16", "CONTENTS", "index2", "VALUE"],

# FM3 Parameters
"RATIO3": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.21", "CONTENTS", "ratio3", "VALUE"],
"INDEX3": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.22", "CONTENTS", "index3", "VALUE"],

# General controls
"VOLUME": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.23", "CONTENTS", "VOL", "VALUE"],
"BPM": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.24", "CONTENTS", "BPM", "VALUE"],
"PROBABILITY": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.25", "CONTENTS", "oddsTRIG", "VALUE"],
"spreadNOTEZ": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.26", "CONTENTS", "spreadNOTEZ", "VALUE"],

# OFFSET/CUT
"SEMITONES": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.27", "CONTENTS", "offset", "VALUE"],
"CENTS": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.28", "CONTENTS", "detune", "VALUE"],
"CUTOFF": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.29", "CONTENTS", "CUTOFF", "VALUE"],
"PEAK": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.30", "CONTENTS", "resonance", "VALUE"],

# ADSR
"A(vol)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.31", "CONTENTS", "attack", "VALUE"],
"D(vol)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.32", "CONTENTS", "decay", "VALUE"],
"S(vol)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.33", "CONTENTS", "sustain", "VALUE"],
"R(vol)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.34", "CONTENTS", "release", "VALUE"],

# FM ADSR
"A(fm)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.35", "CONTENTS", "attackFM", "VALUE"],
"D(fm)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.36", "CONTENTS", "decayFM", "VALUE"],
"S(fm)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.37", "CONTENTS", "sustainFM", "VALUE"],
"R(fm)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.38", "CONTENTS", "releaseFM", "VALUE"],

# CUTOFF ADSR
"A(cut)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.39", "CONTENTS", "attackCUT", "VALUE"],
"D(cut)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.40", "CONTENTS", "decayCUT", "VALUE"],
"S(cut)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.41", "CONTENTS", "sustainCUT", "VALUE"],
"R(cut)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.42", "CONTENTS", "releaseCUT", "VALUE"],

# Modulation Control (MOD_CONTROL)
"RATE": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.43", "CONTENTS", "rateE", "VALUE"],
"COLOR(ph)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.44", "CONTENTS", "colorTonePHASER", "VALUE"],
"FB(fl)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.45", "CONTENTS", "feedbackFLANGE", "VALUE"],
"COLOR(les)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.46", "CONTENTS", "colorLES", "VALUE"],

# Modulation Amount (MOD_AMOUNT)
"CHORUS": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.47", "CONTENTS", "depthCHOR", "VALUE"],
"VIBRATO": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.48", "CONTENTS", "depthVIB", "VALUE"],
"FLANGER": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.49", "CONTENTS", "depthFL", "VALUE"],
"PHASER": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.50", "CONTENTS", "depthPH", "VALUE"],

# FREEZE controls
"SPECTRAL": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.51", "CONTENTS", "freezeMIX", "VALUE"],
"fPITCH": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.52", "CONTENTS", "transp", "VALUE"],
"fTIME": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.53", "CONTENTS", "spectralTIME", "VALUE"],
"SMOOTHf": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.54", "CONTENTS", "smooth", "VALUE"],

# More Modulation Amount (MORE_MOD_AMOUNT)
"PWM": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.55", "CONTENTS", "PWm", "VALUE"],
"LESLIE": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.56", "CONTENTS", "mixLES", "VALUE"],
"CUT(mod)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.57", "CONTENTS", "CUTOFFLFO", "VALUE"],
"SPEC(mod)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.58", "CONTENTS", "spectralLFO", "VALUE"],

# DISTORTION controls
"DISTORTION": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.59", "CONTENTS", "DISmix", "VALUE"],
"EQ(pre)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.61", "CONTENTS", "DISTlowHigh_PRE", "VALUE"],
"MIDS": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.62", "CONTENTS", "DISmid", "VALUE"],
"EQ(in)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.63", "CONTENTS", "DISTlowHigh_IN", "VALUE"],

# VERB controls
"REVERB": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.64", "CONTENTS", "VERBmix", "VALUE"],
"TIME": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.65", "CONTENTS", "verbTIME", "VALUE"],
"PITCH": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.66", "CONTENTS", "VERBpitch", "VALUE"],
"VOL(pitch)": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.67", "CONTENTS", "octvol", "VALUE"],

# DELAY controls
"DELAY": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.71", "CONTENTS", "DELmix", "VALUE"],
"FEEDBACK": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.69", "CONTENTS", "DELregen", "VALUE"],
"SUBDIVISION": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.68", "CONTENTS", "DELfreak", "VALUE"],
"SPREAD": ["CONTENTS", "rnbo", "CONTENTS", "inst", "CONTENTS", "0", "CONTENTS", "params", "CONTENTS", "MIDI_ENC.70", "CONTENTS", "DELmod", "VALUE"],

}

# Updated specific names for each encoder in each menu
encoder_names = [
    ["VOL1", "OFFSET1", "PAN1", "WAVE1"],  # OSC1
    ["VOL2", "OFFSET2", "PAN2", "WAVE2"],  # OSC2
    ["VOL3", "OFFSET3", "PAN3", "WAVE3"],  # OSC3
    ["RATIO1", "INDEX1", "N/A", "N/A"],  # FM1
    ["RATIO2", "INDEX2", "N/A", "N/A"],  # FM2
    ["RATIO3", "INDEX3", "N/A", "N/A"],  # FM3
    ["VOLUME", "BPM", "PROBABILITY", "spreadNOTEZ"],  # GENERAL (Updated with spreadNOTEZ)
    ["SEMITONES", "CENTS", "CUTOFF", "PEAK"],  # OFFSET_CUTOFF
    ["A(vol)", "D(vol)", "S(vol)", "R(vol)"],  # VOLUME_ADSR
    ["A(fm)", "D(fm)", "S(fm)", "R(fm)"],  # FM_ADSR
    ["A(cut)", "D(cut)", "S(cut)", "R(cut)"],  # CUTOFF_ADSR
    ["RATE", "COLOR(ph)", "FB(fl)", "COLOR(les)"],  # MOD_CONTROL
    ["CHORUS", "VIBRATO", "FLANGER", "PHASER"],  # MOD_AMOUNT
    ["PWM", "LESLIE", "CUT(mod)", "SPEC(mod)"],  # MORE_MOD_AMOUNT
    ["DISTORTION", "EQ(pre)", "MIDS", "EQ(in)"],  # DISTORTION
    ["REVERB", "TIME", "PITCH", "VOL(pitch)"],  # VERB
    ["DELAY", "REGEN", "FREAK", "MOD"],  # DELAY
    ["SPECTRAL", "fPITCH", "fTIME", "SMOOTHf"],  # freeze
]

# List of function titles for each menu
menu_titles = [
    "OSC1", "OSC2", "OSC3", "FM1", "FM2", "FM3", "GENERAL", "OFFSET_CUTOFF",
    "VOLUME_ADSR", "FM_ADSR", "CUTOFF_ADSR", "MOD_CONTROL",
    "MOD_AMOUNT", "MORE_MOD_AMOUNT", "DISTORTION", "VERB",
    "DELAY", "SPECTRAL"
]

menu_map = {
    "FM1_tog": "FM1",
    "FM2_tog": "FM2",
    "FM3_tog": "FM3",
    "OSC1_tog": "OSC1",
    "OSC2_tog": "OSC2",
    "OSC3_tog": "OSC3",
    "GENERAL_tog": "GENERAL",
    "OFFSET_CUT_tog": "OFFSET_CUTOFF",
    "MOD_CONTROL_tog": "MOD_CONTROL",
    "MOD_AMOUNT_tog": "MOD_AMOUNT",
    "MORE_MOD_AMOUNT_tog": "MORE_MOD_AMOUNT",
    "DISTORTION_tog": "DISTORTION",
    "DELAY_tog": "DELAY",
    "VERB_tog": "VERB",
    "VOL(env)_tog": "VOLUME_ADSR",
    "FM(env)_tog": "FM_ADSR",
    "CUTOFF(env)_tog": "CUTOFF_ADSR",
    "SPECTRAL_tog": "SPECTRAL"
}

menu_osc_addresses = {
    "OSC1": 0,
    "OSC2": 1,
    "OSC3": 2,
    "FM1": 3,
    "FM2": 4,
    "FM3": 5,
    "GENERAL": 6,
    "OFFSET_CUTOFF": 7,
    "VOLUME_ADSR": 8,
    "FM_ADSR": 9,
    "CUTOFF_ADSR": 10,
    "MOD_CONTROL": 11,
    "MOD_AMOUNT": 12,
    "MORE_MOD_AMOUNT": 13,
    "DISTORTION": 14,
    "VERB": 15,
    "DELAY": 16,
    "SPECTRAL": 17
}


class CircularSlider(BoxLayout):
    value = NumericProperty(0)

    def __init__(self, name, json_key, min_value=0, max_value=127, **kwargs):
        super(CircularSlider, self).__init__(**kwargs)
        self.orientation = 'vertical'
        self.json_key = json_key  # Store the JSON key path

        # Label above slider
        self.label = Label(text=name, font_size=22, color=(1, 1, 1, 1))
        # Value label below slider
        self.value_label = Label(text=f"{self.value:.1f}", font_size=40, color=(1, 1, 1, 1))

        self.slider = Slider(min=min_value, max=max_value, value=self.value,
                             orientation='vertical', size_hint_y=None, height=380)
        self.slider.bind(value=self.on_slider_value_change)

        self.add_widget(self.label)
        self.add_widget(self.slider)
        self.add_widget(self.value_label)

    def on_slider_value_change(self, instance, value):
        self.value = value
        self.value_label.text = f"{value:.1f}"

    def update_value(self, value):
        self.slider.value = value
        self.value_label.text = f"{value:.1f}"

class UI(App):
    def build(self):
        self.layout = BoxLayout(orientation='vertical')
        

        with self.layout.canvas.before:
            Color(0.12, 0, 0.12, 1)
            self.rect = Rectangle(size=Window.size, pos=self.layout.pos)
            self.layout.bind(size=self._update_rect, pos=self._update_rect)

        self.menu_title_label = Label(text='', font_size=50, color=(1, 1, 1, 1), size_hint_y=None, height=60)
        self.encoder_layout = GridLayout(cols=4, padding=20, spacing=40, size_hint=(1, 0.9))

        self.encoder_values = [[0] * 4 for _ in range(len(encoder_names))]

        self.encoders = []
        for i in range(len(encoder_names)):
            menu_encoders = []
            for j, name in enumerate(encoder_names[i]):
                min_value = 0
                max_value = 127
                if name in parameter_ranges:
                    min_value, max_value = parameter_ranges[name]
                json_key = self.get_json_key(name, menu_titles[i])
                encoder = CircularSlider(name, json_key, min_value=min_value, max_value=max_value)
                menu_encoders.append(encoder)
            self.encoders.append(menu_encoders)
        self.current_menu = 0

        self.layout.add_widget(self.menu_title_label)
        self.layout.add_widget(self.encoder_layout)

        self.update_encoders()

        # Start the data fetching thread
        threading.Thread(target=self.data_fetcher, daemon=True).start()

        # Set the application to full-screen mode
        Window.fullscreen = True
        
        # Hide the mouse cursor
        Window.show_cursor = False

        return self.layout

    def _update_rect(self, instance, value):
        self.rect.size = instance.size
        self.rect.pos = instance.pos

    def switch_menu(self, menu_index):
        # Save current slider values
        for i, encoder in enumerate(self.encoders[self.current_menu]):
            self.encoder_values[self.current_menu][i] = encoder.value

        self.current_menu = menu_index
        Clock.schedule_once(self.update_encoders)

    def update_encoders(self, *args):
        self.encoder_layout.clear_widgets()
        self.menu_title_label.text = menu_titles[self.current_menu]
        for i in range(4):
            encoder = self.encoders[self.current_menu][i]
            encoder.value = self.encoder_values[self.current_menu][i]
            encoder.update_value(encoder.value)
            self.encoder_layout.add_widget(encoder)

    def get_json_key(self, name, menu_title):
        return json_addresses.get(name)

    def data_fetcher(self):
        url = "http://127.0.0.1:5678/"  # Update if necessary
        #url = "http://192.168.8.233:5678/"
        


        while True:
            try:
                response = requests.get(url)
                response.raise_for_status()
                data = response.json()
                self.update_from_json(data)
                self.handle_radio_button(data)  # Check for menu switch using RADIO_BUTTON_MATRIX
            except Exception as e:
                print(f"Error fetching data: {e}")
            sleep(0.1)

    def update_from_json(self, data):
        for i, menu_encoders in enumerate(self.encoders):
            for j, encoder in enumerate(menu_encoders):
                json_path = encoder.json_key
                if json_path:
                    value = self.get_value_from_json(data, json_path)
                    if value is not None:
                        encoder.update_value(value)
                        self.encoder_values[i][j] = value

    def get_value_from_json(self, data, path):
        try:
            for key in path:
                data = data[key]
            return data
        except KeyError:
            return None

    def handle_radio_button(self, data):
        try:
            # Access the RADIO_BUTTONS_MATRIX
            radio_buttons = data["CONTENTS"]["rnbo"]["CONTENTS"]["inst"]["CONTENTS"]["0"]["CONTENTS"]["params"]["CONTENTS"]["RADIO_BUTTONS_MATRIX"]["CONTENTS"]

            # Loop through each parameter in the RADIO_BUTTONS_MATRIX
            for param, details in radio_buttons.items():
                # Access the top-level "VALUE" key
                value = details.get("VALUE", 0)
                
                # Check if the value is 1 and the param is in the menu_map
                if value == 1 and param in menu_map:
                    menu_name = menu_map[param]
                    menu_index = menu_osc_addresses.get(menu_name)
                    
                    # Switch to the appropriate menu if menu_index is found
                    if menu_index is not None:
                        self.switch_menu(menu_index)

        except KeyError as e:
            print(f"Error accessing RADIO_BUTTONS_MATRIX: {e}")



if __name__ == "__main__":
    UI().run()