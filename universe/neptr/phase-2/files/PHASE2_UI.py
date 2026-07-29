from kivy.app import App
from kivy.uix.widget import Widget
from kivy.uix.label import Label
from kivy.uix.floatlayout import FloatLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.slider import Slider
from kivy.uix.image import Image
from kivy.graphics import Color, Ellipse, Rectangle, Line
from kivy.clock import Clock
import random
import math
from kivy.core.window import Window
import subprocess
import time
from pythonosc.dispatcher import Dispatcher
from pythonosc.osc_server import ThreadingOSCUDPServer
import threading
import logging

logging.basicConfig(level=logging.INFO)

Window.size = (720, 1900)


class Circle:
    def __init__(self, x, y, velocity_x, velocity_y, radius, color, lifetime):
        self.x = x
        self.y = y
        self.velocity_x = velocity_x
        self.velocity_y = velocity_y
        self.radius = radius
        self.color = color
        self.lifetime = lifetime


class LavaLampEffect(Widget):
    def __init__(self, state_label_height, **kwargs):
        super().__init__(**kwargs)
        self.current_color = (0.11, 0.0, 0.12, 1)
        self.state_label_height = state_label_height

        with self.canvas:
            Color(*self.current_color)
            self.rect = Rectangle(pos=self.pos, size=self.size)

        self.bind(pos=self.update_rect, size=self.update_rect)
        self.circles = []
        Clock.schedule_once(self.init_create, 0)
        Clock.schedule_interval(self.update_circles, 1 / 20)  # Reduced to 30 FPS for efficiency

    def update_rect(self, *args):
        self.rect.pos = self.pos
        self.rect.size = self.size

    def random_color(self):
        r = random.uniform(0.05, 0.25)
        g = random.uniform(0.05, 0.15)
        b = random.uniform(0.3, 0.5)
        a = random.uniform(0.4, 0.6)
        return [r, g, b, a]

    def init_create(self, dt):
        for _ in range(random.randint(2, 3)):  # Reduced circle count for efficiency
            self.create_circle(0)
        self.schedule_next_create()

    def schedule_next_create(self):
        Clock.schedule_once(self.create_circle, random.uniform(2, 5))

    def create_circle(self, dt):
        x = random.uniform(0, self.width)
        y = random.uniform(self.height * 0.3, self.height - self.state_label_height - (self.height * 0.1))
        angle = random.uniform(0, 2 * math.pi)
        speed = random.uniform(100, 300)
        velocity_x = math.cos(angle) * speed
        velocity_y = math.sin(angle) * speed
        radius = random.randint(50, 180)
        color = self.random_color()
        lifetime = random.uniform(5, 20)

        self.circles.append(Circle(x, y, velocity_x, velocity_y, radius, color, lifetime))
        self.schedule_next_create()

    def update_circles(self, dt):
        self.canvas.after.clear()
        to_remove = []
        bottom_line = self.height * 0.3
        top_line = self.height - self.state_label_height - (self.height * 0.1)

        for circle in self.circles:
            circle.x += circle.velocity_x * dt
            circle.y += circle.velocity_y * dt

            if circle.x - circle.radius < 0 or circle.x + circle.radius > self.width:
                circle.velocity_x *= -1
            if circle.y - circle.radius < bottom_line or circle.y + circle.radius > top_line:
                circle.velocity_y *= -1

            circle.lifetime -= dt
            if circle.lifetime <= 0:
                to_remove.append(circle)
                continue

            with self.canvas.after:
                Color(*circle.color)
                Ellipse(pos=(circle.x - circle.radius, circle.y - circle.radius),
                        size=(circle.radius * 2, circle.radius * 2))

        for circle in to_remove:
            self.circles.remove(circle)


