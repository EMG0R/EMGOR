import requests
from kivy.app import App
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.slider import Slider
from kivy.uix.label import Label
from kivy.core.window import Window
from kivy.clock import Clock
from kivy.graphics import Color, Rectangle
from time import sleep
import threading
from pythonosc.udp_client import SimpleUDPClient

# Configuration
OSC_ADDRESS = "GORPI3.local"  # Replace with your Pi's hostname if different
OSC_PORT = 1234  # OSC UDP port for RNBO runner

# Set window size
Window.size = (300, 400) #edit window sizing and slider sizes to fit your pi or laptop display
Window.fullscreen = False
Window.show_cursor = True

class ParamSlider(BoxLayout):
    """
    A vertical layout containing a label, a slider, and a value display.
    Allows users to control a parameter and sends the updated value to the Pi.
    """
    def __init__(self, param_name, min_val, max_val, send_func, **kwargs):
        super(ParamSlider, self).__init__(**kwargs)
        self.orientation = 'vertical'
        self.param_name = param_name
        self.send_func = send_func

        # Label above the slider
        self.label = Label(
            text=param_name.upper(),
            font_size=40,
            color=(1, 1, 1, 1),
            size_hint_y=None,
            height=50
        )
        self.add_widget(self.label)

        # Vertical slider to control the parameter
        self.slider = Slider(
            min=min_val,
            max=max_val,
            value=min_val,
            orientation='vertical',
            size_hint=(1, None),
            height=600
        )
        self.slider.bind(value=self.on_slider_value_change)
        self.add_widget(self.slider)

        # Display the current slider value below the slider
        self.value_label = Label(
            text=f"{min_val:.3f}",
            font_size=30,
            color=(1, 1, 1, 1),
            size_hint_y=None,
            height=50
        )
        self.add_widget(self.value_label)

    def on_slider_value_change(self, instance, value):
        """
        Called whenever the slider's value changes.
        Updates the label and sends the new value to the Pi.
        """
        self.value_label.text = f"{value:.3f}"
        self.send_func(self.param_name, value)

    def update_value(self, val):
        """
        Update the slider and value label without triggering an OSC message.
        Used when the value changes externally (e.g., from the Pi).
        """
        self.slider.value = val
        self.value_label.text = f"{val:.3f}"


class MyApp(App):
    """
    Main application class for the Kivy UI.
    Handles the layout, OSC communication, and data fetching.
    """
    def build(self):
        # Main layout for the app
        self.layout = BoxLayout(orientation='vertical', padding=20, spacing=20)

        # Background color setup
        with self.layout.canvas.before:
            Color(0.12, 0, 0.12, 1)  # Purple background
            self.rect = Rectangle(size=Window.size, pos=self.layout.pos)
            self.layout.bind(size=self._update_rect, pos=self._update_rect)

        # Title at the top
        self.title_label = Label(
            text="PYTHON_EXAMPLE",
            font_size=50,
            color=(1, 1, 1, 1),
            size_hint_y=None,
            height=80
        )
        self.layout.add_widget(self.title_label)

        # OSC client to send messages to the Pi
        self.osc_client = SimpleUDPClient(OSC_ADDRESS, OSC_PORT)

        # Create sliders for "ratio" and "index" parameters
        sliders_box = BoxLayout(orientation='horizontal', spacing=100, size_hint=(1, None), height=700)
        self.ratio_slider = ParamSlider("ratio", 1, 100, self.send_param)
        self.index_slider = ParamSlider("index", 1, 20, self.send_param)

        sliders_box.add_widget(self.ratio_slider)
        sliders_box.add_widget(self.index_slider)
        self.layout.add_widget(sliders_box)

        # Start a thread to fetch data from the Pi
        threading.Thread(target=self.data_fetcher, daemon=True).start()

        return self.layout

    def _update_rect(self, instance, value):
        """
        Updates the background rectangle size and position when the window is resized.
        """
        self.rect.size = instance.size
        self.rect.pos = instance.pos

    def data_fetcher(self):
        """
        Continuously fetches parameter data from the Pi's RNBO runner via HTTP.
        Updates the sliders based on the fetched data.
        """
        # Uncomment the appropriate URL based on where the RNBO runner is hosted
        # url = "http://127.0.0.1:5678/"  # Use this when running on the Pi itself
        # url = "http://[YOUR_PI_NAME].local:5678/"  # Replace with your Pi's hostname
        url = "http://GORPI3.local:5678/"  # Example remote Pi hostname

        while True:
            try:
                # Fetch the JSON data from the RNBO runner
                response = requests.get(url)
                response.raise_for_status()
                data = response.json()

                # Navigate to the parameters in the JSON structure
                params = data["CONTENTS"]["rnbo"]["CONTENTS"]["inst"]["CONTENTS"]["0"]["CONTENTS"]["params"]["CONTENTS"]

                # Extract the "ratio" and "index" parameter values
                ratio_val = params["ratio"]["VALUE"]
                index_val = params["index"]["VALUE"]

                # Update the sliders on the main thread
                Clock.schedule_once(lambda dt: self.update_sliders(ratio_val, index_val))
            except Exception as e:
                print(f"Error fetching data: {e}")  # Log errors for debugging
            sleep(0.1)  # Fetch data every 0.1 seconds

    def update_sliders(self, ratio_val, index_val):
        """
        Updates the sliders to reflect the current parameter values.
        Called when new data is fetched from the Pi.
        """
        self.ratio_slider.update_value(ratio_val)  # Update the ratio slider
        self.index_slider.update_value(index_val)  # Update the index slider

    def send_param(self, param_name, value):
        """
        Sends an OSC message to the Pi to update a parameter.
        """
        # Construct the OSC address for the parameter
        address = f"/rnbo/inst/0/params/{param_name}"
        self.osc_client.send_message(address, float(value))  # Send the OSC message


if __name__ == "__main__":
    MyApp().run()