class CircularSlider(BoxLayout):
    def __init__(self, name, osc_addr, min_value, max_value, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'
        self.osc_addr = osc_addr
        self.size_hint_x = None
        self.width = 400  # Increased width for much fatter sliders

        self.label = Label(text=name, font_size=26, color=(1, 1, 1, 1))
        self.value_label = Label(text="0.0", font_size=56, color=(1, 1, 1, 1))
        self.slider = Slider(min=min_value, max=max_value, value=0, orientation='vertical',
                            size_hint_y=None, height=760, disabled=True)

        self.add_widget(self.label)
        self.add_widget(self.slider)
        self.add_widget(self.value_label)

    def update_value(self, value):
        self.slider.value = value
        self.value_label.text = f"{value:.2f}"


class NeutralScreen(Widget):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        with self.canvas:
            Color(0.109, 0.0, 0.119, 1)
            self.rect = Rectangle(pos=self.pos, size=self.size)

        self.bind(pos=self.update_rect, size=self.update_rect)

        self.face_image = Image(
            source="/home/pi/protoFACES/FACE1.png",
            size_hint=(None, None),
            size=(720, 720),
            pos_hint={'center_x': 0.5, 'y': 0.884}
        )
        self.add_widget(self.face_image)
        self.face2_duration = 0.4  # Duration to show face2.png
        self.face2_probability = 0.15  # 10% chance to switch to face2
        self.other_face_duration = 2.5  # Duration to show face3-7.png
        self.other_face_probability = 0.16  # 10% chance to switch to face3-7
        self.other_faces = [f"/home/pi/protoFACES/FACE{i}.png" for i in range(3, 8)]
        self.face_switch_event = None
        self.is_showing_other_face = False

    def update_rect(self, *args):
        self.rect.pos = self.pos
        self.rect.size = self.size
        self.face_image.pos = (self.width / 2 - self.face_image.width / 2, self.height * 0.884 - self.face_image.height)

    def start_face_switching(self):
        self.face_switch_event = Clock.schedule_interval(self.try_switch_face, 1.0)

    def stop_face_switching(self):
        if self.face_switch_event:
            self.face_switch_event.cancel()
            self.face_switch_event = None
            self.face_image.source = "/home/pi/protoFACES/FACE1.png"
            self.is_showing_other_face = False

    def try_switch_face(self, dt):
        if self.is_showing_other_face:
            return  # Skip if currently showing face3-7

        # Check for other face (face3-7) switch
        if random.random() < self.other_face_probability:
            self.is_showing_other_face = True
            self.face_image.source = random.choice(self.other_faces)
            Clock.schedule_once(self.revert_to_face1, self.other_face_duration)
        # Check for face2 switch
        elif random.random() < self.face2_probability:
            self.face_image.source = "/home/pi/protoFACES/FACE2.png"
            Clock.schedule_once(self.revert_to_face1, self.face2_duration)

    def revert_to_face1(self, dt):
        self.face_image.source = "/home/pi/protoFACES/FACE1.png"
        self.is_showing_other_face = False


class LavaLampApp(App):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.current_effect = None
        self.param_values = {}
        self.last_osc_time = time.time()
        self.is_sleeping = False
        self.effect_pages = {
            'automoog': [('sens', '/automoog_sens', 0, 1)],
            'mood': [
                ('mix', '/mood_mix', 0, 1),
                ('clock', '/mood_clock', 0, 1),
                ('dens', '/mood_dens', 0, 1400),
                ('dly', '/mood_dly', 0, 6)
            ],
            'ps': [
                ('semi', '/semi', -12, 12),
                ('dry pct', '/dry_pct_ps', 0, 100)
            ],
            'dist': [
                ('amt', '/dist_amt', 0.01, 1),
                ('bits', '/bits_bc', 1, 16)
            ],
            'delay': [
                ('ms', '/delay_ms', 0, 700000),
                ('semi', '/semi_del', -5, 12),
                ('feedback', '/feedback', 0, 1.2),
                ('mix', '/del_mix', 0, 1)
            ],
            'volpedal': [('value', '/vol_pedal', 0, 1)],
            'rm': [('mod freq', '/ar_mod_freq', 1.25, 250)],
            'verb': [
                ('mix', '/verb_mix', 0, 1),
                ('fb', '/reverb_fb', 0, 0.999)
            ],
            'specdel': [
                ('max del', '/spec_del_max_del', 0, 1),
                ('fb', '/spec_del_fb', 0, 1),
                ('mix', '/spec_del_mix', 0, 1)
            ],
            'stut': [
                ('chance', '/stut_chance', 0, 1),
                ('bpm', '/stut_bpm', 40, 400),
                ('drywet', '/stut_drywet', 0, 1)
            ],
            'gps': [
                ('semis', '/gps_semis', -12, 12),
                ('fback', '/gps_fback', 0, 0.99),
                ('dly', '/gps_dly', 0.001, 0.1),
                ('mix', '/gps_mix', 0, 1)
            ],
            'gran': [('mix', '/gran_mix', 0, 1)],
            'ts': [('amt acc', '/ts_amt_acc', 0, 1)],
            'chorus': [
                ('rate', '/chorus_rate', 0.001, 7),
                ('depth', '/chorus_depth', 0, 1),
                ('mix', '/chorus_mix', 0, 1)
            ],
            'vib': [
                ('freq', '/vib_freq', 0.001, 10),
                ('depth', '/vib_depth', 0, 0.03)
            ],
            'fla': [
                ('rate', '/fla_rate', 0.001, 40),
                ('depth', '/fla_depth', 0, 0.01),
                ('fback', '/fla_fback', -1, 1)
            ],
            'pha': [
                ('rate', '/pha_rate', 0.001, 40),
                ('depth', '/pha_depth', 0, 1),
                ('fback', '/pha_fback', 0, 1),
                ('stages', '/pha_stages', 1, 24)
            ],
            'leslie': [
                ('speed', '/leslie_speed', 0.1, 10),
                ('depth', '/leslie_depth', 0, 1)
            ],
            'formant': [('shift', '/formant_shift', -12, 12)],
            'attk': [
                ('sens', '/attk_sens', 0.1, 1),
                ('hold ms', '/attk_hold_ms', 100, 2000)
            ],
            'dropouts': [
                ('amount', '/dropout_amount', 0, 100),
                ('depth', '/dropout_depth', 0, 1),
                ('smoothing', '/dropout_smoothing', 0, 1)
            ]
        }

    def build(self):
        Window.fullscreen = False
        Window.show_cursor = False

        self.root = FloatLayout()

        self.state_label = Label(text="[b]AUTOMOOG[/b]", font_size=50, markup=True, color=(0, 0, 0, 1),
                                 size_hint=(None, None), pos_hint={'center_x': 0.5, 'top': 1})
        self.state_label.halign = 'center'
        self.state_label.valign = 'middle'
        self.state_label.texture_update()
        state_label_height = self.state_label.texture_size[1]

        self.lava_effect = LavaLampEffect(state_label_height=state_label_height, size_hint=(1, 1), pos=(0, 0))
        self.neutral_screen = NeutralScreen(size_hint=(1, 1), pos=(0, 0))

        self.root.add_widget(self.lava_effect)
        with self.lava_effect.canvas.after:
            Color(1, 1, 1, 1)
            self.divider = Line(points=[0, Window.height - state_label_height - (Window.height * 0.1),
                                       Window.width, Window.height - state_label_height - (Window.height * 0.1)], width=1)
        self.root.add_widget(self.state_label)

        self.slider_grid = GridLayout(cols=4, padding=20, spacing=40, size_hint=(None, None), pos_hint={'center_x': 0.5})
        self.root.add_widget(self.slider_grid)

        self.csd_path = "/Users/emgor/Desktop/EMGOR_SYNTH/______PHASE2/NEPTR_PHASE2_PIinto.csd"
        self.launch_csound()
        self.setup_osc_listener()
        Clock.schedule_interval(self.update_state, 1 / 30)
        Clock.schedule_interval(self.check_sleep, 1)
        Window.bind(size=self.update_label_pos)

        return self.root

    def launch_csound(self):
        self.cs_process = subprocess.Popen(
            ['csound', self.csd_path, '-+rtaudio=coreaudio', '-odac', '-iadc', '-b256', '-B512', '--daemon'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        time.sleep(1)

    def setup_osc_listener(self):
        self.dispatcher = Dispatcher()
        self.dispatcher.map("/automoog_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('automoog', val)))
        self.dispatcher.map("/mood_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('mood', val)))
        self.dispatcher.map("/ps_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('ps', val)))
        self.dispatcher.map("/dist_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('dist', val)))
        self.dispatcher.map("/delay_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('delay', val)))
        self.dispatcher.map("/volpedal_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('volpedal', val)))
        self.dispatcher.map("/rm_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('rm', val)))
        self.dispatcher.map("/verb_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('verb', val)))
        self.dispatcher.map("/specdel_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('specdel', val)))
        self.dispatcher.map("/stut_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('stut', val)))
        self.dispatcher.map("/gps_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('gps', val)))
        self.dispatcher.map("/gran_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('gran', val)))
        self.dispatcher.map("/ts_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('ts', val)))
        self.dispatcher.map("/chorus_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('chorus', val)))
        self.dispatcher.map("/vib_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('vib', val)))
        self.dispatcher.map("/fla_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('fla', val)))
        self.dispatcher.map("/pha_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('pha', val)))
        self.dispatcher.map("/leslie_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('leslie', val)))
        self.dispatcher.map("/formant_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('formant', val)))
        self.dispatcher.map("/attk_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('attk', val)))
        self.dispatcher.map("/dropout_state", lambda addr, val: Clock.schedule_once(lambda dt: self.handle_toggle('dropouts', val)))

        self.dispatcher.map("/automoog_sens", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/automoog_sens', val)))
        self.dispatcher.map("/mood_mix", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/mood_mix', val)))
        self.dispatcher.map("/mood_clock", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/mood_clock', val)))
        self.dispatcher.map("/mood_dens", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/mood_dens', val)))
        self.dispatcher.map("/mood_dly", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/mood_dly', val)))
        self.dispatcher.map("/semi", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/semi', val)))
        self.dispatcher.map("/dry_pct_ps", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/dry_pct_ps', val)))
        self.dispatcher.map("/dist_amt", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/dist_amt', val)))
        self.dispatcher.map("/delay_ms", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/delay_ms', val)))
        self.dispatcher.map("/semi_del", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/semi_del', val)))
        self.dispatcher.map("/feedback", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/feedback', val)))
        self.dispatcher.map("/del_mix", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/del_mix', val)))
        self.dispatcher.map("/vol_pedal", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/vol_pedal', val)))
        self.dispatcher.map("/ar_mod_freq", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/ar_mod_freq', val)))
        self.dispatcher.map("/verb_mix", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/verb_mix', val)))
        self.dispatcher.map("/reverb_fb", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/reverb_fb', val)))
        self.dispatcher.map("/spec_del_max_del", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/spec_del_max_del', val)))
        self.dispatcher.map("/spec_del_fb", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/spec_del_fb', val)))
        self.dispatcher.map("/spec_del_mix", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/spec_del_mix', val)))
        self.dispatcher.map("/stut_chance", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/stut_chance', val)))
        self.dispatcher.map("/stut_bpm", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/stut_bpm', val)))
        self.dispatcher.map("/stut_drywet", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/stut_drywet', val)))
        self.dispatcher.map("/gps_semis", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/gps_semis', val)))
        self.dispatcher.map("/gps_fback", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/gps_fback', val)))
        self.dispatcher.map("/gps_dly", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/gps_dly', val)))
        self.dispatcher.map("/gps_mix", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/gps_mix', val)))
        self.dispatcher.map("/gran_mix", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/gran_mix', val)))
        self.dispatcher.map("/ts_amt_acc", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/ts_amt_acc', val)))
        self.dispatcher.map("/chorus_rate", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/chorus_rate', val)))
        self.dispatcher.map("/chorus_depth", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/chorus_depth', val)))
        self.dispatcher.map("/chorus_mix", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/chorus_mix', val)))
        self.dispatcher.map("/vib_freq", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/vib_freq', val)))
        self.dispatcher.map("/vib_depth", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/vib_depth', val)))
        self.dispatcher.map("/fla_rate", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/fla_rate', val)))
        self.dispatcher.map("/fla_depth", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/fla_depth', val)))
        self.dispatcher.map("/fla_fback", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/fla_fback', val)))
        self.dispatcher.map("/pha_rate", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/pha_rate', val)))
        self.dispatcher.map("/pha_depth", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/pha_depth', val)))
        self.dispatcher.map("/pha_fback", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/pha_fback', val)))
        self.dispatcher.map("/pha_stages", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/pha_stages', val)))
        self.dispatcher.map("/leslie_speed", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/leslie_speed', val)))
        self.dispatcher.map("/leslie_depth", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/leslie_depth', val)))
        self.dispatcher.map("/formant_shift", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/formant_shift', val)))
        self.dispatcher.map("/attk_sens", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/attk_sens', val)))
        self.dispatcher.map("/attk_hold_ms", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/attk_hold_ms', val)))
        self.dispatcher.map("/bits_bc", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/bits_bc', val)))
        self.dispatcher.map("/dropout_amount", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/dropout_amount', val)))
        self.dispatcher.map("/dropout_depth", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/dropout_depth', val)))
        self.dispatcher.map("/dropout_smoothing", lambda addr, val: Clock.schedule_once(lambda dt: self.update_param('/dropout_smoothing', val)))

        self.osc_server = ThreadingOSCUDPServer(("127.0.0.1", 8000), self.dispatcher)
        self.server_thread = threading.Thread(target=self.osc_server.serve_forever)
        self.server_thread.daemon = True
        self.server_thread.start()

    def handle_toggle(self, effect, bypass_value):
        self.last_osc_time = time.time()
        if self.is_sleeping:
            self.wake_up()
        state_text = f"[b]{effect.upper()}[/b]"
        color = (1, 1, 1, 1) if bypass_value == 0 else (0, 0, 0, 1)
        Clock.schedule_once(lambda dt: setattr(self.state_label, 'text', state_text))
        Clock.schedule_once(lambda dt: setattr(self.state_label, 'color', color))
        self.current_effect = effect
        Clock.schedule_once(self.update_sliders)

    def update_param(self, osc_addr, value):
        self.last_osc_time = time.time()
        if self.is_sleeping:
            self.wake_up()
        self.param_values[osc_addr] = value
        if self.current_effect:
            Clock.schedule_once(self.update_sliders)

    def update_sliders(self, dt):
        self.slider_grid.clear_widgets()
        if self.current_effect in self.effect_pages:
            params = self.effect_pages[self.current_effect]
            num_sliders = len(params)
            num_cols = min(4, num_sliders)
            self.slider_grid.cols = num_cols
            slider_width = 400  # Increased for fatter sliders
            spacing = 40
            padding_horizontal = 40
            self.slider_grid.width = num_cols * slider_width + (num_cols - 1) * spacing + padding_horizontal
            self.slider_grid.size_hint_x = None

            num_rows = math.ceil(num_sliders / num_cols)
            slider_box_height = 860
            self.slider_grid.height = num_rows * slider_box_height + (num_rows - 1) * spacing + padding_horizontal
            self.slider_grid.size_hint_y = None

            upper_height_fraction = 0.6
            grid_height_fraction = self.slider_grid.height / Window.height
            y_pos = 0.4 + (upper_height_fraction - grid_height_fraction) / 2
            self.slider_grid.pos_hint = {'center_x': 0.5, 'y': y_pos}

            for name, osc_addr, min_val, max_val in params:
                slider_box = CircularSlider(name, osc_addr, min_val, max_val)
                value = self.param_values.get(osc_addr, min_val)
                slider_box.update_value(value)
                self.slider_grid.add_widget(slider_box)

    def check_sleep(self, dt):
        if time.time() - self.last_osc_time > 10 and not self.is_sleeping:
            self.enter_sleep()

    def enter_sleep(self):
        self.is_sleeping = True
        self.root.remove_widget(self.lava_effect)
        self.root.remove_widget(self.state_label)
        self.root.remove_widget(self.slider_grid)
        self.root.add_widget(self.neutral_screen)
        self.neutral_screen.start_face_switching()

    def wake_up(self):
        self.is_sleeping = False
        self.neutral_screen.stop_face_switching()
        self.root.remove_widget(self.neutral_screen)
        self.root.add_widget(self.lava_effect)
        self.root.add_widget(self.state_label)
        self.root.add_widget(self.slider_grid)
        self.update_label_pos()
        if self.current_effect:
            Clock.schedule_once(self.update_sliders)

    def update_state(self, dt):
        self.state_label.texture_update()
        self.update_label_pos()

    def update_label_pos(self, *args):
        self.state_label.size = self.state_label.texture_size
        self.state_label.pos = (Window.width / 2 - self.state_label.width / 2,
                                Window.height - self.state_label.height)
        if hasattr(self, 'lava_effect') and hasattr(self.lava_effect, 'divider') and not self.is_sleeping:
            self.lava_effect.canvas.after.remove(self.lava_effect.divider)
            with self.lava_effect.canvas.after:
                Color(1, 1, 1, 1)
                self.lava_effect.divider = Line(points=[0, Window.height - self.state_label.height - (Window.height * 0.1),
                                                       Window.width, Window.height - self.state_label.height - (Window.height * 0.1)], width=1)

    def on_stop(self):
        if hasattr(self, 'cs_process'):
            self.cs_process.terminate()
            self.cs_process.wait()
        if hasattr(self, 'osc_server'):
            self.osc_server.shutdown()


if __name__ == "__main__":
    try:
        LavaLampApp().run()
    except KeyboardInterrupt:
        print("Program interrupted.")